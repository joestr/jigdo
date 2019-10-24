/* $Id: job-download.cc,v 1.5 2002/04/15 15:51:55 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  HTTP or FTP retrievals - GUI-related code

*/

#include <glib.h>
#include <gtk-error.hh>
#include <job-download.hh>
#include <string.hh>

#include <iostream>
namespace std { }
using namespace std;
//______________________________________________________________________

DownloadJob::DownloadJob(const string& uri, const string& dest)
    : download(uri, this), destination(dest), currentSizeVal(0),
      dataSizeVal(0), currSlot(0), calcSlot(0),
      currSlotLeft(SPEED_TICK_INTERVAL),
      prevTimeLeft(-TIME_LEFT_GROW_THRESHOLD) {
  for (int i = 0; i < SPEED_SLOTS; ++i) slotStart[i].tv_sec = 0;
  slotSizeVal[0] = 0;
  startTime.tv_sec = 0;
}
//________________________________________

/* Note: We do not destroy the widgets, this will be done
   automatically once the row is removed from the GtkCList. */
DownloadJob::~DownloadJob() {
  // cerr << "~DownloadJob"<<endl;
  if (jobVector()->isWindowOwner(this)) jobVector()->setWindowOwner(0);
}
//______________________________________________________________________

namespace {

  /* Returns true if last characters of uri are the same as ext.
     Cannot use string::compare because of incompatibility between
     GCC2.95/3.0 */
  inline bool compareEnd(const string& uri, const char* ext) {
    static const unsigned extLen = strlen(ext);
    if (uri.size() < extLen) return false;
    unsigned pos = uri.size() - extLen;
    for (unsigned i = 0; i < extLen; ++i)
      if (ext[i] != uri[pos + i]) return false;
    return true;
  }

}
//________________________________________

// Create widgets for a download Job
void DownloadJob::run() {
  Paranoid(getHandler() == 0);

  struct stat fileInfo;
  int statResult = stat(destination.c_str(), &fileInfo);

  // destination is present, but error accessing it
  if (statResult != 0 && errno != ENOENT) {
    string err = subst(_("Cannot access `%1': %2"), destination,
                       strerror(errno));
    errorBox(err);
    jobVector()->erase(row()); // Causes "delete this;"
    return;
  }

  /* We perform a regular download if the destination is either a
     filename (as opposed to a dir name), or the URI is not a jigdo
     file. */
  if ((statResult != 0 && errno == ENOENT) // destination does not exist
      || (statResult == 0 && !S_ISDIR(fileInfo.st_mode)) // exists as file
      || !compareEnd(download.uri(), ".jigdo")) { // URI end not ".jigdo"
    // Show URL as object name
    gtk_clist_set_text(list(), row(), JobVector::COLUMN_PROGRESS,
                       _("Waiting"));
    GtkPixmap* icon = pixmapForURI(download.uri());
    GdkPixmap* pixmap;
    GdkBitmap* mask;
    gtk_pixmap_get(icon, &pixmap, &mask);
    gtk_clist_set_pixtext(list(), row(), JobVector::COLUMN_OBJECT,
                          download.uri().c_str(), JobVector::PIXMAP_SPACING,
                          pixmap, mask);
    /* Register showProgress *before* run(), because the download
       might finish immediately and callRegularly(somethingElse) */
    callRegularly(&DownloadJob::showProgress);
    download.run();
  } else {
    errorBox("Processing of .jigdo files not yet implemented");
    jobVector()->erase(row()); // Causes "delete this;"
    callRegularly(&DownloadJob::showProgress);
  }

}
//______________________________________________________________________

namespace {

  // Print something like "9999", "9999k", "9999M", "99.9M"
  void printSize(string& s, uint64 size) {
    if (size < 10000) {
      append(s, size).append(_("B"));
      return;
    }
    if (size < 1024 * 100) {
      append(s, static_cast<double>(size) / 1024.0).append(_("kB"));
      return;
    }
    if (size < 1024 * 10000) {
      append(s, size / 1024).append(_("kB"));
      return;
    }
    if (size < 1048576 * 100) {
      append(s, static_cast<double>(size) / 1048576.0).append(_("MB"));
      return;
    }
    append(s, size / 1048576).append(_("MB"));
    return;
  }

}
//________________________________________

void DownloadJob::dataSize(uint64 n) { dataSizeVal = n; }
//______________________________________________________________________

/* The way the speed of the connection is measured is quite
   complicated, because it attempts to do two things equally well: 1)
   Show the *average* throughput accurately even on bursty
   connections, 2) React quickly when data suddenly stops flowing. */
void DownloadJob::speedTick(const GTimeVal& now) {
  currSlotLeft -= JobVector::TICK_INTERVAL;
  if (currSlotLeft <= 0) { // New slot starts now
    /* Compare the throughput of the slot that was just finished (at
       [currSlot]) with the next older one. If speed between these two
       differs too much, set calcSlot=currSlot, i.e. from now on
       calculate speed from youngest slot and ignore older slots. */
    int oldSlot = currSlot - 1;
    if (oldSlot < 0) oldSlot = SPEED_SLOTS - 1;
    if (slotStart[oldSlot].tv_sec != 0) {
      uint64 slotSize = currentSizeVal - slotSizeVal[currSlot];
      uint64 oldSlotSize = slotSizeVal[currSlot] - slotSizeVal[oldSlot];
      uint64 sizeChange = 100 * slotSize / (oldSlotSize + 1);
      //cerr << "sizeChange=" << sizeChange << " slotSize=" << slotSize
      //     << " oldSlotSize=" << oldSlotSize << endl;
      if (sizeChange >= SPEED_MAX_GROW || sizeChange <= SPEED_MIN_SHRINK)
        calcSlot = currSlot;
    }

    // Increase currSlot, overwriting oldest slot
    //cerr << "currSlot=" << currSlot << " oldSlot=" << oldSlot
    //     << " calcSlot=" << calcSlot << endl;
    currSlotLeft += SPEED_TICK_INTERVAL;
    if (++currSlot == SPEED_SLOTS) currSlot = 0;
    // Init new slot's entries
    slotSizeVal[currSlot] = currentSizeVal;
    slotStart[currSlot] = now;

    // If calcSlot was oldest slot, increase it so it's the oldest again
    if (currSlot == calcSlot)
      if (++calcSlot == SPEED_SLOTS) calcSlot = 0;
  }
}
//________________________________________

int DownloadJob::speed(const GTimeVal& now) {
  if (slotStart[calcSlot].tv_sec == 0) return -1;
  double elapsed = (now.tv_sec - slotStart[calcSlot].tv_sec) * 1000000.0;
  if (now.tv_usec >= slotStart[calcSlot].tv_usec)
    elapsed += now.tv_usec - slotStart[calcSlot].tv_usec;
  else
    elapsed -= slotStart[calcSlot].tv_usec - now.tv_usec;
  int speed = static_cast<int>(
      static_cast<double>(currentSizeVal - slotSizeVal[calcSlot])
      / elapsed * 1000000.0);
  return speed;
}
//________________________________________

int DownloadJob::timeLeft(const GTimeVal& now) {
  int oldestSlot = currSlot + 1;
  if (oldestSlot == SPEED_SLOTS) oldestSlot = 0;
  if (slotStart[oldestSlot].tv_sec == 0 || dataSizeVal <= currentSizeVal)
    return -1;

  double elapsed = (now.tv_sec - slotStart[oldestSlot].tv_sec)
                   * 1000000.0;
  if (elapsed == 0.0) return -1;
  if (now.tv_usec >= slotStart[oldestSlot].tv_usec)
    elapsed += now.tv_usec - slotStart[oldestSlot].tv_usec;
  else
    elapsed -= slotStart[oldestSlot].tv_usec - now.tv_usec;
  double speed = (currentSizeVal - slotSizeVal[oldestSlot])
                 / elapsed * 1000000.0;
  if (speed == 0.0) return -1;
  int remaining = static_cast<uint32>(
                  (dataSizeVal - currentSizeVal) / speed);
  /* Avoid sudden and frequent jumps in the reported time by not
     reporting a change if the newly calculated remaining time is a
     little higher than the previous one. */
  if (remaining >= prevTimeLeft
      && remaining < prevTimeLeft + TIME_LEFT_GROW_THRESHOLD)
    return prevTimeLeft;
  prevTimeLeft = remaining;
  return remaining;
}
//______________________________________________________________________

// Show progress info.

void DownloadJob::showProgress() {
  if (currentSizeVal == 0) return;
  GdkPixmap* progress = JobVector::progress(currentSizeVal, dataSizeVal);
  GTimeVal now;
  g_get_current_time(&now);

  /** Calculation of estimated time of arrival must take place before
      oldest slot is overwritten by speedTick(). */
  int remaining = timeLeft(now);

  // Append "50kB" if size not known, else "50kB of 10MB"
  string s;
  if (dataSizeVal <= currentSizeVal) {
    dataSizeVal = currentSizeVal;
    printSize(s, currentSizeVal);
  } else {
    //append(s, 100.0 * currentSizeVal / dataSizeVal).append("%, ");
    printSize(s, currentSizeVal);
    s += _(" of ");
    printSize(s, dataSizeVal);
  }
  //____________________

  // Append "10kB/s"
  speedTick(now);
  int curSpeed = speed(now);
  string speedStr;
  if (curSpeed >= 0) {
    printSize(speedStr, curSpeed);
    s += ", ";
    s += speedStr;
    s += _("/s");
  }

  gtk_clist_set_pixtext(list(), row(), JobVector::COLUMN_PROGRESS,
                        s.c_str(), JobVector::PIXMAP_SPACING, progress, 0);
  //____________________

  if (jobVector()->isWindowOwner(this)) {
    // Percentage/kBytes/bytes done in main window
    s.erase();
    if (dataSizeVal <= currentSizeVal) {
      dataSizeVal = currentSizeVal;
      printSize(s, currentSizeVal);
    } else {
      append(s, 100.0 * currentSizeVal / dataSizeVal).append("%, ");
      printSize(s, currentSizeVal);
      s += _(" of ");
      printSize(s, dataSizeVal);
    }
    if (dataSizeVal > 0) {
      s += " (";
      append(s, currentSizeVal);
      s += _(" of ");
      append(s, dataSizeVal);
      s += _(" bytes)");
    }
    gtk_label_set_text(GTK_LABEL(GUI::window.download_progress), s.c_str());

    // Speed/ETA in main window
    s.erase();
    if (remaining >= 0) {
      const int BUF_LEN = 40;
      char buf[BUF_LEN];
      int hr = remaining / 3600;
      snprintf(buf, BUF_LEN, _("%02d:%02d:%02d remaining"),
               hr,  remaining / 60 - hr * 60, remaining % 60);
      buf[BUF_LEN - 1] = '\0';
      s += buf;
    }
    if (curSpeed >= 0) {
      if (!s.empty()) s += ", ";
      s += speedStr;
      s += _("/sec");
    }
    gtk_label_set_text(GTK_LABEL(GUI::window.download_speed), s.c_str());

  } // endif (jobVector()->isWindowOwner(this))
}
//________________________________________

void DownloadJob::newData(const byte* data, size_t size,
                          uint64 currentSize) {
  // Update status info
  if (startTime.tv_sec == 0) {
    g_get_current_time(&startTime);
    // Vars must be in state left by ctor
    Paranoid(currSlot == 0 && slotStart[SPEED_SLOTS - 1].tv_sec == 0
             && slotSizeVal[0] == 0 && startTime.tv_sec == 0);
    g_get_current_time(&slotStart[0]);
  }
  currentSizeVal = currentSize;

  if (false) {
    cerr << "Got " << size << " bytes ["<<currentSizeVal<<"]: ";
    if (size > 80) size = 80;
    while (size > 0 && (*data >= ' ' && *data < 127 || *data >= 160))
      cerr << *data++;
    cerr << endl;
  }
}
//________________________________________

void DownloadJob::finished() {
  string size;
  printSize(size, currentSizeVal);
  string s = subst(_("Finished - fetched %1"), size);
  gtk_clist_set_text(list(), row(), JobVector::COLUMN_PROGRESS, s.c_str());
  callRegularlyLater(JobVector::FINISHED_WAIT, &DownloadJob::exit);
}
//________________________________________

void DownloadJob::info(const string& message) {
  gtk_clist_set_text(list(), row(), JobVector::COLUMN_PROGRESS,
                     message.c_str());
}
//________________________________________

void DownloadJob::error(const string& message) {
  gtk_clist_set_text(list(), row(), JobVector::COLUMN_PROGRESS,
                     message.c_str());
}
//________________________________________

void DownloadJob::exit() {
  // Delete ourself from the list
  jobVector()->erase(row());
}
//______________________________________________________________________

void DownloadJob::selectRow(int, GdkEventButton*) {
  bool ownedByMe = jobVector()->isWindowOwner(this);
  setNotebookPage(GUI::window.pageDownload);
  jobVector()->setWindowOwner(this);
  if (!ownedByMe) updateWindow();
}
//________________________________________

void DownloadJob::updateWindow() {
  gtk_label_set_text(GTK_LABEL(GUI::window.download_URL),
                     download.uri().c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.download_dest),
                     destination.c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.download_progress), "");
  gtk_label_set_text(GTK_LABEL(GUI::window.download_speed), "");
}
