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

typedef struct _WrappingPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *spinbtn_linewrap;
	GtkWidget *checkbtn_wrapquote;
	GtkWidget *checkbtn_autowrap;
	GtkWidget *checkbtn_wrapatsend;
	GtkWidget *checkbtn_smart_wrapping;
} WrappingPage;

void prefs_wrapping_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	WrappingPage *prefs_wrapping = (WrappingPage *) _page;
	
	GtkWidget *table;
	GtkWidget *label_linewrap;
	GtkObject *spinbtn_linewrap_adj;
	GtkWidget *spinbtn_linewrap;
	GtkWidget *checkbtn_wrapquote;
	GtkWidget *checkbtn_autowrap;
	GtkWidget *checkbtn_wrapatsend;
	GtkWidget *checkbtn_smart_wrapping;
	GtkWidget *hbox1;

	table = gtk_table_new(8, 3, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

  	checkbtn_autowrap = gtk_check_button_new_with_label(_("Wrap on input"));
	gtk_widget_show (checkbtn_autowrap);
  	gtk_table_attach (GTK_TABLE (table), checkbtn_autowrap, 0, 1, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	checkbtn_wrapatsend = gtk_check_button_new_with_label(_("Wrap before sending"));
	gtk_widget_show (checkbtn_wrapatsend);
  	gtk_table_attach (GTK_TABLE (table), checkbtn_wrapatsend, 0, 1, 2, 3,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	checkbtn_wrapquote = gtk_check_button_new_with_label(_("Wrap quotation"));
	gtk_widget_show (checkbtn_wrapquote);
  	gtk_table_attach (GTK_TABLE (table), checkbtn_wrapquote, 0, 1, 3, 4,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	checkbtn_smart_wrapping = 
		gtk_check_button_new_with_label(_("Smart wrapping (EXPERIMENTAL)"));
	gtk_widget_show (checkbtn_smart_wrapping);
  	gtk_table_attach (GTK_TABLE (table), checkbtn_smart_wrapping, 0, 1, 4, 5,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);
	
  	hbox1 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox1);
  	gtk_table_attach (GTK_TABLE (table), hbox1, 0, 1, 5, 6,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (GTK_FILL), 0, 0);

	label_linewrap = gtk_label_new (_("Wrap messages at"));
	gtk_widget_show (label_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox1), label_linewrap, FALSE, FALSE, 4);

	spinbtn_linewrap_adj = gtk_adjustment_new (72, 20, 1024, 1, 10, 10);
	spinbtn_linewrap = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_linewrap_adj), 1, 0);
	gtk_widget_show (spinbtn_linewrap);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_linewrap, FALSE, FALSE, 0);

	label_linewrap = gtk_label_new (_("characters"));
	gtk_widget_show (label_linewrap);
  	gtk_box_pack_start (GTK_BOX (hbox1), label_linewrap, FALSE, FALSE, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_autowrap),
				     prefs_common.autowrap);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_wrapatsend),
				     prefs_common.linewrap_at_send);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_wrapquote),
				     prefs_common.linewrap_quote);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_smart_wrapping),
				     prefs_common.smart_wrapping);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_linewrap),
				  prefs_common.linewrap_len);

	prefs_wrapping->window			= GTK_WIDGET(window);
	prefs_wrapping->spinbtn_linewrap	= spinbtn_linewrap;
	prefs_wrapping->checkbtn_wrapquote	= checkbtn_wrapquote;
	prefs_wrapping->checkbtn_autowrap	= checkbtn_autowrap;
	prefs_wrapping->checkbtn_wrapatsend	= checkbtn_wrapatsend;
	prefs_wrapping->checkbtn_smart_wrapping	= checkbtn_smart_wrapping;

	prefs_wrapping->page.widget = table;
}

void prefs_wrapping_save(PrefsPage *_page)
{
	WrappingPage *page = (WrappingPage *) _page;

	prefs_common.linewrap_len = 
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->spinbtn_linewrap));
	prefs_common.linewrap_quote = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_wrapquote));
	prefs_common.autowrap =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_autowrap));
	prefs_common.linewrap_at_send = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_wrapatsend));
	prefs_common.smart_wrapping = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_smart_wrapping));
}

static void prefs_wrapping_destroy_widget(PrefsPage *_page)
{
}

WrappingPage *prefs_wrapping;

void prefs_wrapping_init(void)
{
	WrappingPage *page;

	page = g_new0(WrappingPage, 1);
	page->page.path = _("Compose/Message Wrapping");
	page->page.create_widget = prefs_wrapping_create_widget;
	page->page.destroy_widget = prefs_wrapping_destroy_widget;
	page->page.save_page = prefs_wrapping_save;
	page->page.weight = 60.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_wrapping = page;
}

void prefs_wrapping_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_wrapping);
	g_free(prefs_wrapping);
}
