/* $Id: compat.cc,v 1.8 2002/02/13 00:36:16 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Cross-platform compatibility

*/

#include <compat.hh>
#include <stdio.h>
#if WINDOWS
#  include <windows.h>
#  include <winbase.h>
#endif
//______________________________________________________________________

#if HAVE_TRUNCATE
// No additional code required
//______________________________________________________________________

#elif !HAVE_TRUNCATE && HAVE_FTRUNCATE
// Truncate using POSIX ftruncate()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef O_LARGEFILE
#  define O_LARGEFILE 0 // Linux: Allow 64-bit file sizes on 32-bit arches
#endif
int compat_truncate(const char* path, uint64 length) {
  int fd = open(path, O_RDWR | O_LARGEFILE);
  if (fd == -1) return -1;
  if (ftruncate(fd, length) != 0) { close(fd); return -1; }
  return close(fd);
}
//______________________________________________________________________

#elif !HAVE_TRUNCATE && WINDOWS
// Truncate using native Windows API
int compat_truncate(const char* path, uint64 length) {
  // TODO error handling: GetLastError(), FormatMessage()
  HANDLE handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, 0,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (handle == INVALID_HANDLE_VALUE) return -1;
  LONG lengthHi = length >> 32;
  if (SetFilePointer(handle, length, &lengthHi, FILE_BEGIN) == 0xffffffffU
      || SetEndOfFile(handle) == 0U) {
    CloseHandle(handle);
    return -1;
  }
  if (CloseHandle(handle) == 0) return -1;
  return 0;
}
//______________________________________________________________________

#else
#  error "No implementation for truncating files!"
#endif
//====================================================================

#if WINDOWS
// Delete destination before renaming
int compat_rename(const char* src, const char* dst) {
  remove(dst); // Ignore errors
  return rename(src, dst);
}
#endif
//====================================================================

#if !WINDOWS && HAVE_IOCTL_WINSZ && HAVE_FILENO
#include <sys/ioctl.h>
#include <errno.h>
int ttyWidth() {
  struct winsize w;
  if (ioctl(fileno(stderr), TIOCGWINSZ, &w) == -1
      || w.ws_col == 0)
    return 0;
  return w.ws_col;
}
#endif
