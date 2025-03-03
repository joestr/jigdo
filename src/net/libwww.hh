/* $Id: libwww.hh,v 1.8 2003/09/12 23:08:01 atterer Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/�|  Richard Atterer          |  atterer.org
  � '` �
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Include libwww headers, clean up namespace afterwards.

*/

#ifndef LIBWWW_HH
#define LIBWWW_HH

#include <dirent.hh>

#undef PACKAGE

extern "C" {
#include <HTInit.h>
#include <HTNet.h>
#include <HTReqMan.h>
#include <WWWCore.h>
#include <WWWStream.h>
#include <WWWTrans.h>
#include <HTParse.h>

// HTReqMan.h incorrectly defines the name as HTRequest_deleteRangeAll
extern BOOL HTRequest_deleteRange (HTRequest * request);
}

#include <config.h>
#undef PACKAGE
#define PACKAGE "jigdo"
#undef _
#if ENABLE_NLS
#  define _(String) dgettext (PACKAGE, String)
#  else
#  define _(String) (String)
#endif

#endif
