#/* $Id: gtk-gui.hh,v 1.9 2002/04/01 00:40:56 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2 or
  later. See the file COPYING for details.

  Global variables: Pointers to GUI elements.

*/

#ifndef GTK_GUI_H
#define GTK_GUI_H

#include <config.h>
#include <gtk-interface.hh>
//______________________________________________________________________

namespace GUI {

  // Are initialized by create()
  extern Window window;
  extern Filesel filesel;
  extern Errorbox errorbox;
  extern License license;
  // To be called by main() to set up the variables above
  void create();

} // namespace GUI
//______________________________________________________________________

// Callback prototypes for gtk-interface.cc

// Defined in gtk-gui.cc
void on_toolbarExit_clicked(GtkButton*, gpointer);
gboolean on_window_delete_event(GtkWidget*, GdkEvent*, gpointer);
void setNotebookPage(GtkWidget* pageObject);
void on_openButton_clicked(GtkButton*, gpointer);
void on_toolbarExit_clicked(GtkButton*, gpointer);
void on_aboutJigdoButton_clicked(GtkButton*, gpointer);

// Defined in gtk-error.cc
void on_errorbox_okButton_clicked(GtkButton*, gpointer);
//______________________________________________________________________

#endif
