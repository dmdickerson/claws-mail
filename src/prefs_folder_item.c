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

/* alfons - all folder item specific settings should migrate into 
 * folderlist.xml!!! the old folderitemrc file will only serve for a few 
 * versions (for compatibility) */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "intl.h"
#include "defs.h"
#include "folder.h"
#include "prefs_folder_item.h"
#include "folderview.h"
#include "summaryview.h"
#include "menu.h"
#include "account.h"
#include "prefs.h"
#include "manage_window.h"
#include "utils.h"
#include "addr_compl.h"
#include "prefs_scoring.h"
#include "gtkutils.h"
#include "filtering.h"

PrefsFolderItem tmp_prefs;

struct PrefsFolderItemDialog
{
	FolderView *folderview;
	FolderItem *item;
	GtkWidget *window;
	GtkWidget *checkbtn_request_return_receipt;
	GtkWidget *checkbtn_save_copy_to_folder;
	GtkWidget *checkbtn_default_to;
	GtkWidget *entry_default_to;
	GtkWidget *checkbtn_default_reply_to;
	GtkWidget *entry_default_reply_to;
	GtkWidget *checkbtn_simplify_subject;
	GtkWidget *entry_simplify_subject;
	GtkWidget *checkbtn_folder_chmod;
	GtkWidget *entry_folder_chmod;
	GtkWidget *checkbtn_enable_default_account;
	GtkWidget *optmenu_default_account;
	GtkWidget *folder_color;
	GtkWidget *folder_color_btn;
};

static GtkWidget *color_dialog;

static PrefParam param[] = {
	{"sort_by_number", "FALSE", &tmp_prefs.sort_by_number, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_size", "FALSE", &tmp_prefs.sort_by_size, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_date", "FALSE", &tmp_prefs.sort_by_date, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_from", "FALSE", &tmp_prefs.sort_by_from, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_subject", "FALSE", &tmp_prefs.sort_by_subject, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_score", "FALSE", &tmp_prefs.sort_by_score, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_descending", "FALSE", &tmp_prefs.sort_descending, P_BOOL,
	 NULL, NULL, NULL},
	/*{"enable_thread", "TRUE", &tmp_prefs.enable_thread, P_BOOL,
	 NULL, NULL, NULL},*/
	{"hide_score", "-9999", &tmp_prefs.kill_score, P_INT,
	 NULL, NULL, NULL},
	{"important_score", "1", &tmp_prefs.important_score, P_INT,
	 NULL, NULL, NULL},
	/* MIGRATION */	 
	{"request_return_receipt", "", &tmp_prefs.request_return_receipt, P_BOOL,
	 NULL, NULL, NULL},
	{"enable_default_to", "", &tmp_prefs.enable_default_to, P_BOOL,
	 NULL, NULL, NULL},
	{"default_to", "", &tmp_prefs.default_to, P_STRING,
	 NULL, NULL, NULL},
	{"enable_default_reply_to", "", &tmp_prefs.enable_default_reply_to, P_BOOL,
	 NULL, NULL, NULL},
	{"default_reply_to", "", &tmp_prefs.default_reply_to, P_STRING,
	 NULL, NULL, NULL},
	{"enable_simplify_subject", "", &tmp_prefs.enable_simplify_subject, P_BOOL,
	 NULL, NULL, NULL},
	{"simplify_subject_regexp", "", &tmp_prefs.simplify_subject_regexp, P_STRING,
	 NULL, NULL, NULL},
	{"enable_folder_chmod", "", &tmp_prefs.enable_folder_chmod, P_BOOL,
	 NULL, NULL, NULL},
	{"folder_chmod", "", &tmp_prefs.folder_chmod, P_INT,
	 NULL, NULL, NULL},
	{"enable_default_account", "", &tmp_prefs.enable_default_account, P_BOOL,
	 NULL, NULL, NULL},
	{"default_account", NULL, &tmp_prefs.default_account, P_INT,
	 NULL, NULL, NULL},
	{"save_copy_to_folder", NULL, &tmp_prefs.save_copy_to_folder, P_BOOL,
	 NULL, NULL, NULL},
	{"folder_color", "", &tmp_prefs.color, P_INT,
	 NULL, NULL, NULL},
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

void prefs_folder_item_delete_cb		(GtkWidget *widget, GdkEventAny *event, 
						 struct PrefsFolderItemDialog *dialog);
void prefs_folder_item_cancel_cb		(GtkWidget *widget, 
						 struct PrefsFolderItemDialog *dialog);
void prefs_folder_item_ok_cb			(GtkWidget *widget, 
						 struct PrefsFolderItemDialog *dialog);
gint prefs_folder_item_chmod_mode		(gchar *folder_chmod);

static void set_button_color(guint rgbvalue, GtkWidget *button);
static void folder_color_set_dialog(GtkWidget *widget, gpointer data);
static void folder_color_set_dialog_ok(GtkWidget *widget, gpointer data);
static void folder_color_set_dialog_cancel(GtkWidget *widget, gpointer data);
static void folder_color_set_dialog_key_pressed(GtkWidget *widget,
						GdkEventKey *event,
						gpointer data);


void prefs_folder_item_read_config(FolderItem * item)
{
	gchar * id;

	id = folder_item_get_identifier(item);

	tmp_prefs.scoring = NULL;
	tmp_prefs.processing = NULL;
	prefs_read_config(param, id, FOLDERITEM_RC);
	g_free(id);

	*item->prefs = tmp_prefs;

	/*
	 * MIGRATION: next lines are migration code. the idea is that
	 *            if used regularly, claws folder config ends up
	 *            in the same file as sylpheed-main
	 */

	item->ret_rcpt = tmp_prefs.request_return_receipt ? TRUE : FALSE;

	/* MIGRATION: 0.7.8main+ has persistent sort order. claws had the sort
	 *	      order in different members, which is ofcourse a little
	 *	      bit phoney. */
	if (item->sort_key == SORT_BY_NONE) {
		item->sort_key  = (tmp_prefs.sort_by_number  ? SORT_BY_NUMBER  :
				   tmp_prefs.sort_by_size    ? SORT_BY_SIZE    :
				   tmp_prefs.sort_by_date    ? SORT_BY_DATE    :
				   tmp_prefs.sort_by_from    ? SORT_BY_FROM    :
				   tmp_prefs.sort_by_subject ? SORT_BY_SUBJECT :
				   tmp_prefs.sort_by_score   ? SORT_BY_SCORE   :
								 SORT_BY_NONE);
		item->sort_type = tmp_prefs.sort_descending ? SORT_DESCENDING : SORT_ASCENDING;
	}								
}

void prefs_folder_item_save_config(FolderItem * item)
{	
	gchar * id;

	tmp_prefs = * item->prefs;

	id = folder_item_get_identifier(item);

	prefs_save_config(param, id, FOLDERITEM_RC);
	g_free(id);

	/* MIGRATION: make sure migrated items are not saved
	 */
}

void prefs_folder_item_set_config(FolderItem * item,
				  int sort_type, gint sort_mode)
{
	g_assert(item);
	g_warning("prefs_folder_item_set_config() should never be called\n");
	item->sort_key  = sort_type;
	item->sort_type = sort_mode;
}

PrefsFolderItem * prefs_folder_item_new(void)
{
	PrefsFolderItem * prefs;

	prefs = g_new0(PrefsFolderItem, 1);

	tmp_prefs.sort_by_number = FALSE;
	tmp_prefs.sort_by_size = FALSE;
	tmp_prefs.sort_by_date = FALSE;
	tmp_prefs.sort_by_from = FALSE;
	tmp_prefs.sort_by_subject = FALSE;
	tmp_prefs.sort_by_score = FALSE;
	tmp_prefs.sort_descending = FALSE;
	tmp_prefs.kill_score = -9999;
	tmp_prefs.important_score = 9999;

	tmp_prefs.request_return_receipt = FALSE;
	tmp_prefs.enable_default_to = FALSE;
	tmp_prefs.default_to = NULL;
	tmp_prefs.enable_default_reply_to = FALSE;
	tmp_prefs.default_reply_to = NULL;
	tmp_prefs.enable_simplify_subject = FALSE;
	tmp_prefs.simplify_subject_regexp = NULL;
	tmp_prefs.enable_folder_chmod = FALSE;
	tmp_prefs.folder_chmod = 0;
	tmp_prefs.enable_default_account = FALSE;
	tmp_prefs.default_account = 0;
	tmp_prefs.save_copy_to_folder = FALSE;
	tmp_prefs.color = 0;

	tmp_prefs.scoring = NULL;
	tmp_prefs.processing = NULL;

	* prefs = tmp_prefs;
	
	return prefs;
}

void prefs_folder_item_free(PrefsFolderItem * prefs)
{
	if (prefs->default_to) 
		g_free(prefs->default_to);
	if (prefs->default_reply_to) 
		g_free(prefs->default_reply_to);
	if (prefs->scoring != NULL)
		prefs_scoring_free(prefs->scoring);
	g_free(prefs);
}

gint prefs_folder_item_get_sort_mode(FolderItem * item)
{
	g_assert(item != NULL);
	g_warning("prefs_folder_item_get_sort_mode() should never be called\n");
	return item->sort_key;
}

gint prefs_folder_item_get_sort_type(FolderItem * item)
{
	g_assert(item != NULL);
	g_warning("prefs_folder_item_get_sort_type() should never be called\n");
	return item->sort_type;
}

#define SAFE_STRING(str) \
	(str) ? (str) : ""

void prefs_folder_item_create(void *folderview, FolderItem *item) 
{
	struct PrefsFolderItemDialog *dialog;
	guint rowcount;
	gchar *folder_identifier, *infotext;

	GtkWidget *window;
	GtkWidget *table;
	GtkWidget *infolabel;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;
	
	GtkWidget *checkbtn_request_return_receipt;
	GtkWidget *checkbtn_save_copy_to_folder;
	GtkWidget *checkbtn_default_to;
	GtkWidget *entry_default_to;
	GtkWidget *checkbtn_default_reply_to;
	GtkWidget *entry_default_reply_to;
	GtkWidget *checkbtn_simplify_subject;
	GtkWidget *entry_simplify_subject;
	GtkWidget *checkbtn_folder_chmod;
	GtkWidget *entry_folder_chmod;
	GtkWidget *checkbtn_enable_default_account;
	GtkWidget *optmenu_default_account;
	GtkWidget *optmenu_default_account_menu;
	GtkWidget *optmenu_default_account_menuitem;
	GtkWidget *folder_color;
	GtkWidget *folder_color_btn;
	GList *cur_ac;
	GList *account_list;
	PrefsAccount *ac_prefs;
	GtkOptionMenu *optmenu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gint account_index, index;

	dialog = g_new0(struct PrefsFolderItemDialog, 1);
	dialog->folderview = folderview;
	dialog->item	   = item;

	/* Window */
	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_window_set_title (GTK_WINDOW(window),
			      _("Folder Properties"));
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_folder_item_delete_cb), dialog);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);

	/* Table */
	table = gtk_table_new(8, 2, FALSE);
	gtk_widget_show(table);
	gtk_table_set_row_spacings(GTK_TABLE(table), VSPACING_NARROW);
	gtk_container_add(GTK_CONTAINER (window), table);
	rowcount = 0;

	/* Label */
	folder_identifier = folder_item_get_identifier(item);
	infotext = g_strconcat(_("Folder Properties for "), folder_identifier, NULL);
	infolabel = gtk_label_new(infotext);
	gtk_table_attach(GTK_TABLE(table), infolabel, 0, 2, rowcount, 
			 rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_widget_show(infolabel);
	rowcount++;

	/* Request Return Receipt */
	checkbtn_request_return_receipt = gtk_check_button_new_with_label
		(_("Request Return Receipt"));
	gtk_widget_show(checkbtn_request_return_receipt);
	gtk_table_attach(GTK_TABLE(table), checkbtn_request_return_receipt, 
			 0, 2, rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, 
			 GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_request_return_receipt),
				     item->ret_rcpt ? TRUE : FALSE);

	rowcount++;

	/* Save Copy to Folder */
	checkbtn_save_copy_to_folder = gtk_check_button_new_with_label
		(_("Save copy of outgoing messages to this folder instead of Sent"));
	gtk_widget_show(checkbtn_save_copy_to_folder);
	gtk_table_attach(GTK_TABLE(table), checkbtn_save_copy_to_folder, 0, 2, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_save_copy_to_folder),
				     item->prefs->save_copy_to_folder ? TRUE : FALSE);

	rowcount++;

	/* Default To */
	checkbtn_default_to = gtk_check_button_new_with_label(_("Default To: "));
	gtk_widget_show(checkbtn_default_to);
	gtk_table_attach(GTK_TABLE(table), checkbtn_default_to, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_default_to), 
				     item->prefs->enable_default_to);

	entry_default_to = gtk_entry_new();
	gtk_widget_show(entry_default_to);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_default_to, 1, 2, rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_default_to, entry_default_to);
	gtk_entry_set_text(GTK_ENTRY(entry_default_to), SAFE_STRING(item->prefs->default_to));
	address_completion_register_entry(GTK_ENTRY(entry_default_to));

	rowcount++;

	/* Default Reply-To */
	checkbtn_default_reply_to = gtk_check_button_new_with_label(_("Default Reply-To: "));
	gtk_widget_show(checkbtn_default_reply_to);
	gtk_table_attach(GTK_TABLE(table), checkbtn_default_reply_to, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_default_reply_to), 
				     item->prefs->enable_default_reply_to);

	entry_default_reply_to = gtk_entry_new();
	gtk_widget_show(entry_default_reply_to);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_default_reply_to, 1, 2, rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_default_reply_to, entry_default_reply_to);
	gtk_entry_set_text(GTK_ENTRY(entry_default_reply_to), SAFE_STRING(item->prefs->default_reply_to));
	address_completion_register_entry(GTK_ENTRY(entry_default_reply_to));

	rowcount++;

	/* Simplify Subject */
	checkbtn_simplify_subject = gtk_check_button_new_with_label(_("Simplify Subject RegExp: "));
	gtk_widget_show(checkbtn_simplify_subject);
	gtk_table_attach(GTK_TABLE(table), checkbtn_simplify_subject, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_simplify_subject), 
				     item->prefs->enable_simplify_subject);

	entry_simplify_subject = gtk_entry_new();
	gtk_widget_show(entry_simplify_subject);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_simplify_subject, 1, 2, 
				  rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_simplify_subject, entry_simplify_subject);
	gtk_entry_set_text(GTK_ENTRY(entry_simplify_subject), 
			   SAFE_STRING(item->prefs->simplify_subject_regexp));

	rowcount++;

	/* Folder chmod */
	checkbtn_folder_chmod = gtk_check_button_new_with_label(_("Folder chmod: "));
	gtk_widget_show(checkbtn_folder_chmod);
	gtk_table_attach(GTK_TABLE(table), checkbtn_folder_chmod, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_chmod), 
				     item->prefs->enable_folder_chmod);

	entry_folder_chmod = gtk_entry_new();
	gtk_widget_show(entry_folder_chmod);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_folder_chmod, 1, 2, 
				  rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_folder_chmod, entry_folder_chmod);
	if (item->prefs->folder_chmod) {
		gchar *buf;

		buf = g_strdup_printf("%o", item->prefs->folder_chmod);
		gtk_entry_set_text(GTK_ENTRY(entry_folder_chmod), buf);
		g_free(buf);
	}
	
	rowcount++;

	/* Default account */
	checkbtn_enable_default_account = gtk_check_button_new_with_label(_("Default account: "));
	gtk_widget_show(checkbtn_enable_default_account);
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_default_account, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_default_account), 
				     item->prefs->enable_default_account);

 	optmenu_default_account = gtk_option_menu_new ();
 	gtk_widget_show (optmenu_default_account);
	gtk_table_attach_defaults(GTK_TABLE(table), optmenu_default_account, 1, 2, 
				  rowcount, rowcount + 1);
 	optmenu_default_account_menu = gtk_menu_new ();

	account_list = account_get_list();
	account_index = 0;
	index = 0;
	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;
	 	MENUITEM_ADD (optmenu_default_account_menu, optmenu_default_account_menuitem,
					ac_prefs->account_name?ac_prefs->account_name : _("Untitled"),
					ac_prefs->account_id);
		/* get the index for menu's set_history (sad method?) */
		if (ac_prefs->account_id == item->prefs->default_account)
			account_index = index;
		index++;			
	}

	dialog->item->prefs->default_account = item->prefs->default_account;

	optmenu=GTK_OPTION_MENU(optmenu_default_account);
 	gtk_option_menu_set_menu(optmenu, optmenu_default_account_menu);

	gtk_option_menu_set_history(optmenu, account_index);

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));

	SET_TOGGLE_SENSITIVITY(checkbtn_enable_default_account, optmenu_default_account);

	rowcount++;

	/* Folder color */
	folder_color = gtk_label_new(_("Folder color: "));
	gtk_misc_set_alignment(GTK_MISC(folder_color), 0, 0.5);
	gtk_widget_show(folder_color);
	gtk_table_attach_defaults(GTK_TABLE(table), folder_color, 0, 1, 
			 rowcount, rowcount + 1);

	folder_color_btn = gtk_button_new_with_label("");
	gtk_widget_set_usize(folder_color_btn, 36, 26);
	gtk_container_set_border_width(GTK_CONTAINER(folder_color_btn), 2);
	gtk_widget_show(folder_color_btn);
	gtk_table_attach(GTK_TABLE(table), folder_color_btn,
			 1, 2, rowcount, rowcount + 1,
			 GTK_SHRINK, 0, 0, 0);

	dialog->item->prefs->color = item->prefs->color;

	gtk_signal_connect(GTK_OBJECT(folder_color_btn), "clicked",
			   GTK_SIGNAL_FUNC(folder_color_set_dialog),
			   dialog);

	set_button_color(item->prefs->color, folder_color_btn);

	rowcount++;

	/* Ok and Cancle Buttons */
	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_table_attach_defaults(GTK_TABLE(table), confirm_area, 0, 2, 
				  rowcount, rowcount + 1);
	gtk_widget_grab_default(ok_btn);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_folder_item_ok_cb), dialog);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_folder_item_cancel_cb), dialog);

	dialog->window = window;
	dialog->checkbtn_request_return_receipt = checkbtn_request_return_receipt;
	dialog->checkbtn_save_copy_to_folder = checkbtn_save_copy_to_folder;
	dialog->checkbtn_default_to = checkbtn_default_to;
	dialog->entry_default_to = entry_default_to;
	dialog->checkbtn_default_reply_to = checkbtn_default_reply_to;
	dialog->entry_default_reply_to = entry_default_reply_to;
	dialog->checkbtn_simplify_subject = checkbtn_simplify_subject;
	dialog->entry_simplify_subject = entry_simplify_subject;
	dialog->checkbtn_folder_chmod = checkbtn_folder_chmod;
	dialog->entry_folder_chmod = entry_folder_chmod;
	dialog->checkbtn_enable_default_account = checkbtn_enable_default_account;
	dialog->optmenu_default_account = optmenu_default_account;
	dialog->folder_color = folder_color;
	dialog->folder_color_btn = folder_color_btn;

	g_free(infotext);

	address_completion_start(window);

	gtk_widget_show(window);
}

void prefs_folder_item_destroy(struct PrefsFolderItemDialog *dialog) 
{
	address_completion_unregister_entry(GTK_ENTRY(dialog->entry_default_to));
	address_completion_unregister_entry(GTK_ENTRY(dialog->entry_default_reply_to));
	address_completion_end(dialog->window);
	gtk_widget_destroy(dialog->window);
	g_free(dialog);
}

void prefs_folder_item_cancel_cb(GtkWidget *widget, 
				 struct PrefsFolderItemDialog *dialog) 
{
	prefs_folder_item_destroy(dialog);
}

void prefs_folder_item_delete_cb(GtkWidget *widget, GdkEventAny *event, 
				 struct PrefsFolderItemDialog *dialog) 
{
	prefs_folder_item_destroy(dialog);
}

void prefs_folder_item_ok_cb(GtkWidget *widget, 
			     struct PrefsFolderItemDialog *dialog) 
{
	gchar *buf;
	PrefsFolderItem *prefs = dialog->item->prefs;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gboolean   old_simplify_val;
	gchar     *old_simplify_str;

	g_return_if_fail(prefs != NULL);

	old_simplify_val = prefs->enable_simplify_subject;
	old_simplify_str = prefs->simplify_subject_regexp;

	prefs->request_return_receipt = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_request_return_receipt));
	/* MIGRATION */    
	dialog->item->ret_rcpt = prefs->request_return_receipt;

	prefs->save_copy_to_folder = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_save_copy_to_folder));

	prefs->enable_default_to = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_default_to));
	g_free(prefs->default_to);
	prefs->default_to = 
	    gtk_editable_get_chars(GTK_EDITABLE(dialog->entry_default_to), 0, -1);

	prefs->enable_default_reply_to = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_default_reply_to));
	g_free(prefs->default_reply_to);
	prefs->default_reply_to = 
	    gtk_editable_get_chars(GTK_EDITABLE(dialog->entry_default_reply_to), 0, -1);

	prefs->enable_simplify_subject =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_simplify_subject));
	prefs->simplify_subject_regexp = 
	    gtk_editable_get_chars(GTK_EDITABLE(dialog->entry_simplify_subject), 0, -1);
	
	if (dialog->item == dialog->folderview->summaryview->folder_item &&
	    (prefs->enable_simplify_subject != old_simplify_val ||  
	    0 != strcmp2(prefs->simplify_subject_regexp, old_simplify_str))) 
		summary_show(dialog->folderview->summaryview, dialog->item);
		
	if (old_simplify_str) g_free(old_simplify_str);

	prefs->enable_folder_chmod = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_folder_chmod));
	buf = gtk_editable_get_chars(GTK_EDITABLE(dialog->entry_folder_chmod), 0, -1);
	prefs->folder_chmod = prefs_folder_item_chmod_mode(buf);
	g_free(buf);

 	prefs->enable_default_account = 
 	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->checkbtn_enable_default_account));
 	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(dialog->optmenu_default_account));
 	menuitem = gtk_menu_get_active(GTK_MENU(menu));
 	prefs->default_account = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(menuitem)));

	prefs->color = dialog->item->prefs->color;
	/* update folder view */
	if (prefs->color > 0)
		folderview_update_item(dialog->item, FALSE);

	prefs_folder_item_save_config(dialog->item);
	prefs_folder_item_destroy(dialog);
}

gint prefs_folder_item_chmod_mode(gchar *folder_chmod) 
{
	gint newmode = 0;
	gchar *tmp;

	if (folder_chmod) {
		newmode = strtol(folder_chmod, &tmp, 8);
		if (!(*(folder_chmod) && !(*tmp)))
			newmode = 0;
	}

	return newmode;
}

static void set_button_color(guint rgbvalue, GtkWidget *button)
{
	GtkStyle *newstyle;
	GdkColor gdk_color;

	gtkut_convert_int_to_gdk_color(rgbvalue, &gdk_color);
	newstyle = gtk_style_copy(gtk_widget_get_default_style());
	newstyle->bg[GTK_STATE_NORMAL]   = gdk_color;
	newstyle->bg[GTK_STATE_PRELIGHT] = gdk_color;
	newstyle->bg[GTK_STATE_ACTIVE]   = gdk_color;
	gtk_widget_set_style(GTK_WIDGET(button), newstyle);
}

static void folder_color_set_dialog(GtkWidget *widget, gpointer data)
{
	struct PrefsFolderItemDialog *folder_dialog = data;
	GtkColorSelectionDialog *dialog;
	gdouble color[4] = {0.0, 0.0, 0.0, 0.0};
	guint rgbcolor;

	color_dialog = gtk_color_selection_dialog_new(_("Pick color for folder"));
	gtk_window_set_position(GTK_WINDOW(color_dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);
	gtk_window_set_policy(GTK_WINDOW(color_dialog), FALSE, FALSE, FALSE);
	manage_window_set_transient(GTK_WINDOW(color_dialog));

	gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(folder_color_set_dialog_ok), data);
	gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->cancel_button),
			   "clicked", GTK_SIGNAL_FUNC(folder_color_set_dialog_cancel), data);
	gtk_signal_connect(GTK_OBJECT(color_dialog), "key_press_event",
			   GTK_SIGNAL_FUNC(folder_color_set_dialog_key_pressed),
			   data);

	rgbcolor = folder_dialog->item->prefs->color;
	color[0] = (gdouble) ((rgbcolor & 0xff0000) >> 16) / 255.0;
	color[1] = (gdouble) ((rgbcolor & 0x00ff00) >>  8) / 255.0;
	color[2] = (gdouble)  (rgbcolor & 0x0000ff)        / 255.0;

	dialog = GTK_COLOR_SELECTION_DIALOG(color_dialog);
	gtk_color_selection_set_color(GTK_COLOR_SELECTION(dialog->colorsel), color);

	gtk_widget_show(color_dialog);
}

static void folder_color_set_dialog_ok(GtkWidget *widget, gpointer data)
{
	struct PrefsFolderItemDialog *folder_dialog = data;
	GtkColorSelection *colorsel = (GtkColorSelection *)
				((GtkColorSelectionDialog *) color_dialog)->colorsel;
	gdouble color[4];
	guint red, green, blue, rgbvalue;

	gtk_color_selection_get_color(colorsel, color);

	red      = (guint) (color[0] * 255.0);
	green    = (guint) (color[1] * 255.0);
	blue     = (guint) (color[2] * 255.0);
	rgbvalue = (guint) ((red * 0x10000) | (green * 0x100) | blue);

	folder_dialog->item->prefs->color = rgbvalue;
	set_button_color(rgbvalue, folder_dialog->folder_color_btn);

	gtk_widget_destroy(color_dialog);
}

static void folder_color_set_dialog_cancel(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(color_dialog);
}

static void folder_color_set_dialog_key_pressed(GtkWidget *widget,
						GdkEventKey *event,
						gpointer data)
{
	gtk_widget_destroy(color_dialog);
}

void prefs_folder_item_copy_prefs(FolderItem * src, FolderItem * dest)
{
	GSList *tmp_prop_list = NULL, *tmp;
	prefs_folder_item_read_config(src);

	tmp_prefs.directory			= g_strdup(src->prefs->directory);
	tmp_prefs.sort_by_number		= src->prefs->sort_by_number;
	tmp_prefs.sort_by_size			= src->prefs->sort_by_size;
	tmp_prefs.sort_by_date			= src->prefs->sort_by_date;
	tmp_prefs.sort_by_from			= src->prefs->sort_by_from;
	tmp_prefs.sort_by_subject		= src->prefs->sort_by_subject;
	tmp_prefs.sort_by_score			= src->prefs->sort_by_score;
	tmp_prefs.sort_descending		= src->prefs->sort_descending;
	tmp_prefs.enable_thread			= src->prefs->enable_thread;
	tmp_prefs.kill_score			= src->prefs->kill_score;
	tmp_prefs.important_score		= src->prefs->important_score;
	/* FIXME!
	tmp_prefs.scoring			= g_slist_copy(src->prefs->scoring); 
	*/

	for (tmp = src->prefs->processing; tmp != NULL && tmp->data != NULL;) {
		FilteringProp *prop = (FilteringProp *)tmp->data;
		
		tmp_prop_list = g_slist_append(tmp_prop_list,
					   filteringprop_copy(prop));
		tmp = tmp->next;
	}
	tmp_prefs.processing			= tmp_prop_list;
	
	tmp_prefs.request_return_receipt	= src->prefs->request_return_receipt;
	tmp_prefs.enable_default_to		= src->prefs->enable_default_to;
	tmp_prefs.default_to			= g_strdup(src->prefs->default_to);
	tmp_prefs.enable_default_reply_to	= src->prefs->enable_default_reply_to;
	tmp_prefs.default_reply_to		= src->prefs->default_reply_to;
	tmp_prefs.enable_simplify_subject	= src->prefs->enable_simplify_subject;
	tmp_prefs.simplify_subject_regexp	= g_strdup(src->prefs->simplify_subject_regexp);
	tmp_prefs.enable_folder_chmod		= src->prefs->enable_folder_chmod;
	tmp_prefs.folder_chmod			= src->prefs->folder_chmod;
	tmp_prefs.enable_default_account	= src->prefs->enable_default_account;
	tmp_prefs.default_account		= src->prefs->default_account;
	tmp_prefs.save_copy_to_folder		= src->prefs->save_copy_to_folder;
	tmp_prefs.color				= src->prefs->color;

	*dest->prefs = tmp_prefs;
	prefs_folder_item_save_config(dest);
}
