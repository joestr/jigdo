/* $Id: download.hh,v 1.15 2002/03/31 21:45:50 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download data from URL, write to output function, report on progress

*/

#ifndef DOWNLOAD_HH
#define DOWNLOAD_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

#include <string>
namespace std { }
using namespace std;

//#include <download.fh>
#include <libwww.hh>
//______________________________________________________________________

/** Class containing the state of a download. libwww must be properly
    initialised before usage (this happens in glibwww-init.cc). */
class Download {
public:
  /// Class for outputting progress reports and data
  class Output;
  Download(const string& uri, Output* o /*= 0*/);
  ~Download();
  /** Start downloading. Returns (almost) immediately and runs the
      download in the background via the glib/libwww event loop. */
  void run();
  inline const string& uri() const;
  //inline void setOutput(Output* o);

  // Initialize libwww - call this before starting any downloads
  static void init();
  // Function called by libwww for progress reports
  static BOOL alertCallback(HTRequest* request, HTAlertOpcode op, int msgnum,
                            const char* dfault, void* input,
                            HTAlertPar* reply);
  // Function called by libwww on error
  static int afterFilter(HTRequest* request, HTResponse* response,
                         void* param, int status);

private:
  // libwww HTStream interface
  static int flush(HTStream* me);
  static int free(HTStream* me);
  static int abort(HTStream* me, HTList* e);
  static int putChar(HTStream* me, char c);
  static int putString(HTStream* me, const char* s);
  static int write(HTStream* me, const char* s, int l);
  // Function called by libwww, just before any alertCallback
  static int requestCallback(HTRequest* request, void* downloadObj);

  /* In order for this object to be a libwww stream, we need to supply
     a "vtbl pointer" for HTStream AS THE FIRST DATA MEMBER. */
  const HTStreamClass* vptr;
  string uriVal;
  uint64 currentSize;
  Output* output;
  HTRequest* request;
};
//______________________________________________________________________

/** A derived class of Output must be supplied by the user of
    Download, to take care of the downloaded data and to output progress
    reports. */
class Download::Output {
public:
  virtual ~Output() { }
  /** Called as soon as the size of the downloaded data is known. May
      not be called at all if the size is unknown. */
  virtual void dataSize(uint64) { }
  /** Called during download whenever data arrives, with the data that
      just arrived. You can write the data to a file, copy it away
      etc. currentSize is the offset into the downloaded data (including
      the "size" new bytes) - useful for "x% done" messages. */
  virtual void newData(const byte* data, size_t size, uint64 currentSize) =0;
  /// Called when download is complete
  virtual void finished() = 0;
  /** Short progress message ("Looking up host", "Connecting" etc). As
      a special case, an empty string indicates that the download
      proper has begun. */
  virtual void info(const string& /*message*/) { }
  /** Short progress message ("Timeout", "404 not found") which means
      that the download has failed. */
  virtual void error(const string& /*message*/) { }
};
//______________________________________________________________________

const string& Download::uri() const { return uriVal; }

//void Download::setOutput(Output* o) { output = o; }

#ifndef NOINLINE
//#  include <download.ih> /* NOINLINE */
#endif

#endif
