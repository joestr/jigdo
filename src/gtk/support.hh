/* $Id: support.hh,v 1.1.1.1 2003/07/04 22:29:48 atterer Exp $

   Originally created by glade, modified by Richard Atterer.

*/

#ifndef GTK_SUPPORT_HH
#define GTK_SUPPORT_HH

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

/*
 * Standard gettext macros.
 */
#if ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif


/*
 * Public Functions.
 */

/*
 * This function returns a widget in a component created by Glade.
 * Call it with the toplevel widget in the component (i.e. a window/dialog),
 * or alternatively any widget in the component, and the name of the widget
 * you want returned.
 */
GtkWidget*  lookup_widget              (GtkWidget       *widget,
                                        const gchar     *widget_name);


/* Use this function to set the directory containing installed pixmaps. */
void        add_pixmap_directory       (const gchar     *directory);


/*
 * Private Functions.
 */

/* This is used to create the pixmaps used in the interface. */
GtkWidget*  create_pixmap              (GtkWidget       *widget,
                                        const gchar     *filename);

/* This is used to create the pixbufs used in the interface. */
GdkPixbuf*  create_pixbuf              (const gchar     *filename);

/* This is used to set ATK action descriptions. */
void        glade_set_atk_action_description (AtkAction       *action,
                                              const gchar     *action_name,
                                              const gchar     *description);

#endif /* GTK_SUPPORT_HH */
