/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & the Sylpheed-Claws team
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "utils.h"
#include "menu.h"
#include "prefs_common.h"
#include "description_window.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "quicksearch.h"

struct _QuickSearch
{
	GtkWidget			*hbox_search;
	GtkWidget			*search_type;
	GtkWidget			*search_type_opt;
	GtkWidget			*search_string_entry;
	GtkWidget			*search_description;

	gboolean			 active;
	gchar				*search_string;
	MatcherList			*matcher_list;

	QuickSearchExecuteCallback	 callback;
	gpointer			 callback_data;
};

static void prepare_matcher(QuickSearch *quicksearch)
{
	const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

	if (quicksearch->matcher_list != NULL) {
		matcherlist_free(quicksearch->matcher_list);
		quicksearch->matcher_list = NULL;
	}

	if (search_string == NULL || search_string[0] == '\0') {
		quicksearch->active = FALSE;
		return;
	}

	if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED) {
		char *newstr = NULL;

		newstr = expand_search_string(search_string);
		if (newstr && newstr[0] != '\0') {
			quicksearch->matcher_list = matcher_parser_get_cond(newstr);
			g_free(newstr);
		} else {
			quicksearch->matcher_list = NULL;
			quicksearch->active = FALSE;

			return;
		}
	} else {
		if (quicksearch->search_string != NULL)
			g_free(quicksearch->search_string);
		quicksearch->search_string = g_strdup(search_string);
	}

	quicksearch->active = TRUE;
}

static void update_extended_button (QuickSearch *quicksearch)
{
	GtkWidget *btn = quicksearch->search_description;
	
	g_return_if_fail(btn != NULL);
		
	if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED) {
		gtk_button_set_label(GTK_BUTTON(btn), _("Extended symbols"));
		gtk_widget_show(btn);
	} else {
		gtk_widget_hide(btn);
	}
	
}

static gboolean searchbar_pressed(GtkWidget *widget, GdkEventKey *event,
			      	  QuickSearch *quicksearch)
{
	if (event != NULL && event->keyval == GDK_Return) {
		const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

		if (search_string && strlen(search_string) != 0) {
			prefs_common.summary_quicksearch_history =
				add_history(prefs_common.summary_quicksearch_history,
					    search_string);
			gtk_combo_set_popdown_strings(GTK_COMBO(quicksearch->search_string_entry), 
				prefs_common.summary_quicksearch_history);			
		}

		prepare_matcher(quicksearch);

		if (quicksearch->callback != NULL)
			quicksearch->callback(quicksearch, quicksearch->callback_data);
	 	g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
		return TRUE;
	}

	return FALSE; 		
}

static gboolean searchtype_changed(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;

	prefs_common.summary_quicksearch_type = GPOINTER_TO_INT(g_object_get_data(
				   G_OBJECT(GTK_MENU_ITEM(gtk_menu_get_active(
				   GTK_MENU(quicksearch->search_type)))), MENU_VAL_ID));

	/* Show extended search description button, only when Extended is selected */
	update_extended_button(quicksearch);

	prepare_matcher(quicksearch);

	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);
}

/*
 * Strings describing how to use Extended Search
 * 
 * When adding new lines, remember to put 2 strings for each line
 */
static gchar *search_descr_strings[] = {
	"a",	 N_("all messages"),
	"ag #",  N_("messages whose age is greater than #"),
	"al #",  N_("messages whose age is less than #"),
	"b S",	 N_("messages which contain S in the message body"),
	"B S",	 N_("messages which contain S in the whole message"),
	"c S",	 N_("messages carbon-copied to S"),
	"C S",	 N_("message is either to: or cc: to S"),
	"D",	 N_("deleted messages"), /** how I can filter deleted messages **/
	"e S",	 N_("messages which contain S in the Sender field"),
	"E S",	 N_("true if execute \"S\" succeeds"),
	"f S",	 N_("messages originating from user S"),
	"F",	 N_("forwarded messages"),
	"h S",	 N_("messages which contain header S"),
	"i S",	 N_("messages which contain S in Message-Id header"),
	"I S",	 N_("messages which contain S in inreplyto header"),
	"L",	 N_("locked messages"),
	"n S",	 N_("messages which are in newsgroup S"),
	"N",	 N_("new messages"),
	"O",	 N_("old messages"),
	"r",	 N_("messages which have been replied to"),
	"R",	 N_("read messages"),
	"s S",	 N_("messages which contain S in subject"),
	"se #",  N_("messages whose score is equal to #"),
	"sg #",  N_("messages whose score is greater than #"),
	"sl #",  N_("messages whose score is lower than #"),
	"Se #",  N_("messages whose size is equal to #"),
	"Sg #",  N_("messages whose size is greater than #"),
	"Ss #",  N_("messages whose size is smaller than #"),
	"t S",	 N_("messages which have been sent to S"),
	"T",	 N_("marked messages"),
	"U",	 N_("unread messages"),
	"x S",	 N_("messages which contain S in References header"),
	"X cmd", N_("messages returning 0 when passed to command"),
	"y S",	 N_("messages which contain S in X-Label header"),
	 "",	 "" ,
	"&",	 N_("logical AND operator"),
	"|",	 N_("logical OR operator"),
	"! or ~",	N_("logical NOT operator"),
	"%",	 N_("case sensitive search"),
	NULL,	 NULL 
};
 
static DescriptionWindow search_descr = {
	NULL, 
	2,
	N_("Extended Search symbols"),
	search_descr_strings
};
	
static void search_description_cb(GtkWidget *widget)
{
	description_window_create(&search_descr);
};

static gboolean clear_search_cb(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;
	
	quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");
	
	return TRUE;
};

/*
static void summary_searchbar_focus_evt(GtkWidget *widget, GdkEventFocus *event,
					SummaryView *summaryview)
{
	if (event != NULL && event->in)
		gtk_signal_handler_block_by_func(GTK_OBJECT(summaryview->mainwin->window), 
						 GTK_SIGNAL_FUNC(mainwindow_key_pressed),
                                         	 summaryview->mainwin);
	else
		gtk_signal_handler_unblock_by_func(GTK_OBJECT(summaryview->mainwin->window), 
						   GTK_SIGNAL_FUNC(mainwindow_key_pressed),
                                         	   summaryview->mainwin);
}
*/

QuickSearch *quicksearch_new()
{
	QuickSearch *quicksearch;

	GtkWidget *hbox_search;
	GtkWidget *search_type_opt;
	GtkWidget *search_type;
	GtkWidget *search_string_entry;
	GtkWidget *search_hbbox;
	GtkWidget *search_description;
	GtkWidget *clear_search;
	GtkWidget *menuitem;

	quicksearch = g_new0(QuickSearch, 1);

	/* quick search */
	hbox_search = gtk_hbox_new(FALSE, 0);

	search_type_opt = gtk_option_menu_new();
	gtk_widget_show(search_type_opt);
	gtk_box_pack_start(GTK_BOX(hbox_search), search_type_opt, FALSE, FALSE, 0);

	search_type = gtk_menu_new();
	MENUITEM_ADD (search_type, menuitem, _("Subject"), QUICK_SEARCH_SUBJECT);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem, _("From"), QUICK_SEARCH_FROM);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem, _("To"), QUICK_SEARCH_TO);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem, _("Extended"), QUICK_SEARCH_EXTENDED);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(search_type_opt), search_type);
	
	gtk_option_menu_set_history(GTK_OPTION_MENU(search_type_opt), prefs_common.summary_quicksearch_type);
	
	gtk_widget_show(search_type);
	
	search_string_entry = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(hbox_search), search_string_entry, FALSE, FALSE, 2);
	gtk_combo_set_value_in_list(GTK_COMBO(search_string_entry), FALSE, TRUE);
	gtk_combo_set_case_sensitive(GTK_COMBO(search_string_entry), TRUE);
	if (prefs_common.summary_quicksearch_history) 
		gtk_combo_set_popdown_strings(GTK_COMBO(search_string_entry), 
			prefs_common.summary_quicksearch_history);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(search_string_entry)->entry), "");
	gtk_widget_show(search_string_entry);

	search_hbbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(search_hbbox), 	
				  GTK_BUTTONBOX_START);

	gtk_box_set_spacing(GTK_BOX(search_hbbox), 5);
		
	clear_search = gtk_button_new_with_label(_("Clear"));
	gtk_box_pack_start(GTK_BOX(search_hbbox), clear_search, 
			   TRUE, TRUE, 0);
	gtk_widget_set_size_request(clear_search, 120, -1);
	
	gtk_widget_show(clear_search);

	search_description = gtk_button_new_with_label(_("Extended Symbols"));
	gtk_box_pack_start(GTK_BOX(search_hbbox), search_description,
			   TRUE, TRUE, 0);
	gtk_widget_show(search_description);
		
	g_signal_connect(G_OBJECT(search_description), "clicked",
			 G_CALLBACK(search_description_cb), NULL);
	g_signal_connect(G_OBJECT(clear_search), "clicked",
			 G_CALLBACK(clear_search_cb), quicksearch);
	gtk_box_pack_start(GTK_BOX(hbox_search), search_hbbox, FALSE, FALSE, 2);				
	gtk_widget_show(search_hbbox);
	
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(search_string_entry)->entry), 
			   "key_press_event",
			   GTK_SIGNAL_FUNC(searchbar_pressed),
			   quicksearch);

	/*
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(search_string_entry)->entry), 
			   "focus_in_event",
			   GTK_SIGNAL_FUNC(searchbar_focus_evt),
			   quicksearch);

	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(search_string_entry)->entry), 
			   "focus_out_event",
			   GTK_SIGNAL_FUNC(searchbar_focus_evt),
			   quicksearch);
	*/

	quicksearch->hbox_search = hbox_search;
	quicksearch->search_type = search_type;
	quicksearch->search_type_opt = search_type_opt;
	quicksearch->search_string_entry = search_string_entry;
	quicksearch->search_description = search_description;
	quicksearch->matcher_list = NULL;
	quicksearch->active = FALSE;

	update_extended_button(quicksearch);
	
	return quicksearch;
}

GtkWidget *quicksearch_get_widget(QuickSearch *quicksearch)
{
	return quicksearch->hbox_search;
}

void quicksearch_show(QuickSearch *quicksearch)
{
	prepare_matcher(quicksearch);
	gtk_widget_show(quicksearch->hbox_search);
	update_extended_button(quicksearch);
}

void quicksearch_hide(QuickSearch *quicksearch)
{
	quicksearch->active = FALSE;
	gtk_widget_hide(quicksearch->hbox_search);
}

void quicksearch_set(QuickSearch *quicksearch, QuickSearchType type,
		     const gchar *matchstring)
{
	gtk_option_menu_set_history(GTK_OPTION_MENU(quicksearch->search_type_opt),
				    type);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry),
			   matchstring);
	prefs_common.summary_quicksearch_type = type;

	prepare_matcher(quicksearch);

	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);	
}

gboolean quicksearch_is_active(QuickSearch *quicksearch)
{
	return quicksearch->active;
}

void quicksearch_set_execute_callback(QuickSearch *quicksearch,
				      QuickSearchExecuteCallback callback,
				      gpointer data)
{
	quicksearch->callback = callback;
	quicksearch->callback_data = data;
}

gboolean quicksearch_match(QuickSearch *quicksearch, MsgInfo *msginfo)
{
	gchar *searched_header = NULL;

	if (!quicksearch->active)
		return TRUE;

	switch (prefs_common.summary_quicksearch_type) {
	case QUICK_SEARCH_SUBJECT:
		searched_header = msginfo->subject;
		break;
	case QUICK_SEARCH_FROM:
		searched_header = msginfo->from;
		break;
	case QUICK_SEARCH_TO:
		searched_header = msginfo->to;
		break;
	case QUICK_SEARCH_EXTENDED:
		break;
	default:
		debug_print("unknown search type (%d)\n", prefs_common.summary_quicksearch_type);
		break;
	}

	if (prefs_common.summary_quicksearch_type != QUICK_SEARCH_EXTENDED && quicksearch->search_string &&
            searched_header && strcasestr(searched_header, quicksearch->search_string) != NULL)
		return TRUE;
	else if ((quicksearch->matcher_list != NULL) && matcherlist_match(quicksearch->matcher_list, msginfo))
		return TRUE;

	return FALSE;
}
