/* $Id: gtk-error.hh,v 1.3 2002/02/13 00:36:16 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Display an error box with a message and an OK button.

*/

#ifndef GTK_ERROR_HH
#define GTK_ERROR_HH

#include <string>
namespace std { }
using namespace std;
//______________________________________________________________________

/** Displays message in GUI::errorbox.errorbox - when the user clicks
    the OK button, the window is closed again. There is only ever one
    error window on screen, any new calls will simply replace the old
    error message in case the window is already open. */
void errorBox(const char* message);
inline void errorBox(const string& message) { errorBox(message.c_str()); }
//______________________________________________________________________

#endif
