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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "manage_window.h"
#include "description_window.h"
#include "gtkutils.h"

static void description_create			(DescriptionWindow *dwindow);
static gboolean description_window_key_pressed	(GtkWidget *widget,
						 GdkEventKey *event,
						 gpointer data);

void description_window_create(DescriptionWindow *dwindow)
{
	if (!dwindow->window)
		description_create(dwindow);

	manage_window_set_transient(GTK_WINDOW(dwindow->window));
	gtk_widget_show(dwindow->window);
	gtk_main();
	gtk_widget_hide(dwindow->window);
}

static void description_create(DescriptionWindow * dwindow)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *scrolledwin;
	int i;
	int sz;
	int line;
	int j;

	dwindow->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_usize(dwindow->window,400,450);
	
	gtk_window_set_title(GTK_WINDOW(dwindow->window),
			     gettext(dwindow->title));
	gtk_container_set_border_width(GTK_CONTAINER(dwindow->window), 8);
	gtk_window_set_modal(GTK_WINDOW(dwindow->window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(dwindow->window), FALSE, TRUE, FALSE);

	/* Check number of lines to be show */
	sz = 0;
	for (i = 0; dwindow->symbol_table[i] != NULL; i = i + dwindow->columns) {
		sz++;
	}
	
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	table = gtk_table_new(sz, dwindow->columns, FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwin), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 4);

	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	line = 0;
	for(i = 0; dwindow->symbol_table[i] != NULL; i = i + dwindow->columns) {
		if(dwindow->symbol_table[i][0] != '\0') {
			GtkWidget *label;

			for (j = 0; j < dwindow->columns; j++) {
				gint col = j;
				gint colend = j+1;
				/* Expand using next NULL columns */
				while ((colend < dwindow->columns) && 
				       (dwindow->symbol_table[i+colend] == NULL)) {
				       colend++;
				       j++;
				}
				label = gtk_label_new(gettext(dwindow->symbol_table[i+col]));
				gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
				gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
				gtk_table_attach(GTK_TABLE(table), label,
						 col, colend, line, line+1,
						 GTK_EXPAND | GTK_FILL, 0,
						 0, 0);
			}
		} else {
			GtkWidget *separator;
			
			separator = gtk_hseparator_new();
			gtk_table_attach(GTK_TABLE(table), separator,
					 0, dwindow->columns, line, line+1,
					 GTK_EXPAND | GTK_FILL, 0,
					 0, 4);
		}
		line++;
	}

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				NULL, NULL, NULL, NULL);
	gtk_widget_show(hbbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(dwindow->window), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(scrolledwin),
			   TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbbox),
			   FALSE, FALSE, 3);
			   
/* OLD CODE
	gtk_table_attach_defaults(GTK_TABLE(table), hbbox,
				  1, 2, i, i+1);
*/
	gtk_widget_grab_default(ok_btn);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
	gtk_signal_connect
		(GTK_OBJECT(dwindow->window), "key_press_event",
		 GTK_SIGNAL_FUNC(description_window_key_pressed),
		 NULL);
	gtk_signal_connect(GTK_OBJECT(dwindow->window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_widget_show_all(table);
}

static gboolean description_window_key_pressed(GtkWidget *widget,
					   GdkEventKey *event,
					   gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_main_quit();
	return FALSE;
}

