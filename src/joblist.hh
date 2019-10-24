/* $Id: joblist.hh,v 1.18 2002/04/06 23:20:53 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Interface to the GtkCList of running jobs (downloads etc),
  GUI::window.jobs, i.e. the list at the bottom of the jigdo window.

*/

#ifndef JOBLIST_HH
#define JOBLIST_HH

#include <string>
#include <vector>
namespace std { }
using namespace std;

#include <debug.hh>
#include <download.hh>
#include <gtk-gui.hh>
//______________________________________________________________________

class JobVector;

/** One "job", e.g. file download, scanning through files, or image
    download (which has many file downloads as children). */
class Job {
  friend class JobVector; // Overwrites jobVec and rowNrVal on insert()
public:
  inline Job();
  /// If the Job was in a needTicks() state, it is unregistered by ~Job
  virtual ~Job();
  /** Start/restart this job. When called, must also create GUI
      elements for this Job in row number row() of GtkCList list(). Is
      called after Job is added to the list, and when it is restarted
      (if at all). Child classes should provide an implementation. */
  virtual void run() = 0;
  /** Called when the Job's line is selected in the list. Default
      version does nothing. */
  virtual void selectRow(int column, GdkEventButton* event);

  /// Return appropriate pixmap for object of that name
  static GtkPixmap* pixmapForURI(const string& uri);

protected:
  typedef void (Job::*tickHandler)();
  /// Pointer to JobVector
  inline JobVector* jobVector() const;
  /// List to use when updating the GUI elements
  inline GtkCList* list() const;
  /// Row number in list. May change when empty entries collapse
  inline size_t row() const;

  /** Register a tick handler. The handler will be called every
      JobVector::TICK_INTERVAL milliseconds by a GTK+ callback
      function that JobVector registers. Handy for updating progress
      reports at regular intervals.
      Only register a handler if you really need it. Otherwise, pass 0
      as the handler function pointer - if no Job at all registers a
      tick handler, the callback fnc will not be registered at all,
      saving some CPU time. */
  inline void callRegularly(tickHandler handler);
  /// Return current tick handler
  tickHandler getHandler() const { return tick; }
  /// Does this object need to be called regularly?
  bool needTicks() const { return tick != 0; }
  /** Wait appropriate nr of ticks, then register the supplied
      handler. Effectively, this means the Job pauses for a while -
      e.g. so the user can read some status report. */
  inline void callRegularlyLater(const int milliSec, tickHandler handler);

private:
  void waitTick();
  int waitCountdown;
  tickHandler waitDestination;

  JobVector* jobVec;
  size_t rowVal;
  tickHandler tick;
};
//______________________________________________________________________

/** A bit like a vector<Job*>, but uses the GtkCList for storing the
    elements. Big difference from vector<>: When deleting elements,
    the elements after the deleted one are not copied by one position
    immediately. Instead, such copying only happens when empty entries
    collapse (=are "garbage-collected", which happens at regular
    intervals). Reason: User should not be annoyed by rows becoming
    non-selectable because they move around the list so quickly. OTOH,
    insertion happens immediately.

    ~JobVector deletes all Job objects in the list. */
class JobVector {
public:
  // Assumed number of columns in job display (progress bar, URL)
  static const int COLUMN_PROGRESS = 0;
  static const int COLUMN_OBJECT = 1;
  static const int NR_OF_COLUMNS = 2;
  static const int PIXMAP_SPACING = 4; // In pixmap+text GtkCList cells
  /* Time values are in milliseconds. All values should be multiples
     of TICK_INTERVAL */
  static const int TICK_INTERVAL = 250; // = progress report update interval
  static const int COLLAPSE_INTERVAL = 1000;
  static const int FINISHED_WAIT = 5000;
  static const int ERROR_WAIT = 10000;

  typedef unsigned size_type;
  inline JobVector();
  /// Any Jobs still in the list are deleted
  ~JobVector();

  /** Size *includes* elements that have been delete()d, but that have
      not yet collapsed away. */
  inline size_type size() const;
  /// Number of non-deleted entries (always <= size())
  inline size_type entryCount() const;
  inline bool empty() const { return size() == 0; }

  /// Use this instead of operator[]
  inline Job* get(size_type n);
  /// Simply overwrites pointer, will not delete the old entry
  inline void set(size_type n, Job* j);
  /** Makes row blank so that it will eventually be collapsed away.
      The Job object pointed to by the entry is deleted. Regular
      callbacks for the collapse are scheduled unless they are already
      pending. */
  void erase(size_type n);
  /** Insert new Job before position n. Calls j->run() so the Job can
      visualize itself. */
  inline void insert(size_type n, Job* j);
  /** Append new Job at end of list. Calls j->run() so the Job can
      visualize itself. */
  inline void append(Job* j);
  /** Make row blank by setting text labels to "". Sets data pointer
      to 0, does *not* delete Job object of that row. */
  inline void makeRowBlank(size_type n);

  void freeze() { gtk_clist_freeze(list()); }
  void thaw() { gtk_clist_thaw(list()); }

  /** Set a static var of the JobVector class to the supplied value.
      The Job that is currently selected and is in charge of updating
      the main window (e.g. with progress info) calls this with
      j==this, and subsequently uses isWindowOwner(this) to check
      whether it is still in charge of updating the window. This way,
      it is ensured that only one Job at a time updates the window.
      Supply 0 to unset. */
  inline void setWindowOwner(Job* j);
  /** Check whether the supplied Job is in charge of updating the
      window. */
  inline bool isWindowOwner(Job* j) const;

  /** (De)register a Job whose tick() method should be called
      regularly. As soon as there is at least one such Job, a GTK
      timeout function is registered which does a freeze(), then scans
      through the whole list calling objects' tick handler where
      present, then thaw()s the list. As soon as the count of
      tick-needing Jobs reaches 0, the timeout function is
      unregistered again. */
  inline void registerTicks();
  inline void unregisterTicks();

  /** To be called sometime after gtk_init, but before any
      Job(Vector)s are used. */
  static void initAfterGtk();

  // Return pointer to appropriate "x% done" pixmap
  static GdkPixmap* progress(uint64 curAmount, uint64 totalAmount);

  // We save the room for the pointer, since it always points to window.jobs
  // GtkCList* listVal;
  GtkCList* list() const { return GTK_CLIST(GUI::window.jobs); }

private:
  // GTK+ timeout function: calls tick(), collapses empty rows
  static gint timeoutCallback(gpointer jobVector);
  // GTK+ callback function, called when a line in the list is selected
  static void selectRowCallback(GtkWidget* widget, gint row, gint column,
                                GdkEventButton* event, gpointer data);

  /* Used by initAfterGtk(): Nr of pixmaps to subdivide the progress
     XPM into, filename to load from. */
  static const int PROGRESS_SUBDIV = 51;
  static const char* const PROGRESS_XPM = "progress051.xpm";
  static vector<GdkPixmap*> progressGfx;
  static gchar* noText[]; // For init'ing CList row

  unsigned entryCountVal;
  Job* windowOwner;

  /* Count the number of entries in the list which are in a state in
     which they need tick() calls. */
  int needTicks;
  /* While entryCount() < size(), contains nr of ticks left before
     next collapse of empty rows. */
  int collapseCountdown;
};
//______________________________________________________________________

/// Global list of running jobs
namespace GUI {
  extern JobVector jobVector;
}

//======================================================================

Job::Job() : jobVec(0), tick(0) { }
JobVector* Job::jobVector() const { return jobVec; }
GtkCList* Job::list() const { return jobVec->list(); }
size_t Job::row() const { return rowVal; }
//________________________________________

void Job::callRegularly(tickHandler handler) {
  if (!needTicks() && handler != 0)
    jobVector()->registerTicks();
  else if (needTicks() && handler == 0)
    jobVector()->unregisterTicks();
  tick = handler;
}

void Job::callRegularlyLater(const int milliSec, tickHandler handler) {
  waitCountdown = milliSec / JobVector::TICK_INTERVAL;
  waitDestination = handler;
  callRegularly(&Job::waitTick);
}
//________________________________________

JobVector::JobVector() : entryCountVal(0), windowOwner(0), needTicks(0),
                         collapseCountdown(0) {
  // Mustn't access GtkCList here because GTK+ is not initialized yet!
}

JobVector::size_type JobVector::size() const { return list()->rows; }
JobVector::size_type JobVector::entryCount() const { return entryCountVal; }

Job* JobVector::get(size_type n) {
  return static_cast<Job*>(gtk_clist_get_row_data(list(), n));
}
void JobVector::set(size_type n, Job* j) {
  gtk_clist_unselect_row(list(), n, 0);
  gtk_clist_set_row_data(list(), n, j);
  if (j) {
    j->jobVec = this;
    j->rowVal = n;
  }
}
void JobVector::insert(size_type n, Job* j) {
  Paranoid(j != 0);
  gtk_clist_insert(list(), n, noText); // 0 => no text initially
  gtk_clist_set_row_data(list(), n, j);
  ++entryCountVal;
  j->jobVec = this;
  j->rowVal = n;
  j->run();
}
void JobVector::append(Job* j) {
  Paranoid(j != 0);
  size_type n = gtk_clist_append(list(), noText); // 0 => no text initially
  gtk_clist_set_row_data(list(), n, j);
  ++entryCountVal;
  j->jobVec = this;
  j->rowVal = n;
  j->run();
}

#ifndef DEBUG_JOBLIST
#  define DEBUG_JOBLIST 0
#endif

void JobVector::registerTicks() {
  if (++needTicks == 1)
    gtk_timeout_add(TICK_INTERVAL, timeoutCallback, this);
# if DEBUG_JOBLIST
  cerr << "registerTicks: " << needTicks << endl;
# endif
}
/* No further action is required. The timeout function will unregister
   itself next time it is called, by returning FALSE. */
void JobVector::unregisterTicks() {
  --needTicks;
# if DEBUG_JOBLIST
  cerr << "unregisterTicks: " << needTicks << endl;
# endif
}

void JobVector::makeRowBlank(size_type n) {
  Paranoid(NR_OF_COLUMNS == 2);
  gtk_clist_set_text(list(), n, 0, "");
  gtk_clist_set_text(list(), n, 1, "");
  gtk_clist_set_row_data(list(), n, 0);
  gtk_clist_unselect_row(list(), n, 0);
}

void JobVector::setWindowOwner(Job* j) { windowOwner = j; }
bool JobVector::isWindowOwner(Job* j) const { return windowOwner == j; }

#endif
