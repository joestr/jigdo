/* $Id: gtk-error.cc,v 1.3 2002/02/13 00:36:16 richard Exp $ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002 Richard Atterer
  | \/¯|  <richard@atterer.net>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Display an error box with a message and an OK button.

*/

#include <config.h>
#include <gtk-error.hh>
#include <gtk-gui.hh>
//______________________________________________________________________

void errorBox(const char* message) {
  gtk_label_set_text(GTK_LABEL(GUI::errorbox.message), message);
  gtk_widget_show(GUI::errorbox.errorbox);
}
//______________________________________________________________________

void on_errorbox_okButton_clicked(GtkButton*, gpointer) {
  gtk_widget_hide(GUI::errorbox.errorbox);
}
