 /* $Id: download.cc,v 1.18 2002/04/15 15:49:36 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download data from URL, write to output function, report on progress

*/

#include <stdio.h>
#include <string.h>

#include <config.h>
#include <debug.hh>
#include <download.hh>
//#include <download.ih>
#include <glibwww.hh>
#include <string.hh>

#include <iostream>
namespace std { }
using namespace std;

#ifndef DEBUG_DOWNLOAD
#  define DEBUG_DOWNLOAD (DEBUG && 1)
#endif

#if DEBUG_DOWNLOAD
#  include <stdio.h>
#endif
//______________________________________________________________________

Download::Download(const string& uri, Output* o)
    : uriVal(uri), currentSize(0), output(o), request(0) {
  static const HTStreamClass downloadWriter = {
    "jigdoDownloadWriter", flush, free, abort, putChar, putString, write
  };
  vptr = &downloadWriter;
}

Download::~Download() {
  HTRequest_delete(request);
}
//______________________________________________________________________

namespace {

# if DEBUG_DOWNLOAD
  extern "C"
  int tracer(const char* fmt, va_list args) {
    return vfprintf(stdout, fmt, args);
  }
# endif

  BOOL nonono(HTRequest*, HTAlertOpcode, int, const char*, void*,
              HTAlertPar*) {
    return NO;
  }
}

// Initialize (g)libwww
void Download::init() {
  HTAlertInit();

# if DEBUG_DOWNLOAD && 0
  HTSetTraceMessageMask("flbtspuhox");
  HTTrace_setCallback(tracer);
# endif

  HTNet_setMaxSocket (32);

  /* These calls are necessary for redirections to work. (Why? Don't
     ask why - this is libwww, after all...) */
  HTList* converters = HTList_new();
  HTConverterInit(converters); // Register the default set of converters
  HTFormat_setConversion(converters); // Global converters for all requests

  HTAlert_setInteractive(YES);
  // HTPrint_setCallback(printer);
  glibwww_init("jigdo", JIGDO_VERSION);

  HTAlert_add(Download::alertCallback, HT_A_PROGRESS); // Progress reports
  HTAlert_add(nonono, static_cast<HTAlertOpcode>(
              HT_A_CONFIRM | HT_A_PROMPT | HT_A_SECRET | HT_A_USER_PW));
  // To get notified of errors, redirects etc.
  HTNet_addAfter(Download::afterFilter, NULL /*template*/, 0 /*param*/,
                 HT_ALL, HT_FILTER_MIDDLE);
}
//______________________________________________________________________

namespace {

  inline Download* getDownloadObj(HTRequest* request) {
    return static_cast<Download*>(HTRequest_context(request));
  }
  inline Download* getDownloadObj(HTStream* stream) {
    return reinterpret_cast<Download*>(stream);
  }

}
//________________________________________

void Download::run() {
  Assert(output != 0);

  /* The code below (e.g. in putChar()) silently assumes that the
     first data member's address of a Download object is identical to
     the object's address. The C++ standard makes no guarantee about
     this. :-/ */
  Assert(static_cast<void*>(this) == static_cast<void*>(&vptr));
  HTStream* writer = reinterpret_cast<HTStream*>(this); // Shudder... :-)
  request = HTRequest_new();

  // Store within the HTRequest object a ptr to the corresponding Download
  HTRequest_setContext(request, static_cast<void*>(this));

  HTRequest_setOutputFormat(request, WWW_SOURCE); // Raw data, no headers...
  HTRequest_setOutputStream(request, writer); // is sent to writer
  //HTRequest_setDebugStream(request, NULL); // body different from 200 OK
  HTRequest_setAnchor(request, HTAnchor_findAddress(uriVal.c_str()));


  //   HTRequest_addAfter(request, after_load_to_file, NULL, data,
  //                      HT_ALL, HT_FILTER_LAST, FALSE);

  //   BOOL HTRequest_addRange       (HTRequest * request,
  //                                  char * unit, char * range);

  // TODO error handling
  if (HTLoad(request, NO) == NO)
    HTRequest_delete(request);

  return;
}
//______________________________________________________________________

/* Implementation for the libwww HTStream functionality - forwards the
   calls to the Output object.
   Return codes: HT_WOULD_BLOCK, HT_ERROR, HT_OK, >0 to pass back. */

int Download::flush(HTStream*) { return HT_OK; }
int Download::free(HTStream*) { return HT_OK; }
int Download::abort(HTStream*, HTList*) { return HT_OK; }

int Download::putChar(HTStream* me, char c) {
  Download* self = getDownloadObj(me);
  self->currentSize += 1;
  self->output->newData(reinterpret_cast<const byte*>(&c),
                        1, self->currentSize);
  return HT_OK; //TODO: allow newData to pass back error...
}
int Download::putString(HTStream* me, const char* s) {
  Download* self = getDownloadObj(me);
  size_t len = strlen(s);
  self->currentSize += len;
  self->output->newData(reinterpret_cast<const byte*>(s),
                        len, self->currentSize);
  return HT_OK;
}
int Download::write(HTStream* me, const char* s, int l) {
  Download* self = getDownloadObj(me);
  size_t len = static_cast<size_t>(l);
  self->currentSize += len;
  self->output->newData(reinterpret_cast<const byte*>(s),
                        len, self->currentSize);
  return HT_OK;
}
//______________________________________________________________________

// Function which is called by libwww whenever anything happens for a request
BOOL Download::alertCallback(HTRequest* request, HTAlertOpcode op,
                             int /*msgnum*/, const char* /*dfault*/,
                             void* input, HTAlertPar* /*reply*/) {
  if (request == 0) return NO;
  // A Download object hides behind the output stream registered with libwww
  Download* self = getDownloadObj(request);
  char* host = "host";
  if (input != 0) host = static_cast<char*>(input);

  switch (op) {
  case HT_PROG_DNS:
    self->output->info(subst(_("Looking up %1"), host));
    break;
  case HT_PROG_CONNECT:
    self->output->info(subst(_("Contacting %1"), host));
    break;
  case HT_PROG_LOGIN:
    self->output->info(_("Logging in"));
    break;
  case HT_PROG_READ: {
    long len = HTAnchor_length(HTRequest_anchor(request));
    if (len != -1 && static_cast<uint64>(len) != self->currentSize)
      self->output->dataSize(len);
    break;
  }
  case HT_PROG_DONE:
    // self->output->finished(); is done by afterFilter instead
    break;
  case HT_PROG_INTERRUPT:
    self->output->info(_("Interrupted"));
    break;
  case HT_PROG_TIMEOUT:
    self->output->info(_("Timeout"));
    break;
  default:
    break;
  }

  return YES; // Value only relevant for op == HT_A_CONFIRM
}
//______________________________________________________________________

namespace {
  struct libwwwError { int code; const char* msg; const char* type; };
  libwwwError libwwwErrors[] = { HTERR_ENGLISH_INITIALIZER };
}

int Download::afterFilter(HTRequest* request, HTResponse* /*response*/,
                          void* /*param*/, int status) {
  Download* self = getDownloadObj(request);

  if (status == HT_LOADED) {
    self->output->finished();
    return HT_OK;
  }

#if 1
  const char* msg = "";
  switch (status) {
  case HT_ERROR: msg = "HT_ERROR"; break;
  case HT_LOADED: msg = "HT_LOADED"; break;
  case HT_NO_DATA: msg = "HT_NO_DATA"; break;
  case HT_NO_ACCESS: msg = "HT_NO_ACCESS"; break;
  case HT_NO_PROXY_ACCESS: msg = "HT_NO_PROXY_ACCESS"; break;
  case HT_RETRY: msg = "HT_RETRY"; break;
  case HT_PERM_REDIRECT: msg = "HT_PERM_REDIRECT"; break;
  case HT_TEMP_REDIRECT: msg = "HT_TEMP_REDIRECT"; break;
  }
  cerr << "Status " << status << " (" << msg << ") for " << self->uri()
       << " obj " << self << endl;
#endif

  if (status >= 0) return HT_OK;

  HTList* errList = HTRequest_error(request);
  HTError* err;
  int errIndex = 0;
  while ((err = static_cast<HTError*>(HTList_removeFirstObject(errList)))) {
    errIndex = HTError_index(err);
#   if DEBUG
    cerr << "  " << libwwwErrors[errIndex].code << ' '
         << libwwwErrors[errIndex].msg << endl;
#   endif
  }

  string s;
  if (strcmp("client_error", libwwwErrors[errIndex].type) == 0
      || strcmp("server_error", libwwwErrors[errIndex].type) == 0) {
    // Include error code with HTTP errors
    append(s, libwwwErrors[errIndex].code);
    s += ' ';
  }
  s += libwwwErrors[errIndex].msg;
  self->output->error(s);

  return HT_OK;
}
