/* $Id: compat.hh,v 1.10 2002/03/17 22:13:38 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Cross-platform compatibility

*/

#ifndef COMPAT_HH
#define COMPAT_HH

#include <string>
namespace std { }
using namespace std;

#include <config.h>
#include <stdio.h>
#include <unistd.h>
//______________________________________________________________________

// No operator<< for uint64, so define our own
#if !HAVE_OUTUINT64
#include <iostream>
namespace std { }
using namespace std;
inline ostream& operator<<(ostream& s, const uint64 x) {
  s << static_cast<unsigned long>(x / 1000000000)
    << static_cast<unsigned long>(x % 1000000000);
  return s;
}
#endif
//______________________________________________________________________

/* Truncate a file to a given length. Behaviour undefined if given
   length is bigger than current file size */
#if HAVE_TRUNCATE
inline int compat_truncate(const char* path, uint64 length) {
  return truncate(path, length);
}
#else
int compat_truncate(const char* path, uint64 length);
#endif
//______________________________________________________________________

/* Rename a file. Mingw does provide rename(), but gives an error if
   the destination name already exists. This one doesn't. */
#if !WINDOWS
inline int compat_rename(const char* src, const char* dst) {
  return rename(src, dst);
}
#else
int compat_rename(const char* src, const char* dst);
#endif
//______________________________________________________________________

/* Width in characters of the tty (for progress display), or 0 if not
   a tty or functions not present on system. */
#if WINDOWS
inline int ttyWidth() { return 80; }
#elif !HAVE_IOCTL_WINSZ || !HAVE_FILENO
inline int ttyWidth() { return 0; }
#else
extern int ttyWidth();
#endif
//______________________________________________________________________

/** (For "file:" URI handling) If directory separator on this system
    is not '/', exchange the actual separator for '/' and vice versa
    in the supplied string. Do the same for any non-'.' file extension
    separator. We trust in the optimizer to remove unnecessary
    code. */
inline void compat_swapFileUriChars(string& s) {
  // Need this "if" because gcc cannot optimize away loops
  if (DIRSEP != '/' || EXTSEP != '.') {
    for (string::iterator i = s.begin(), e = s.end(); i != e; ++i) {
      if (DIRSEP != '/' && *i == DIRSEP) *i = '/';
      else if (DIRSEP != '/' && *i == '/') *i = DIRSEP;
      else if (EXTSEP != '.' && *i == EXTSEP) *i = '.';
      else if (EXTSEP != '.' && *i == '.') *i = EXTSEP;
    }
  }
}

#endif
