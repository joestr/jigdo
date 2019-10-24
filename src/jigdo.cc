/* $Id: jigdo.cc,v 1.18 2002/03/31 22:05:24 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2 or
  later. See the file COPYING for details.

*/

#include <iostream>
namespace std { }
using namespace std;

#include <config.h>
#include <debug.hh>
#include <download.hh>
#include <gtk-gui.hh>
#include <gtk-support.hh>
//______________________________________________________________________

int main (int argc, char *argv[]) {

# if ENABLE_NLS
  bindtextdomain(PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain(PACKAGE);
# endif

  // Initialize GTK+ and display window
  gtk_set_locale();
  gtk_init(&argc, &argv);
  add_pixmap_directory(PACKAGE_DATA_DIR DIRSEPS "pixmaps");
  GUI::create();
  gtk_widget_show(GUI::window.window);

  Download::init();

  gtk_main();
  return 0;
}
