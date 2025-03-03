/* $Id: proxyguess.hh,v 1.2 2005/04/09 23:09:52 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/�|  Richard Atterer     |  atterer.org
  � '` �
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  Find proxy URLs by reading config files of various browsers.

*/

#ifndef PROXYGUESS_HH
#define PROXYGUESS_HH

#include <config.h>
#include <set>
#include <string>

/** Sets http and ftp to the selected proxy, or clears it if using direct
    internet connection. Accumulates noproxy entries from all sources. Call
    this after glibwww_init(). */
void proxyGuess();

#endif
