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
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtksignal.h>

#include "intl.h"
#include "main.h"
#include "inc.h"
#include "mbox.h"
#include "folderview.h"
#include "filesel.h"
#include "foldersel.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "folder.h"

static GtkWidget *window;
static GtkWidget *file_entry;
static GtkWidget *dest_entry;
static GtkWidget *file_button;
static GtkWidget *dest_button;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;
static gboolean import_ack;

static void import_create(void);
static void import_ok_cb(GtkWidget *widget, gpointer data);
static void import_cancel_cb(GtkWidget *widget, gpointer data);
static void import_filesel_cb(GtkWidget *widget, gpointer data);
static void import_destsel_cb(GtkWidget *widget, gpointer data);
static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data);
static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

gint import_mbox(FolderItem *default_dest)
{
	gint ok = 0;
	gchar *path;

	if (!window)
		import_create();
	else
		gtk_widget_show(window);

	gtk_entry_set_text(GTK_ENTRY(file_entry), "");

	if (default_dest) { 
		path = folder_item_get_identifier(default_dest);
		gtk_entry_set_text(GTK_ENTRY(dest_entry), path);
		g_free(path);
	}	
	else
		gtk_entry_set_text(GTK_ENTRY(dest_entry), "");
	gtk_widget_grab_focus(file_entry);

	manage_window_set_transient(GTK_WINDOW(window));

	gtk_main();

	if (import_ack) {
		gchar *filename, *destdir;
		FolderItem *dest;

		filename = gtk_entry_get_text(GTK_ENTRY(file_entry));
		destdir = gtk_entry_get_text(GTK_ENTRY(dest_entry));
		if (filename && *filename) {
			if (!destdir || !*destdir) {
				dest = folder_find_item_from_path(INBOX_DIR);
			} else
				dest = folder_find_item_from_identifier(destdir);

			if (!dest) {
				g_warning("Can't find the folder.\n");
			} else {
				ok = proc_mbox(dest, filename, NULL);
				folder_item_scan(dest);
				folderview_update_item(dest, TRUE);
			}
		}
	}

	gtk_widget_hide(window);

	return ok;
}

static void import_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *desc_label;
	GtkWidget *table;
	GtkWidget *file_label;
	GtkWidget *dest_label;
	GtkWidget *confirm_area;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(window), _("Import"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(delete_event), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);

	desc_label = gtk_label_new
		(_("Specify target mbox file and destination folder."));
	gtk_box_pack_start(GTK_BOX(hbox), desc_label, FALSE, FALSE, 0);

	table = gtk_table_new(2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);
	gtk_widget_set_usize(table, 420, -1);

	file_label = gtk_label_new(_("Importing file:"));
	gtk_table_attach(GTK_TABLE(table), file_label, 0, 1, 0, 1,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(file_label), 1, 0.5);

	dest_label = gtk_label_new(_("Destination dir:"));
	gtk_table_attach(GTK_TABLE(table), dest_label, 0, 1, 1, 2,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(dest_label), 1, 0.5);

	file_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), file_entry, 1, 2, 0, 1,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	dest_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), dest_entry, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	file_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), file_button, 2, 3, 0, 1,
			 0, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(file_button), "clicked",
			   GTK_SIGNAL_FUNC(import_filesel_cb), NULL);

	dest_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), dest_button, 2, 3, 1, 2,
			 0, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(dest_button), "clicked",
			   GTK_SIGNAL_FUNC(import_destsel_cb), NULL);

	gtkut_button_set_create(&confirm_area,
				&ok_button,	_("OK"),
				&cancel_button, _("Cancel"),
				NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			   GTK_SIGNAL_FUNC(import_ok_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(import_cancel_cb), NULL);

	gtk_widget_show_all(window);
}

static void import_ok_cb(GtkWidget *widget, gpointer data)
{
	import_ack = TRUE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void import_cancel_cb(GtkWidget *widget, gpointer data)
{
	import_ack = FALSE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void import_filesel_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;

	filename = filesel_select_file(_("Select importing file"), NULL);
	if (filename)
		gtk_entry_set_text(GTK_ENTRY(file_entry), filename);
}

static void import_destsel_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *dest;
	gchar *path;

	dest = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL);
	if (!dest)
		 return;
	path = folder_item_get_identifier(dest);
	gtk_entry_set_text(GTK_ENTRY(dest_entry), path);
	g_free(path);
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	import_cancel_cb(NULL, NULL);
	return TRUE;
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		import_cancel_cb(NULL, NULL);
}
