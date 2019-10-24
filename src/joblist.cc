/* $Id: joblist.cc,v 1.23 2002/04/06 23:20:53 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Interface to the GtkCList of running jobs (downloads etc), window.jobs

*/

#include <iomanip>
#include <iostream>
namespace std { }
using namespace std;
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <config.h>
#if WINDOWS
#  include <gdkprivate.h>
#endif

#include <gtk-support.hh>
#include <joblist.hh>
#include <string.hh>
//______________________________________________________________________

JobVector GUI::jobVector;

gchar* JobVector::noText[] = { 0, 0 }; // For init'ing CList row
vector<GdkPixmap*> JobVector::progressGfx;
//______________________________________________________________________

Job::~Job() {
  if (needTicks()) jobVector()->unregisterTicks();
}
//______________________________________________________________________

void Job::waitTick() {
  if (--waitCountdown == 0) callRegularly(waitDestination);
# if DEBUG_JOBLIST
    cerr << "waitCountdown=" << waitCountdown << endl;
# endif
}
//______________________________________________________________________

void Job::selectRow(int, GdkEventButton*) { }
//______________________________________________________________________

// Return a pixmap appropriate for the URI. Loads the gfx from file on demand
GtkPixmap* Job::pixmapForURI(const string& uri) {
  enum { DEFAULT, deb, jigdo, iso, LAST };
  static GtkPixmap* icon[LAST]; // Initialized to 0

  struct Mapping { unsigned extLen; const char* ext; int icon; };
  const Mapping map[] = {
    { 4, ".deb",      deb },
    { 4, ".dsc",      deb },
    { 6, ".jigdo",    jigdo },
    { 9, ".template", jigdo },
    { 4, ".iso",      iso },
    { 4, ".raw",      iso }
  };
  //____________________

  // Search through the map entries
  int type = DEFAULT;
  string::const_iterator end = uri.end();
  for (size_t m = 0; m < sizeof(map) / sizeof(Mapping); ++m) {
    // extLen field for each entry must be length of ext string
    const char* ext = map[m].ext;
    Paranoid(map[m].extLen == strlen(ext));
    if (map[m].extLen > uri.size()) continue;
    string::const_iterator i = end - map[m].extLen;
    while (*ext != '\0' && *i == *ext) { ++i; ++ext; }
    if (*ext == '\0') { type = map[m].icon; break; }
  }
  GtkPixmap* result = icon[type];
  if (result == 0) {
    const char* filename = 0;
    switch (type) {
      case deb:     filename = "job-deb.xpm"; break;
      case jigdo:   filename = "job-jigdo.xpm"; break;
      case iso:     filename = "job-iso.xpm"; break;
      case DEFAULT: filename = "job-file.xpm"; break;
    }
#   if DEBUG
    //cerr << "Loading `" << filename << '\'' << endl;
#   endif
    result = GTK_PIXMAP(create_pixmap(GUI::window.window, filename));
    icon[type] = result;
  }
  return result;
}
//======================================================================

JobVector::~JobVector() {
  /* Don't do gtk_clist_clear() - the CList is auto-deleted because it
     is the child of a deleted window. */

  // Delete Jobs
  size_type rows = size();
  for (size_type row = 0; row < rows; ++row) delete get(row);
}
//______________________________________________________________________

/* Create progress pixmaps from the big "progress051.xpm" file. The
   reason why we have to resort to pixmaps and not use a
   GtkProgressBar is that GtkCList does not appear to allow you to
   insert an arbitrary widget into a CList, just pixmaps and text.
   Grrr.
   Called from gtk-gui.cc */
void JobVector::initAfterGtk() {
  GtkCList* self = GUI::jobVector.list();
  Assert(self->columns == 2);
  Assert(GUI::jobVector.empty());
  Paranoid(sizeof(noText) == NR_OF_COLUMNS * sizeof(gchar*));
  //____________________

  // Register callback
  gtk_signal_connect(GTK_OBJECT(self), "select_row",
                     GTK_SIGNAL_FUNC(JobVector::selectRowCallback),
                     GTK_OBJECT(self));
  gtk_signal_connect(GTK_OBJECT(self), "unselect_row",
                     GTK_SIGNAL_FUNC(JobVector::selectRowCallback),
                     GTK_OBJECT(self));

  // Column titles do not act as buttons
  gtk_clist_column_titles_passive(self);
  //____________________

  GtkWidget* progress = create_pixmap(GUI::window.window, PROGRESS_XPM);
  GdkPixmap* progressImage;
  gint width = -1, height = -1, depth = -1;

  gtk_pixmap_get(GTK_PIXMAP(progress), &progressImage, NULL);
# if WINDOWS
  // This is probably the wrong way to get the width/height...
  width = ((GdkDrawablePrivate*)progressImage)->width;
  height = ((GdkDrawablePrivate*)progressImage)->height;
  depth = 24;
# else
  /* It is not clearly documented whether casting from GdkPixmap* to
     GdkWindow* in this way is OK... :-/ */
  gdk_window_get_geometry((GdkWindow*)progressImage, 0, 0,
                          &width, &height, &depth);
  Assert(width != -1 && height != -1 && depth != -1);
# endif
// # if DEBUG
//   cerr << PROGRESS_XPM << ": " << width << " x " << height << " x "
//        << depth << endl;
// # endif
  GdkGC* gc = gdk_gc_new((GdkWindow*)progressImage);
  Paranoid(gc != 0);
  //____________________

  /* Split up progressImage into the many smaller pixmaps for the
     graphical progress bar. These pixmaps will never be de-allocated
     again. */
  Paranoid(progressGfx.empty());
  progressGfx.reserve(PROGRESS_SUBDIV);
  if (width == 1 && height == 1) {
    // progressGfx not found - fill array with dummy pixmaps
    for (int i = 0; i < PROGRESS_SUBDIV; ++i)
      progressGfx.push_back(progressImage);
  } else {
    int sheight = height / PROGRESS_SUBDIV; // Height of small pixmap
    for (int i = 0; i < height; i += sheight) {
      // Create new pixmap and add to list
      GdkPixmap* pic = gdk_pixmap_new(NULL, width, sheight, depth);
      Paranoid(pic != 0);
      progressGfx.push_back(pic);
      gdk_draw_pixmap((GdkDrawable*)pic, gc, progressImage,
                      0, i, 0, 0, width, sheight);
    }
  }
  //____________________

  gdk_gc_destroy(gc);
  gtk_widget_destroy(progress);
}
//______________________________________________________________________

GdkPixmap* JobVector::progress(uint64 curAmount, uint64 totalAmount) {
  if (totalAmount == 0) return progressGfx[0];
  unsigned nr = PROGRESS_SUBDIV * curAmount / totalAmount;
  if (nr >= progressGfx.size()) nr = progressGfx.size() - 1;
  return progressGfx[nr];
}
//______________________________________________________________________

// void JobVector::deleteJob(void* j) {
//   // cerr << "delete Job "<<j<<endl;
//   delete static_cast<Job*>(j);
// }
//______________________________________________________________________

void JobVector::erase(size_type n) {
  // If we were not already collapsing, ensure timeout fnc is registered now
  if (entryCount() == size()) {
    registerTicks();
    collapseCountdown = COLLAPSE_INTERVAL / TICK_INTERVAL;
  }

  // Delete entry
  Job* j = get(n);
  if (j) {
    Paranoid(entryCountVal > 0);
    --entryCountVal;
    delete j; // Delete!
  }
  makeRowBlank(n);
}
//______________________________________________________________________

namespace {

  void copyCell(GtkCList* clist, gint srcRow, gint destRow, gint column) {
    gchar* text;
    guint8 spacing;
    GdkPixmap* pixmap;
    GdkBitmap* mask;
    switch (gtk_clist_get_cell_type(clist, srcRow, column)) {
    case GTK_CELL_EMPTY:
      gtk_clist_set_text(clist, destRow, column, "");
      break;
    case GTK_CELL_TEXT:
      gtk_clist_get_text(clist, srcRow, column, &text);
      gtk_clist_set_text(clist, destRow, column, text);
      break;
    case GTK_CELL_PIXMAP:
      gtk_clist_get_pixmap(clist, srcRow, column, &pixmap, &mask);
      gtk_clist_set_pixmap(clist, destRow, column, pixmap, mask);
      break;
    case GTK_CELL_PIXTEXT:
      gtk_clist_get_pixtext(clist, srcRow, column, &text, &spacing,
                            &pixmap, &mask);
      gtk_clist_set_pixtext(clist, destRow, column, text, spacing,
                            pixmap, mask);
      break;
    case GTK_CELL_WIDGET:
    default:
      Assert("unsupported");
    }
  }

}
//______________________________________________________________________

gint JobVector::timeoutCallback(gpointer jobVector) {
  JobVector* self = static_cast<JobVector*>(jobVector);
  size_type rows = self->size();

# if DEBUG_JOBLIST
  cerr << "timeoutCallback() collapseCountdown=" << self->collapseCountdown
       << endl;
  size_type realEntryCount = 0;
  for (size_type row = 0; row < rows; ++row)
    if (self->get(row) != 0) ++realEntryCount;
  Paranoid(realEntryCount == self->entryCount());
# endif

  bool collapsing = false;
  if (self->collapseCountdown > 0) {
    --self->collapseCountdown;
    collapsing = self->collapseCountdown == 0;
  }

  /* If there are no needTicks() Jobs, just a collapse operation in
     the future, we may not need to scan through the list at all. */
  if (self->needTicks == 1 && self->collapseCountdown > 0) return TRUE;

  /* Walk through list. If collapseCountdown == 0, remove >=0 rows at
     the end, move other rows. */
  bool prevEmpty = false; // Was previous row empty?
  self->freeze();
  //____________________

  for (size_type row = 0; row < rows; ++row) {
    Job* job = self->get(row);
    // Collapse operation
    if (collapsing && job != 0 && prevEmpty) {
      // Move row by one position towards top of list
      Paranoid(NR_OF_COLUMNS == 2);
      copyCell(self->list(), row, row - 1, COLUMN_PROGRESS);
      copyCell(self->list(), row, row - 1, COLUMN_OBJECT);
      self->set(row - 1, self->get(row));
      self->makeRowBlank(row);
      /* NB we do *not* set prevEmpty=true here - this causes the
         "stripes" of alternating empty/non-empty rows in the list.
         The stripe effect is the Right Thing, because it ensures that
         a click on a row always either selects the row the user
         intended to, or an empty one, *never* the following one! */
    }
    prevEmpty = job == 0;
    if (job != 0) {
      /* NB the Job can delete itself, so "job" becomes invalid after
         tick() is called. */
      // Yargh! This syntax took me 30 minutes to get right:
      if (job->needTicks()) (job->*(job->tick))();
    }
  }
  //____________________

  // If last row is empty, delete it
  if (rows > 0 && self->get(rows - 1) == 0)
    gtk_clist_remove(self->list(), rows - 1);

  self->thaw();
  Paranoid(self->collapseCountdown >= 0);

  if (collapsing) {
    // If all empty rows have been removed, maybe stop callback:
    if (self->entryCount() == self->size())
      self->unregisterTicks();
    else
      self->collapseCountdown = COLLAPSE_INTERVAL / TICK_INTERVAL;
  }
  if (self->needTicks > 0) return TRUE;
# if DEBUG_JOBLIST
  cerr << "timeoutCallback() unregistered\n" << endl;
# endif
  return FALSE;
}
//______________________________________________________________________

/* Called when the user clicks on a line in the job list. Simply
   passes the click on to the object whose line was clicked on, calls
   its selectRow() virtual method. */
void JobVector::selectRowCallback(GtkWidget*, gint row, gint column,
                                  GdkEventButton* event, gpointer data) {
  JobVector* self = static_cast<JobVector*>(data);
  Job* job = self->get(row);
  if (job != 0) job->selectRow(column, event);
}
