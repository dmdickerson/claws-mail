/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktext.h>
#include <gtk/gtkbutton.h>
#if HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif

#include "intl.h"
#include "about.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "utils.h"

#include "pixmaps/sylpheed-logo.xpm"

static GtkWidget *window;

static void about_create(void);
static void key_pressed(GtkWidget *widget, GdkEventKey *event);
static void about_uri_clicked(GtkButton *button, gpointer data);

void about_show(void)
{
	if (!window)
		about_create();
	else
		gtk_widget_show(window);
}

static void about_create(void)
{
	GtkWidget *vbox;
	GdkPixmap *logoxpm = NULL;
	GdkBitmap *logoxpmmask;
	GtkWidget *pixmap;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkWidget *confirm_area;
	GtkWidget *ok_button;
	GtkStyle *style;
	GdkColormap *cmap;
	GdkColor uri_color[2] = {{0, 0, 0, 0xffff}, {0, 0xffff, 0, 0}};
	gboolean success[2];
	
#if HAVE_SYS_UTSNAME_H
	struct utsname utsbuf;
#endif	
	gchar buf[1024];
	gint i;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(window), _("About"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_widget_set_usize(window, 518, 358);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	/* gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE); */
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	PIXMAP_CREATE(window, logoxpm, logoxpmmask, sylpheed_logo_xpm);
	pixmap = gtk_pixmap_new(logoxpm, logoxpmmask);
	gtk_box_pack_start(GTK_BOX(vbox), pixmap, FALSE, FALSE, 0);

	label = gtk_label_new("version "VERSION);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	
#if HAVE_SYS_UTSNAME_H
	uname(&utsbuf);
	g_snprintf(buf, sizeof(buf),
		   "GTK+ version %d.%d.%d\n"
		   "Operating System: %s %s (%s)",
		   gtk_major_version, gtk_minor_version, gtk_micro_version,
		   utsbuf.sysname, utsbuf.release, utsbuf.machine);
#else
	g_snprintf(buf, sizeof(buf),
		   "GTK+ version %d.%d.%d\n"
		   "Operating System: Windoze",
		   gtk_major_version, gtk_minor_version, gtk_micro_version);
#endif

	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	g_snprintf(buf, sizeof(buf),
		   "Compiled-in features:%s",
#if HAVE_GDK_IMLIB
		   " gdk_imlib"
#endif
#if HAVE_GDK_PIXBUF
		   " gdk-pixbuf"
#endif
#if USE_THREADS
		   " gthread"
#endif
#if INET6
		   " IPv6"
#endif
#if HAVE_LIBCOMPFACE
		   " libcompface"
#endif
#if HAVE_LIBJCONV
		   " libjconv"
#endif
#if USE_GPGME
		   " GPGME"
#endif
	"");

	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	label = gtk_label_new
		("Copyright (C) 1999-2001 Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_label(" "HOMEPAGE_URI" ");
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(about_uri_clicked), NULL);
	buf[0] = ' ';
	for (i = 1; i <= strlen(HOMEPAGE_URI); i++) buf[i] = '_';
	strcpy(buf + i, " ");
	gtk_label_set_pattern(GTK_LABEL(GTK_BIN(button)->child), buf);
	cmap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(cmap, uri_color, 2, FALSE, TRUE, success);
	if (success[0] == TRUE && success[1] == TRUE) {
		gtk_widget_ensure_style(GTK_BIN(button)->child);
		style = gtk_style_copy
			(gtk_widget_get_style(GTK_BIN(button)->child));
		style->fg[GTK_STATE_NORMAL]   = uri_color[0];
		style->fg[GTK_STATE_ACTIVE]   = uri_color[1];
		style->fg[GTK_STATE_PRELIGHT] = uri_color[0];
		gtk_widget_set_style(GTK_BIN(button)->child, style);
	} else
		g_warning("about_create(): color allocation failed.\n");

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	text = gtk_text_new(gtk_scrolled_window_get_hadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)),
			    gtk_scrolled_window_get_vadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)));
	gtk_text_set_word_wrap(GTK_TEXT(text), TRUE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	gtk_text_freeze(GTK_TEXT(text));

	gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
		_("The portions applied from fetchmail is Copyright 1997 by Eric S. "
		  "Raymond.  Portions of those are also copyrighted by Carl Harris, "
		  "1993 and 1995.  Copyright retained for the purpose of protecting free "
		  "redistribution of source.\n\n"), -1);

	gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
		_("Kcc is copyright by Yasuhiro Tonooka <tonooka@msi.co.jp>, "
		  "and libkcc is copyright by takeshi@SoftAgency.co.jp.\n\n"), -1);

#if USE_GPGME
	gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
		_("GPGME is copyright 2001 by Werner Koch <dd9jn@gnu.org>\n\n"), -1);
#endif /* USE_GPGME */

	gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
		_("This program is free software; you can redistribute it and/or modify "
		  "it under the terms of the GNU General Public License as published by "
		  "the Free Software Foundation; either version 2, or (at your option) "
		  "any later version.\n\n"), -1);

	gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
		_("This program is distributed in the hope that it will be useful, "
		  "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "
		  "See the GNU General Public License for more details.\n\n"), -1);

	gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
		_("You should have received a copy of the GNU General Public License "
		  "along with this program; if not, write to the Free Software "
		  "Foundation, Inc., 59 Temple Place - Suite 330, Boston, "
		  "MA 02111-1307, USA."), -1);

	gtk_text_thaw(GTK_TEXT(text));

	gtkut_button_set_create(&confirm_area, &ok_button, _("OK"),
				NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);
	gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete),
				  GTK_OBJECT(window));

	gtk_widget_show_all(window);
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(window);
}

static void about_uri_clicked(GtkButton *button, gpointer data)
{
	open_uri(HOMEPAGE_URI, prefs_common.uri_cmd);
}
