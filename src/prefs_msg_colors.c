/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & The Sylpheed-Claws Team
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

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"

static struct MessageColorButtons {
	GtkWidget *quote_level1_btn;
	GtkWidget *quote_level2_btn;
	GtkWidget *quote_level3_btn;
	GtkWidget *uri_btn;
	GtkWidget *tgt_folder_btn;
	GtkWidget *signature_btn;
} color_buttons;

typedef struct _MsgColorsPage
{
	PrefsPage page;

	GtkWidget *window;
	
	GtkWidget *chkbtn_enablecol;
	GtkWidget *chkbtn_recycle_colors;
} MsgColorsPage;

static GtkWidget *color_dialog;

static gboolean prefs_quote_colors_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void quote_color_set_dialog		(GtkWidget	*widget,
						 gpointer	 data);
static void quote_colors_set_dialog_ok		(GtkWidget	*widget,
						 gpointer	 data);
static void quote_colors_set_dialog_cancel	(GtkWidget	*widget,
						 gpointer	 data);
static gboolean quote_colors_set_dialog_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void set_button_bg_color			(GtkWidget	*widget,
						 gint		 color);

void prefs_msg_colors_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	    gpointer data)
{
	MsgColorsPage *prefs_msg_colors = (MsgColorsPage *) _page;
	
	GtkWidget *table;
	GtkWidget *chkbtn_enablecol;
	GtkWidget *quotelevel1_label;
	GtkWidget *quotelevel2_label;
	GtkWidget *quotelevel3_label;
	GtkWidget *uri_label;
	GtkWidget *signature_label;
	GtkWidget *tgt_folder_label;
	GtkWidget *chkbtn_recycle_colors;
	GtkWidget *hbox;

	table = gtk_table_new(8, 3, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

  	chkbtn_enablecol = 
		gtk_check_button_new_with_label(_("Enable coloration of message"));
  	gtk_table_attach (GTK_TABLE (table), chkbtn_enablecol, 0, 1, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 2, 3,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	color_buttons.quote_level1_btn = gtk_button_new();
	gtk_widget_set_size_request (color_buttons.quote_level1_btn, 30, 20);
  	gtk_box_pack_start (GTK_BOX(hbox), color_buttons.quote_level1_btn, 
			    FALSE, FALSE, 0);

	quotelevel1_label = gtk_label_new (_("Quoted Text - First Level"));
  	gtk_box_pack_start (GTK_BOX(hbox), quotelevel1_label, 
			    FALSE, FALSE, 0);
		
	hbox = gtk_hbox_new(FALSE, 8);
	gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 3, 4,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	color_buttons.quote_level2_btn = gtk_button_new();
	gtk_widget_set_size_request (color_buttons.quote_level2_btn, 30, 20);
  	gtk_box_pack_start (GTK_BOX(hbox), color_buttons.quote_level2_btn, 
			    FALSE, FALSE, 0);

	quotelevel2_label = gtk_label_new (_("Quoted Text - Second Level"));
  	gtk_box_pack_start (GTK_BOX(hbox), quotelevel2_label, 
			    FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 4, 5,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	color_buttons.quote_level3_btn = gtk_button_new_with_label ("");
	gtk_widget_set_size_request (color_buttons.quote_level3_btn, 30, 20);
  	gtk_box_pack_start (GTK_BOX(hbox), color_buttons.quote_level3_btn, 
			    FALSE, FALSE, 0);

	quotelevel3_label = gtk_label_new (_("Quoted Text - Third Level"));
  	gtk_box_pack_start (GTK_BOX(hbox), quotelevel3_label, 
			    FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 5, 6,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	color_buttons.uri_btn = gtk_button_new_with_label ("");
	gtk_widget_set_size_request (color_buttons.uri_btn, 30, 20);
  	gtk_box_pack_start (GTK_BOX(hbox), color_buttons.uri_btn, 
			    FALSE, FALSE, 0);

	uri_label = gtk_label_new (_("URI link"));
  	gtk_box_pack_start (GTK_BOX(hbox), uri_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 6, 7,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	color_buttons.tgt_folder_btn = gtk_button_new_with_label ("");
	gtk_widget_set_size_request (color_buttons.tgt_folder_btn, 30, 20);
  	gtk_box_pack_start (GTK_BOX(hbox), color_buttons.tgt_folder_btn, 
			    FALSE, FALSE, 0);

	tgt_folder_label = gtk_label_new (_("Target folder"));
  	gtk_box_pack_start (GTK_BOX(hbox), tgt_folder_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 7, 8,
			  (GTK_EXPAND | GTK_FILL), 0, 0, 0);

	color_buttons.signature_btn = gtk_button_new_with_label ("");
	gtk_widget_set_size_request (color_buttons.signature_btn, 30, 20);
  	gtk_box_pack_start (GTK_BOX(hbox), color_buttons.signature_btn, 
			    FALSE, FALSE, 0);

	signature_label = gtk_label_new (_("Signatures"));
  	gtk_box_pack_start (GTK_BOX(hbox), signature_label, FALSE, FALSE, 0);

  	chkbtn_recycle_colors = 
		gtk_check_button_new_with_label(_("Recycle quote colors"));
  	gtk_table_attach (GTK_TABLE (table), chkbtn_recycle_colors, 0, 1, 8, 9,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);
			  
	gtk_widget_show_all(table);

	g_signal_connect(G_OBJECT(color_buttons.quote_level1_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "LEVEL1");
	g_signal_connect(G_OBJECT(color_buttons.quote_level2_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "LEVEL2");
	g_signal_connect(G_OBJECT(color_buttons.quote_level3_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "LEVEL3");
	g_signal_connect(G_OBJECT(color_buttons.uri_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "URI");
	g_signal_connect(G_OBJECT(color_buttons.tgt_folder_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "TGTFLD");
	g_signal_connect(G_OBJECT(color_buttons.signature_btn), "clicked",
			 G_CALLBACK(quote_color_set_dialog), "SIGNATURE");

	set_button_bg_color(color_buttons.quote_level1_btn,
			    prefs_common.quote_level1_col);
	set_button_bg_color(color_buttons.quote_level2_btn,
			    prefs_common.quote_level2_col);
	set_button_bg_color(color_buttons.quote_level3_btn,
			    prefs_common.quote_level3_col);
	set_button_bg_color(color_buttons.uri_btn,
			    prefs_common.uri_col);
	set_button_bg_color(color_buttons.tgt_folder_btn,
			    prefs_common.tgt_folder_col);
	set_button_bg_color(color_buttons.signature_btn,
			    prefs_common.signature_col);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_enablecol),
				     prefs_common.enable_color);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_recycle_colors),
				     prefs_common.recycle_quote_colors);

	prefs_msg_colors->window 		= GTK_WIDGET(window);
	prefs_msg_colors->chkbtn_enablecol 	= chkbtn_enablecol;
	prefs_msg_colors->chkbtn_recycle_colors	= chkbtn_recycle_colors;

	prefs_msg_colors->page.widget = table;
}

static gboolean prefs_quote_colors_key_pressed(GtkWidget *widget,
					   GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_main_quit();
	return FALSE;
}

static void quote_color_set_dialog(GtkWidget *widget, gpointer data)
{
	gchar *type = (gchar *)data;
	gchar *title = NULL;
	gdouble color[4] = {0.0, 0.0, 0.0, 0.0};
	gint rgbvalue = 0;
	GtkColorSelectionDialog *dialog;

	if(g_strcasecmp(type, "LEVEL1") == 0) {
		title = _("Pick color for quotation level 1");
		rgbvalue = prefs_common.quote_level1_col;
	} else if(g_strcasecmp(type, "LEVEL2") == 0) {
		title = _("Pick color for quotation level 2");
		rgbvalue = prefs_common.quote_level2_col;
	} else if(g_strcasecmp(type, "LEVEL3") == 0) {
		title = _("Pick color for quotation level 3");
		rgbvalue = prefs_common.quote_level3_col;
	} else if(g_strcasecmp(type, "URI") == 0) {
		title = _("Pick color for URI");
		rgbvalue = prefs_common.uri_col;
	} else if(g_strcasecmp(type, "TGTFLD") == 0) {
		title = _("Pick color for target folder");
		rgbvalue = prefs_common.tgt_folder_col;
	} else if(g_strcasecmp(type, "SIGNATURE") == 0) {
		title = _("Pick color for signatures");
		rgbvalue = prefs_common.signature_col;
	} else {   /* Should never be called */
		g_warning("Unrecognized datatype '%s' in quote_color_set_dialog\n", type);
		return;
	}

	color_dialog = gtk_color_selection_dialog_new(title);
	gtk_window_set_position(GTK_WINDOW(color_dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);
	gtk_window_set_policy(GTK_WINDOW(color_dialog), FALSE, FALSE, FALSE);
	manage_window_set_transient(GTK_WINDOW(color_dialog));

	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->ok_button),
			 "clicked", G_CALLBACK(quote_colors_set_dialog_ok), data);
	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->cancel_button),
			 "clicked", G_CALLBACK(quote_colors_set_dialog_cancel), data);
	g_signal_connect(G_OBJECT(color_dialog), "key_press_event",
			 G_CALLBACK(quote_colors_set_dialog_key_pressed),data);

	/* preselect the previous color in the color selection dialog */
	color[0] = (gdouble) ((rgbvalue & 0xff0000) >> 16) / 255.0;
	color[1] = (gdouble) ((rgbvalue & 0x00ff00) >>  8) / 255.0;
	color[2] = (gdouble)  (rgbvalue & 0x0000ff)        / 255.0;
	dialog = GTK_COLOR_SELECTION_DIALOG(color_dialog);
	gtk_color_selection_set_color
		(GTK_COLOR_SELECTION(dialog->colorsel), color);

	gtk_widget_show(color_dialog);
}

static void quote_colors_set_dialog_ok(GtkWidget *widget, gpointer data)
{
	GtkColorSelection *colorsel = (GtkColorSelection *)
						((GtkColorSelectionDialog *)color_dialog)->colorsel;
	gdouble color[4];
	gint red, green, blue, rgbvalue;
	gchar *type = (gchar *)data;

	gtk_color_selection_get_color(colorsel, color);

	red      = (gint) (color[0] * 255.0);
	green    = (gint) (color[1] * 255.0);
	blue     = (gint) (color[2] * 255.0);
	rgbvalue = (gint) ((red * 0x10000) | (green * 0x100) | blue);

#if 0
	fprintf(stderr, "redc = %f, greenc = %f, bluec = %f\n", color[0], color[1], color[2]);
	fprintf(stderr, "red = %d, green = %d, blue = %d\n", red, green, blue);
	fprintf(stderr, "Color is %x\n", rgbvalue);
#endif

	if (g_strcasecmp(type, "LEVEL1") == 0) {
		prefs_common.quote_level1_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level1_btn, rgbvalue);
	} else if (g_strcasecmp(type, "LEVEL2") == 0) {
		prefs_common.quote_level2_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level2_btn, rgbvalue);
	} else if (g_strcasecmp(type, "LEVEL3") == 0) {
		prefs_common.quote_level3_col = rgbvalue;
		set_button_bg_color(color_buttons.quote_level3_btn, rgbvalue);
	} else if (g_strcasecmp(type, "URI") == 0) {
		prefs_common.uri_col = rgbvalue;
		set_button_bg_color(color_buttons.uri_btn, rgbvalue);
	} else if (g_strcasecmp(type, "TGTFLD") == 0) {
		prefs_common.tgt_folder_col = rgbvalue;
		set_button_bg_color(color_buttons.tgt_folder_btn, rgbvalue);
		folderview_set_target_folder_color(prefs_common.tgt_folder_col);
	} else if (g_strcasecmp(type, "SIGNATURE") == 0) {
		prefs_common.signature_col = rgbvalue;
		set_button_bg_color(color_buttons.signature_btn, rgbvalue);
	} else
		fprintf( stderr, "Unrecognized datatype '%s' in quote_color_set_dialog_ok\n", type );

	gtk_widget_destroy(color_dialog);
}

static void quote_colors_set_dialog_cancel(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(color_dialog);
}

static gboolean quote_colors_set_dialog_key_pressed(GtkWidget *widget,
						GdkEventKey *event,
						gpointer data)
{
	gtk_widget_destroy(color_dialog);
	return FALSE;
}

static void set_button_bg_color(GtkWidget *widget, gint rgbvalue)
{
	GtkStyle *newstyle;
	GdkColor color;

	gtkut_convert_int_to_gdk_color(rgbvalue, &color);
	newstyle = gtk_style_copy(gtk_widget_get_default_style());
	newstyle->bg[GTK_STATE_NORMAL]   = color;
	newstyle->bg[GTK_STATE_PRELIGHT] = color;
	newstyle->bg[GTK_STATE_ACTIVE]   = color;

	gtk_widget_set_style(GTK_WIDGET(widget), newstyle);
}

void prefs_msg_colors_save(PrefsPage *_page)
{
	MsgColorsPage *page = (MsgColorsPage *) _page;

	prefs_common.enable_color = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->chkbtn_enablecol));
	prefs_common.recycle_quote_colors =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->chkbtn_recycle_colors));
}

static void prefs_msg_colors_destroy_widget(PrefsPage *_page)
{
}

MsgColorsPage *prefs_msg_colors;

void prefs_msg_colors_init(void)
{
	MsgColorsPage *page;

	page = g_new0(MsgColorsPage, 1);
	page->page.path = _("Message View/Colors");
	page->page.create_widget = prefs_msg_colors_create_widget;
	page->page.destroy_widget = prefs_msg_colors_destroy_widget;
	page->page.save_page = prefs_msg_colors_save;
	page->page.weight = 60.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_msg_colors = page;
}

void prefs_msg_colors_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_msg_colors);
	g_free(prefs_msg_colors);
}

