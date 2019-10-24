/* $Id: job-download.hh,v 1.2 2002/03/31 00:00:36 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  HTTP or FTP retrievals - GUI-related code

  More low-level code handling downloads independent of GUI is in
  download.cc

*/

#ifndef JOB_DOWNLOAD_HH
#define JOB_DOWNLOAD_HH

#include <joblist.hh>
//______________________________________________________________________

/// Job: Download of a file.
class DownloadJob : public Job, private Download::Output {
public:
  DownloadJob(const string& uri, const string& dest);
  /// Virtual methods from Job
  virtual ~DownloadJob();
  virtual void run();
  virtual void selectRow(int column, GdkEventButton* event);
  /** Update window with info about ourselves. This only sets the
      constant parts (URL etc.), the progress reports are set from
      showProgress(). */
  void updateWindow();
  /// Return estimated bytes/sec for (roughly) last few secs (-1 for unknown)
  int speed(const GTimeVal& now);
  /// Return estimated remaining time in seconds (-1 for unknown)
  int timeLeft(const GTimeVal& now);

protected:
  typedef void (DownloadJob::*tickHandler)();
  inline void callRegularly(tickHandler handler);
  inline void callRegularlyLater(const int milliSec, tickHandler handler);

private:
  // Virtual methods from Download::Output
  virtual void dataSize(uint64 n);
  virtual void newData(const byte* data, size_t size, uint64 currentSize);
  virtual void finished();
  virtual void info(const string& message);
  virtual void error(const string& message);

  // Methods registered with callRegularly(Later), and called each tick
  void showProgress();
  // Called by showProgress for speed calculation
  void speedTick(const GTimeVal& now);

  // erase() ourself from the JobVector => delete this
  void exit();

  Download download;
  string destination;
  uint64 currentSizeVal;
  uint64 dataSizeVal;

  // Estimation of download speed for "x kBytes per sec" display
  static const int SPEED_TICK_INTERVAL = 3000;
  static const int SPEED_SLOTS = 10;
  // If speed grows by >=x% from newer slot to older, ignores older slots
  static const unsigned SPEED_MAX_GROW = 160;
  static const unsigned SPEED_MIN_SHRINK = 40;
  uint64 slotSizeVal[SPEED_SLOTS]; // Value of currentSizeVal at slot start
  GTimeVal slotStart[SPEED_SLOTS]; // Timestamp of start of slot
  int currSlot; // Rotates through 0..SPEED_SLOTS-1
  int calcSlot; // Index of slot which is used for speed calculation
  int currSlotLeft; // Millisecs before a new slot is started
  /* Secs that timeLeft() must have *increased* by before the new
     value is reported. A *drop* of timeLeft() values is reported
     immediately. */
  static const int TIME_LEFT_GROW_THRESHOLD = 5;
  int prevTimeLeft;

  GTimeVal startTime;
};

//======================================================================

/* The static_cast from DownloadJob::* to Job::* (i.e. member fnc of
   base class) is OK because we know for certain that the handler will
   only be invoked on DownloadJob objects. */
void DownloadJob::callRegularly(tickHandler handler) {
  Job::callRegularly(static_cast<Job::tickHandler>(handler));
}
void DownloadJob::callRegularlyLater(const int milliSec,
                                     tickHandler handler) {
  Job::callRegularlyLater(milliSec, static_cast<Job::tickHandler>(handler));
}

#endif
