/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "summary_search.h"
#include "inputdialog.h"
#include "grouplistdialog.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "procmsg.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_filtering.h"
#include "prefs_scoring.h"
#include "prefs_folder_item.h"
#include "account.h"
#include "folder.h"
#include "foldersel.h"
#include "inc.h"
#include "statusbar.h"
#include "hooks.h"

typedef enum
{
	COL_FOLDER	= 0,
	COL_NEW		= 1,
	COL_UNREAD	= 2,
	COL_TOTAL	= 3
} FolderColumnPos;

#define N_FOLDER_COLS		4
#define COL_FOLDER_WIDTH	150
#define COL_NUM_WIDTH		32

static GList *folderview_list = NULL;

static GdkFont *normalfont;
static GdkFont *boldfont;

static GtkStyle *normal_style;
static GtkStyle *normal_color_style;
static GtkStyle *bold_style;
static GtkStyle *bold_color_style;
static GtkStyle *bold_tgtfold_style;

static GdkBitmap *inboxxpm;
static GdkBitmap *inboxxpmmask;
static GdkPixmap *inboxhrmxpm;
static GdkBitmap *inboxhrmxpmmask;
static GdkPixmap *inboxopenxpm;
static GdkBitmap *inboxopenxpmmask;
static GdkPixmap *inboxopenhrmxpm;
static GdkBitmap *inboxopenhrmxpmmask;
static GdkPixmap *outboxxpm;
static GdkBitmap *outboxxpmmask;
static GdkPixmap *outboxhrmxpm;
static GdkBitmap *outboxhrmxpmmask;
static GdkPixmap *outboxopenxpm;
static GdkBitmap *outboxopenxpmmask;
static GdkPixmap *outboxopenhrmxpm;
static GdkBitmap *outboxopenhrmxpmmask;
static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;
static GdkPixmap *folderhrmxpm;
static GdkBitmap *folderhrmxpmmask;
static GdkPixmap *folderopenxpm;
static GdkBitmap *folderopenxpmmask;
static GdkPixmap *folderopenhrmxpm;
static GdkBitmap *folderopenhrmxpmmask;
static GdkPixmap *trashopenxpm;
static GdkBitmap *trashopenxpmmask;
static GdkPixmap *trashopenhrmxpm;
static GdkBitmap *trashopenhrmxpmmask;
static GdkPixmap *trashxpm;
static GdkBitmap *trashxpmmask;
static GdkPixmap *trashhrmxpm;
static GdkBitmap *trashhrmxpmmask;
static GdkPixmap *queuexpm;
static GdkBitmap *queuexpmmask;
static GdkPixmap *queuehrmxpm;
static GdkBitmap *queuehrmxpmmask;
static GdkPixmap *queueopenxpm;
static GdkBitmap *queueopenxpmmask;
static GdkPixmap *queueopenhrmxpm;
static GdkBitmap *queueopenhrmxpmmask;
static GdkPixmap *newxpm;
static GdkBitmap *newxpmmask;
static GdkPixmap *unreadxpm;
static GdkBitmap *unreadxpmmask;

static void folderview_select_node	 (FolderView	*folderview,
					  GtkCTreeNode	*node);
static void folderview_set_folders	 (FolderView	*folderview);
static void folderview_sort_folders	 (FolderView	*folderview,
					  GtkCTreeNode	*root,
					  Folder	*folder);
static void folderview_append_folder	 (FolderView	*folderview,
					  Folder	*folder);
static void folderview_update_node	 (FolderView	*folderview,
					  GtkCTreeNode	*node);

static GtkCTreeNode *folderview_find_by_name	(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 const gchar	*name);

static gint folderview_clist_compare	(GtkCList	*clist,
					 gconstpointer	 ptr1,
					 gconstpointer	 ptr2);
static gint folderview_compare_name	(gconstpointer	 a,
					 gconstpointer	 b);

/* callback functions */
static void folderview_button_pressed	(GtkWidget	*ctree,
					 GdkEventButton	*event,
					 FolderView	*folderview);
static void folderview_button_released	(GtkWidget	*ctree,
					 GdkEventButton	*event,
					 FolderView	*folderview);
static void folderview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 FolderView	*folderview);
static void folderview_selected		(GtkCTree	*ctree,
					 GtkCTreeNode	*row,
					 gint		 column,
					 FolderView	*folderview);
static void folderview_tree_expanded	(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 FolderView	*folderview);
static void folderview_tree_collapsed	(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 FolderView	*folderview);
static void folderview_popup_close	(GtkMenuShell	*menu_shell,
					 FolderView	*folderview);
static void folderview_col_resized	(GtkCList	*clist,
					 gint		 column,
					 gint		 width,
					 FolderView	*folderview);

static void folderview_update_tree_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void mark_all_read_cb            (FolderView    *folderview,
                                         guint           action,
                                         GtkWidget      *widget);
static void folderview_new_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_new_mbox_folder_cb(FolderView *folderview,
					  guint action,
					  GtkWidget *widget);
static void folderview_rename_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rename_mbox_folder_cb(FolderView *folderview,
					     guint action,
					     GtkWidget *widget);
static void folderview_delete_folder_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_remove_mailbox_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_new_imap_folder_cb(FolderView	*folderview,
					  guint		 action,
					  GtkWidget	*widget);
static void folderview_rm_imap_server_cb (FolderView	*folderview,
					  guint		 action,
					  GtkWidget	*widget);

static void folderview_new_news_group_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rm_news_group_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);
static void folderview_rm_news_server_cb(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_search_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_property_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview);
static void folderview_drag_leave_cb     (GtkWidget        *widget,
					  GdkDragContext   *context,
					  guint             time,
					  FolderView       *folderview);
static void folderview_drag_received_cb  (GtkWidget        *widget,
					  GdkDragContext   *drag_context,
					  gint              x,
					  gint              y,
					  GtkSelectionData *data,
					  guint             info,
					  guint             time,
					  FolderView       *folderview);
static void folderview_start_drag	 (GtkWidget *widget, gint button, GdkEvent *event,
			                  FolderView       *folderview);
static void folderview_drag_data_get     (GtkWidget        *widget,
					  GdkDragContext   *drag_context,
					  GtkSelectionData *selection_data,
					  guint             info,
					  guint             time,
					  FolderView       *folderview);

void folderview_create_folder_node       (FolderView       *folderview, 
					  FolderItem       *item);
gboolean folderview_update_item		 (gpointer 	    source,
					  gpointer	    data);

static void folderview_scoring_cb(FolderView *folderview, guint action,
				  GtkWidget *widget);
static void folderview_processing_cb(FolderView *folderview, guint action,
				     GtkWidget *widget);
static void folderview_move_to(FolderView *folderview, FolderItem *from_folder,
			       FolderItem *to_folder);
static void folderview_move_to_cb(FolderView *folderview);

static GtkItemFactoryEntry folderview_mbox_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_mbox_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_mbox_folder_cb, 0, NULL},
	{N_("/M_ove folder..."),	NULL, folderview_move_to_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _mailbox"),	NULL, folderview_remove_mailbox_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Properties..."),		NULL, NULL, 0, NULL},
	{N_("/_Processing..."),		NULL, folderview_processing_cb, 0, NULL},
	{N_("/_Scoring..."),		NULL, folderview_scoring_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_mail_popup_entries[] =
{
	{N_("/Mark all _read"),		NULL, mark_all_read_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/M_ove folder..."),	NULL, folderview_move_to_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _mailbox"),	NULL, folderview_remove_mailbox_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search folder..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL},
	{N_("/_Processing..."),		NULL, folderview_processing_cb, 0, NULL},
	{N_("/S_coring..."),		NULL, folderview_scoring_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_imap_popup_entries[] =
{
	{N_("/Mark all _read"),		NULL, mark_all_read_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Create _new folder..."),	NULL, folderview_new_imap_folder_cb, 0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb,   0, NULL},
	{N_("/M_ove folder..."),	NULL, folderview_move_to_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb,   0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/R_ebuild folder tree"),	NULL, folderview_update_tree_cb, 1, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _IMAP4 account"),	NULL, folderview_rm_imap_server_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search folder..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL},
	{N_("/_Processing..."),		NULL, folderview_processing_cb, 0, NULL},
	{N_("/S_coring..."),		NULL, folderview_scoring_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_news_popup_entries[] =
{
	{N_("/Mark all _read"),		NULL, mark_all_read_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Subscribe to newsgroup..."),
					NULL, folderview_new_news_group_cb, 0, NULL},
	{N_("/_Remove newsgroup"),	NULL, folderview_rm_news_group_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Check for new messages"),
					NULL, folderview_update_tree_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _news account"),	NULL, folderview_rm_news_server_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Search folder..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL},
	{N_("/_Processing..."),		NULL, folderview_processing_cb, 0, NULL},
	{N_("/S_coring..."),		NULL, folderview_scoring_cb, 0, NULL}
};


FolderView *folderview_create(void)
{
	FolderView *folderview;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	gchar *titles[N_FOLDER_COLS];
	GtkWidget *mail_popup;
	GtkWidget *news_popup;
	GtkWidget *imap_popup;
	GtkWidget *mbox_popup;
	GtkItemFactory *mail_factory;
	GtkItemFactory *news_factory;
	GtkItemFactory *imap_factory;
	GtkItemFactory *mbox_factory;
	gint n_entries;
	gint i;

	debug_print("Creating folder view...\n");
	folderview = g_new0(FolderView, 1);

	titles[COL_FOLDER] = _("Folder");
	titles[COL_NEW]    = _("New");
	titles[COL_UNREAD] = _("Unread");
	titles[COL_TOTAL]  = _("#");

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwin),
		 GTK_POLICY_AUTOMATIC,
		 prefs_common.folderview_vscrollbar_policy);
	gtk_widget_set_usize(scrolledwin,
			     prefs_common.folderview_width,
			     prefs_common.folderview_height);

	ctree = gtk_sctree_new_with_titles(N_FOLDER_COLS, COL_FOLDER, titles);
	
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
#ifndef CLAWS /* text instead of pixmaps */
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_NEW,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_UNREAD,
					   GTK_JUSTIFY_RIGHT);
#endif					   
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_TOTAL,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_FOLDER,
				   prefs_common.folder_col_folder);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_NEW,
				   prefs_common.folder_col_new);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_UNREAD,	
				   prefs_common.folder_col_unread);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_TOTAL,
				   prefs_common.folder_col_total);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(ctree), CTREE_INDENT);
	gtk_clist_set_compare_func(GTK_CLIST(ctree), folderview_clist_compare);

	/* don't let title buttons take key focus */
	for (i = 0; i < N_FOLDER_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[i].button,
				       GTK_CAN_FOCUS);

	/* popup menu */
	n_entries = sizeof(folderview_mail_popup_entries) /
		sizeof(folderview_mail_popup_entries[0]);
	mail_popup = menu_create_items(folderview_mail_popup_entries,
				       n_entries,
				       "<MailFolder>", &mail_factory,
				       folderview);
	n_entries = sizeof(folderview_imap_popup_entries) /
		sizeof(folderview_imap_popup_entries[0]);
	imap_popup = menu_create_items(folderview_imap_popup_entries,
				       n_entries,
				       "<IMAPFolder>", &imap_factory,
				       folderview);
	n_entries = sizeof(folderview_news_popup_entries) /
		sizeof(folderview_news_popup_entries[0]);
	news_popup = menu_create_items(folderview_news_popup_entries,
				       n_entries,
				       "<NewsFolder>", &news_factory,
				       folderview);
	n_entries = sizeof(folderview_mbox_popup_entries) /
		sizeof(folderview_mbox_popup_entries[0]);
	mbox_popup = menu_create_items(folderview_mbox_popup_entries,
				       n_entries,
				       "<MailFolder>", &mbox_factory,
				       folderview);

	gtk_signal_connect(GTK_OBJECT(ctree), "key_press_event",
			   GTK_SIGNAL_FUNC(folderview_key_pressed),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(ctree), "button_press_event",
			   GTK_SIGNAL_FUNC(folderview_button_pressed),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(ctree), "button_release_event",
			   GTK_SIGNAL_FUNC(folderview_button_released),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(ctree), "tree_select_row",
			   GTK_SIGNAL_FUNC(folderview_selected), folderview);
	gtk_signal_connect(GTK_OBJECT(ctree), "start_drag",
			   GTK_SIGNAL_FUNC(folderview_start_drag), folderview);
	gtk_signal_connect(GTK_OBJECT(ctree), "drag_data_get",
			   GTK_SIGNAL_FUNC(folderview_drag_data_get),
			   folderview);

	gtk_signal_connect_after(GTK_OBJECT(ctree), "tree_expand",
				 GTK_SIGNAL_FUNC(folderview_tree_expanded),
				 folderview);
	gtk_signal_connect_after(GTK_OBJECT(ctree), "tree_collapse",
				 GTK_SIGNAL_FUNC(folderview_tree_collapsed),
				 folderview);

	gtk_signal_connect(GTK_OBJECT(ctree), "resize_column",
			   GTK_SIGNAL_FUNC(folderview_col_resized),
			   folderview);

	gtk_signal_connect(GTK_OBJECT(mail_popup), "selection_done",
			   GTK_SIGNAL_FUNC(folderview_popup_close),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(imap_popup), "selection_done",
			   GTK_SIGNAL_FUNC(folderview_popup_close),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(news_popup), "selection_done",
			   GTK_SIGNAL_FUNC(folderview_popup_close),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(mbox_popup), "selection_done",
			   GTK_SIGNAL_FUNC(folderview_popup_close),
			   folderview);

        /* drop callback */
	gtk_drag_dest_set(ctree, GTK_DEST_DEFAULT_ALL &
			  ~GTK_DEST_DEFAULT_HIGHLIGHT,
			  summary_drag_types, 1,
			  GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_DEFAULT);
	gtk_signal_connect(GTK_OBJECT(ctree), "drag_motion",
			   GTK_SIGNAL_FUNC(folderview_drag_motion_cb),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(ctree), "drag_leave",
			   GTK_SIGNAL_FUNC(folderview_drag_leave_cb),
			   folderview);
	gtk_signal_connect(GTK_OBJECT(ctree), "drag_data_received",
			   GTK_SIGNAL_FUNC(folderview_drag_received_cb),
			   folderview);

	folderview->scrolledwin  = scrolledwin;
	folderview->ctree        = ctree;
	folderview->mail_popup   = mail_popup;
	folderview->mail_factory = mail_factory;
	folderview->imap_popup   = imap_popup;
	folderview->imap_factory = imap_factory;
	folderview->news_popup   = news_popup;
	folderview->news_factory = news_factory;
	folderview->mbox_popup   = mbox_popup;
	folderview->mbox_factory = mbox_factory;

	folderview->folder_item_update_callback_id =
		hooks_register_hook(FOLDER_ITEM_UPDATE_HOOKLIST, folderview_update_item, (gpointer) folderview);

	gtk_widget_show_all(scrolledwin);

	folderview_list = g_list_append(folderview_list, folderview);

	return folderview;
}

void folderview_init(FolderView *folderview)
{
	GtkWidget *ctree = folderview->ctree;
	GtkWidget *label_new;
	GtkWidget *label_unread;
	GtkWidget *hbox_new;
	GtkWidget *hbox_unread;

	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_CLOSE, &inboxxpm, &inboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_CLOSE_HRM, &inboxhrmxpm, &inboxhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_OPEN, &inboxopenxpm, &inboxopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_OPEN_HRM, &inboxopenhrmxpm, &inboxopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_CLOSE, &outboxxpm, &outboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_CLOSE_HRM, &outboxhrmxpm, &outboxhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_OPEN, &outboxopenxpm, &outboxopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_OPEN_HRM, &outboxopenhrmxpm, &outboxopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE, &folderxpm, &folderxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE_HRM, &folderhrmxpm, &folderhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN, &folderopenxpm, &folderopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN_HRM, &folderopenhrmxpm, &folderopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_OPEN, &trashopenxpm, &trashopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_OPEN_HRM, &trashopenhrmxpm, &trashopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_CLOSE, &trashxpm, &trashxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_CLOSE_HRM, &trashhrmxpm, &trashhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_CLOSE, &queuexpm, &queuexpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_CLOSE_HRM, &queuehrmxpm, &queuehrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_OPEN, &queueopenxpm, &queueopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_OPEN_HRM, &queueopenhrmxpm, &queueopenhrmxpmmask);

	/* CLAWS: titles for "New" and "Unread" show new & unread pixmaps
	 * instead text (text overflows making them unreadable and ugly) */
        stock_pixmap_gdk(ctree, STOCK_PIXMAP_NEW,
			 &newxpm, &newxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_UNREAD,
			 &unreadxpm, &unreadxpmmask);
		
	label_new = gtk_pixmap_new(newxpm, newxpmmask);
	label_unread = gtk_pixmap_new(unreadxpm, unreadxpmmask);

	hbox_new = gtk_hbox_new(FALSE, 4);
	hbox_unread = gtk_hbox_new(FALSE, 4);

	/* left justified */
	gtk_box_pack_start(GTK_BOX(hbox_new),label_new,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox_unread),label_unread,FALSE,FALSE,0);

	gtk_widget_show_all(hbox_new);
	gtk_widget_show_all(hbox_unread);

	gtk_clist_set_column_widget(GTK_CLIST(ctree),COL_NEW,hbox_new);
	gtk_clist_set_column_widget(GTK_CLIST(ctree),COL_UNREAD,hbox_unread);
			


	if (!normalfont)
#ifdef WIN32
		if (prefs_common.smallfont)
			normalfont = gtkut_font_load(prefs_common.normalfont);
		else
#endif
		normalfont = gtkut_font_load(NORMAL_FONT);
	if (!boldfont)
#ifdef WIN32
	if (prefs_common.smallfont)
		boldfont = gtkut_font_load(prefs_common.boldfont);
	else
#endif
		boldfont = gtkut_font_load(BOLD_FONT);

	if (!bold_style) {
		bold_style = gtk_style_copy(gtk_widget_get_style(ctree));
		bold_style->font = boldfont;
		bold_color_style = gtk_style_copy(bold_style);
		bold_color_style->fg[GTK_STATE_NORMAL] = folderview->color_new;

		bold_tgtfold_style = gtk_style_copy(bold_style);
		bold_tgtfold_style->fg[GTK_STATE_NORMAL] = folderview->color_op;
	}
	if (!normal_style) {
		normal_style = gtk_style_copy(gtk_widget_get_style(ctree));
		normal_style->font = normalfont;
		normal_color_style = gtk_style_copy(normal_style);
		normal_color_style->fg[GTK_STATE_NORMAL] = folderview->color_new;
	}
}

void folderview_set(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	MainWindow *mainwin = folderview->mainwin;

	debug_print("Setting folder info...\n");
	STATUSBAR_PUSH(mainwin, _("Setting folder info..."));

	main_window_cursor_wait(mainwin);

	folderview->selected = NULL;
	folderview->opened = NULL;

	gtk_clist_freeze(GTK_CLIST(ctree));
	gtk_clist_clear(GTK_CLIST(ctree));
	gtk_clist_thaw(GTK_CLIST(ctree));
	gtk_clist_freeze(GTK_CLIST(ctree));

	folderview_set_folders(folderview);

	gtk_clist_thaw(GTK_CLIST(ctree));
	main_window_cursor_normal(mainwin);
	STATUSBAR_POP(mainwin);
}

void folderview_set_all(void)
{
	GList *list;

	for (list = folderview_list; list != NULL; list = list->next)
		folderview_set((FolderView *)list->data);
}

void folderview_select(FolderView *folderview, FolderItem *item)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *old_selected = folderview->selected;

	if (!item) return;

	node = gtk_ctree_find_by_row_data(ctree, NULL, item);
	if (node) folderview_select_node(folderview, node);

	if (old_selected != node)
		folder_update_op_count();
}

static void mark_all_read_cb(FolderView *folderview, guint action,
                             GtkWidget *widget)
{
	if (folderview->selected)
		summary_mark_all_read(folderview->summaryview);
}

static void folderview_select_node(FolderView *folderview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	g_return_if_fail(node != NULL);

	folderview->open_folder = TRUE;
	gtkut_ctree_set_focus_row(ctree, node);
	gtk_ctree_select(ctree, node);
	if (folderview->summaryview->messages > 0)
		gtk_widget_grab_focus(folderview->summaryview->ctree);
	else
		gtk_widget_grab_focus(folderview->ctree);

	gtkut_ctree_expand_parent_all(ctree, node);
}

void folderview_unselect(FolderView *folderview)
{
	if (folderview->opened && !GTK_CTREE_ROW(folderview->opened)->children)
		gtk_ctree_collapse
			(GTK_CTREE(folderview->ctree), folderview->opened);

	folderview->selected = folderview->opened = NULL;
}

static GtkCTreeNode *folderview_find_next_unread(GtkCTree *ctree,
						 GtkCTreeNode *node)
{
	FolderItem *item;

	if (node)
		node = gtkut_ctree_node_next(ctree, node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item && item->unread > 0 && item->stype != F_TRASH)
			return node;
	}

	return NULL;
}

void folderview_select_next_unread(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node = NULL;

	if ((node = folderview_find_next_unread(ctree, folderview->opened))
	    != NULL) {
		folderview_select_node(folderview, node);
		return;
	}

	if (!folderview->opened ||
	    folderview->opened == GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list))
		return;
	/* search again from the first node */
	if ((node = folderview_find_next_unread(ctree, NULL)) != NULL)
		folderview_select_node(folderview, node);
}

void folderview_update_msg_num(FolderView *folderview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	static GtkCTreeNode *prev_row = NULL;
	FolderItem *item;
	gint new, unread, total;
	gchar *new_str, *unread_str, *total_str;

	if (!row) return;

	item = gtk_ctree_node_get_row_data(ctree, row);
	if (!item) return;

	gtk_ctree_node_get_text(ctree, row, COL_NEW, &new_str);
	gtk_ctree_node_get_text(ctree, row, COL_UNREAD, &unread_str);
	gtk_ctree_node_get_text(ctree, row, COL_TOTAL, &total_str);
	new = atoi(new_str);
	unread = atoi(unread_str);
	total = atoi(total_str);

	/* CLAWS: don't know why but this always seems to be true
	 * when deleting messages. Somewhere claws does a folder
	 * scan which sets all new, unread & total to the correct
	 * values. It then enters this function, but leaves it
	 * because new, unread and total are the same... */
#ifndef CLAWS	 
	if (prev_row     == row    &&
	    item->new    == new    &&
	    item->unread == unread &&
	    item->total  == total)
		return;
#endif		

	prev_row = row;

	folderview_update_node(folderview, row);
}

static void folderview_set_folders(FolderView *folderview)
{
	GList *list;

	list = folder_get_list();

	for (; list != NULL; list = list->next)
		folderview_append_folder(folderview, FOLDER(list->data));
}

static void folderview_scan_tree_func(Folder *folder, FolderItem *item,
				      gpointer data)
{
	GList *list;
	gchar *rootpath;

	if (FOLDER_IS_LOCAL(folder))
		rootpath = LOCAL_FOLDER(folder)->rootpath;
	else if (folder->type == F_IMAP && folder->account &&
		 folder->account->recv_server)
		rootpath = folder->account->recv_server;
	else
		return;

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		MainWindow *mainwin = folderview->mainwin;
		gchar *str;
#ifdef WIN32
		gchar *p_rootpath, *p_path;

		p_rootpath = g_strdup(rootpath);
		locale_to_utf8(&p_rootpath);

		if (item->path) {
			p_path = g_strdup(item->path);
			locale_to_utf8(&p_path);
			str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
					      p_rootpath, G_DIR_SEPARATOR,
					      p_path);
			g_free(p_path);
		}
		else
			str = g_strdup_printf(_("Scanning folder %s ..."),
					      p_rootpath);
		g_free(p_rootpath);
#else

		if (item->path)
			str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
					      rootpath, G_DIR_SEPARATOR,
					      item->path);
		else
			str = g_strdup_printf(_("Scanning folder %s ..."),
					      rootpath);
#endif

		STATUSBAR_PUSH(mainwin, str);
		STATUSBAR_POP(mainwin);
		g_free(str);
	}
}

static GtkWidget *label_window_create(const gchar *str)
{
	GtkWidget *window;
	GtkWidget *label;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(window, 380, 60);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(window), str);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	manage_window_set_transient(GTK_WINDOW(window));

	label = gtk_label_new(str);
	gtk_container_add(GTK_CONTAINER(window), label);
	gtk_widget_show(label);

	gtk_widget_show_now(window);

	return window;
}

void folderview_rescan_tree(Folder *folder)
{
	GtkWidget *window;

	g_return_if_fail(folder != NULL);

	if (!folder->scan_tree) return;

	inc_lock();
	window = label_window_create(_("Rebuilding folder tree..."));

	folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
	folder_scan_tree(folder);
	folder_set_ui_func(folder, NULL, NULL);

	folderview_set_all();

	folderview_check_new(folder);
	
	gtk_widget_destroy(window);
	inc_unlock();
}

#if 0
void folderview_rescan_all(void)
{
	GList *list;
	GtkWidget *window;

	inc_lock();
	window = label_window_create(_("Rescanning all folder trees..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (!folder->scan_tree) continue;
		folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
		folder_scan_tree(folder);
		folder_set_ui_func(folder, NULL, NULL);
	}

	folder_write_list();
	folderview_set_all();

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		folderview_check_new(folder);
	}

	gtk_widget_destroy(window);
	inc_unlock();
}
#endif

void folderview_check_new(Folder *folder)
{
	GList *list;
	FolderItem *item;
	FolderView *folderview;
	GtkCTree *ctree;
	GtkCTreeNode *node;

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		ctree = GTK_CTREE(folderview->ctree);

		inc_lock();
		main_window_lock(folderview->mainwin);
		gtk_widget_set_sensitive(folderview->ctree, FALSE);

		for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
		     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
			item = gtk_ctree_node_get_row_data(ctree, node);
			if (!item || !item->path || !item->folder) continue;
			if (item->no_select) continue;
			if (folder && folder != item->folder) continue;
			if (!folder && !FOLDER_IS_LOCAL(item->folder)) continue;

			folderview_scan_tree_func(item->folder, item, NULL);
			if (folder_item_scan(item) < 0) {
				if (folder && !FOLDER_IS_LOCAL(folder))
					break;
			}
			folderview_update_node(folderview, node);
		}

		gtk_widget_set_sensitive(folderview->ctree, TRUE);
		main_window_unlock(folderview->mainwin);
		inc_unlock();
	}

	folder_write_list();
}

void folderview_check_new_all(void)
{
	GList *list;
	GtkWidget *window;
	FolderView *folderview;

	folderview = (FolderView *)folderview_list->data;

	inc_lock();
	main_window_lock(folderview->mainwin);
	window = label_window_create
		(_("Checking for new messages in all folders..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		folderview_check_new(folder);
	}

	folder_write_list();
	folderview_set_all();

	gtk_widget_destroy(window);
	main_window_unlock(folderview->mainwin);
	inc_unlock();
}

static gboolean folderview_search_new_recursive(GtkCTree *ctree,
						GtkCTreeNode *node)
{
	FolderItem *item;

	if (node) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item) {
			if (item->new > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		node = GTK_CTREE_ROW(node)->children;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		if (folderview_search_new_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean folderview_have_new_children(FolderView *folderview,
					     GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	if (!node)
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	if (!node)
		return FALSE;

	node = GTK_CTREE_ROW(node)->children;

	while (node) {
		if (folderview_search_new_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean folderview_search_unread_recursive(GtkCTree *ctree,
						   GtkCTreeNode *node)
{
	FolderItem *item;

	if (node) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item) {
			if (item->unread > 0 ||
			    (item->stype == F_QUEUE && item->total > 0))
				return TRUE;
		}
		node = GTK_CTREE_ROW(node)->children;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		if (folderview_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean folderview_have_unread_children(FolderView *folderview,
						GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	if (!node)
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	if (!node)
		return FALSE;

	node = GTK_CTREE_ROW(node)->children;

	while (node) {
		if (folderview_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static void folderview_update_node(FolderView *folderview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkStyle *style = NULL;
	GtkStyle *color_style = NULL;
	FolderItem *item;
	GdkPixmap *xpm, *openxpm;
	GdkBitmap *mask, *openmask;
	gchar *name;
	gchar *str;
	gboolean add_unread_mark;
	gboolean use_bold, use_color;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);

	switch (item->stype) {
	case F_INBOX:
		if (item->hide_read_msgs) {
			xpm = inboxhrmxpm;
			mask = inboxhrmxpmmask;
			openxpm = inboxopenhrmxpm;
			openmask = inboxopenhrmxpmmask;
		} else {
			xpm = inboxxpm;
			mask = inboxxpmmask;
			openxpm = inboxopenxpm;
			openmask = inboxopenxpmmask;
		}
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, INBOX_DIR) ? _("Inbox") :
				item->name);
		break;
	case F_OUTBOX:
		if (item->hide_read_msgs) {
			xpm = outboxhrmxpm;
			mask = outboxhrmxpmmask;
			openxpm = outboxopenhrmxpm;
			openmask = outboxopenhrmxpmmask;
		} else {
			xpm = outboxxpm;
			mask = outboxxpmmask;
			openxpm = outboxopenxpm;
			openmask = outboxopenxpmmask;
		}
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, OUTBOX_DIR) ? _("Sent") :
				item->name);
		break;
	case F_QUEUE:
		if (item->hide_read_msgs) {
			xpm = queuehrmxpm;
			mask = queuehrmxpmmask;
			openxpm = queueopenhrmxpm;
			openmask = queueopenhrmxpmmask;
		} else {
			xpm = queuexpm;
			mask = queuexpmmask;
			openxpm = queueopenxpm;
			openmask = queueopenxpmmask;
		}
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, QUEUE_DIR) ? _("Queue") :
				item->name);
		break;
	case F_TRASH:
		if (item->hide_read_msgs) {
			xpm = trashhrmxpm;
			mask = trashhrmxpmmask;
			openxpm = trashopenhrmxpm;
			openmask = trashopenhrmxpmmask;
		} else {
			xpm = trashxpm;
			mask = trashxpmmask;
			openxpm = trashopenxpm;
			openmask = trashopenxpmmask;
		}
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, TRASH_DIR) ? _("Trash") :
				item->name);
		break;
	case F_DRAFT:
		xpm = folderxpm;
		mask = folderxpmmask;
		if (item->hide_read_msgs) {
			openxpm = folderopenhrmxpm;
			openmask = folderopenhrmxpmmask;
		} else {
			openxpm = folderopenxpm;
			openmask = folderopenxpmmask;
		}
		name = g_strdup(FOLDER_IS_LOCAL(item->folder) &&
				!strcmp2(item->name, DRAFT_DIR) ? _("Drafts") :
				item->name);
		break;
	default:
		if (item->hide_read_msgs) {
			xpm = folderhrmxpm;
			mask = folderhrmxpmmask;
			openxpm = folderopenhrmxpm;
			openmask = folderopenhrmxpmmask;
		} else {
			xpm = folderxpm;
			mask = folderxpmmask;
			openxpm = folderopenxpm;
			openmask = folderopenxpmmask;
		}
		if (!item->parent) {
			switch (item->folder->type) {
			case F_MH:
				name = " (MH)"; break;
			case F_MBOX:
				name = " (mbox)"; break;
			case F_IMAP:
				name = " (IMAP4)"; break;
			case F_NEWS:
				name = " (News)"; break;
			default:
				name = "";
			}
			name = g_strconcat(item->name, name, NULL);
		} else {
			if (item->folder->type == F_NEWS &&
			    item->path &&
			    !strcmp2(item->name, item->path) &&
			    prefs_common.ng_abbrev_len < strlen(item->path))
				name = get_abbrev_newsgroup_name(item->path, prefs_common.ng_abbrev_len);
			else
				name = g_strdup(item->name);
		}
	}

	if (!GTK_CTREE_ROW(node)->expanded &&
	    folderview_have_unread_children(folderview, node))
		add_unread_mark = TRUE;
	else
		add_unread_mark = FALSE;

	if (item->stype == F_QUEUE && item->total > 0 &&
	    prefs_common.display_folder_unread) {
		str = g_strdup_printf("%s (%d%s)", name, item->total,
				      add_unread_mark ? "+" : "");
		gtk_ctree_set_node_info(ctree, node, str, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
		g_free(str);
	} else if ((item->unread > 0 || add_unread_mark) &&
		 prefs_common.display_folder_unread) {

		if (item->unread > 0)
			str = g_strdup_printf("%s (%d%s%s)", name, item->unread,
					      add_unread_mark ? "+" : "", 
				              item->unreadmarked > 0 ? "!":"");
		else
			str = g_strdup_printf("%s (+)", name);
		gtk_ctree_set_node_info(ctree, node, str, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
		g_free(str);
	} else {
		str = g_strdup_printf("%s%s", name, 
			              item->unreadmarked > 0 ? " (!)":"");
	
		gtk_ctree_set_node_info(ctree, node, str, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
		g_free(str);
	}
	g_free(name);

	if (!item->parent) {
		gtk_ctree_node_set_text(ctree, node, COL_NEW,    "-");
		gtk_ctree_node_set_text(ctree, node, COL_UNREAD, "-");
		gtk_ctree_node_set_text(ctree, node, COL_TOTAL,  "-");
	} else {
		gtk_ctree_node_set_text(ctree, node, COL_NEW,    itos(item->new));
		gtk_ctree_node_set_text(ctree, node, COL_UNREAD, itos(item->unread));
		gtk_ctree_node_set_text(ctree, node, COL_TOTAL,  itos(item->total));
	}

	if (item->stype == F_OUTBOX || item->stype == F_DRAFT ||
	    item->stype == F_TRASH) {
		use_bold = use_color = FALSE;
	} else if (item->stype == F_QUEUE) {
		/* highlight queue folder if there are any messages */
		use_bold = use_color = (item->total > 0);
	} else {
		/* if unread messages exist, print with bold font */
		use_bold = (item->unread > 0) || add_unread_mark;
		/* if new messages exist, print with colored letter */
		use_color =
			(item->new > 0) ||
			(add_unread_mark &&
			 folderview_have_new_children(folderview, node));	
	}

	gtk_ctree_node_set_foreground(ctree, node, NULL);

	if (use_bold && use_color) {
		style = bold_color_style;
	} else if (use_bold) {
		style = bold_style;
		if (item->op_count > 0) {
			style = bold_tgtfold_style;
		}
	} else if (use_color) {
		style = normal_color_style;
		gtk_ctree_node_set_foreground(ctree, node,
					      &folderview->color_new);
	} else if (item->op_count > 0) {
		style = bold_tgtfold_style;
	} else if (item->prefs->color > 0) {
		GdkColor gdk_color;

		gtkut_convert_int_to_gdk_color(item->prefs->color, &gdk_color);
		color_style = gtk_style_copy(normal_style);
		color_style->fg[GTK_STATE_NORMAL] = gdk_color;
		style = color_style;
	} else {
		style = normal_style;
	}

	gtk_ctree_node_set_row_style(ctree, node, style);

	item->need_update = FALSE;

	if ((node = gtkut_ctree_find_collapsed_parent(ctree, node)) != NULL)
		folderview_update_node(folderview, node);
}

gboolean folderview_update_item(gpointer source, gpointer data)
{
	FolderItemUpdateData *update_info = (FolderItemUpdateData *)source;
	FolderView *folderview = (FolderView *)data;
	GtkCTree *ctree;
	GtkCTreeNode *node;

	g_return_val_if_fail(update_info != NULL, TRUE);
	g_return_val_if_fail(update_info->item != NULL, TRUE);
	g_return_val_if_fail(folderview != NULL, FALSE);

    	ctree = GTK_CTREE(folderview->ctree);

	node = gtk_ctree_find_by_row_data(ctree, NULL, update_info->item);
	if (node) {
		folderview_update_node(folderview, node);
		if (update_info->content_change && folderview->opened == node)
			summary_show(folderview->summaryview, update_info->item);
	}
	
	return FALSE;
}

static gboolean folderview_gnode_func(GtkCTree *ctree, guint depth,
				      GNode *gnode, GtkCTreeNode *cnode,
				      gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item = FOLDER_ITEM(gnode->data);

	g_return_val_if_fail(item != NULL, FALSE);

	gtk_ctree_node_set_row_data(ctree, cnode, item);
	folderview_update_node(folderview, cnode);

	return TRUE;
}

static void folderview_expand_func(GtkCTree *ctree, GtkCTreeNode *node,
				   gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;

	if (GTK_CTREE_ROW(node)->children) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		g_return_if_fail(item != NULL);

		if (!item->collapsed)
			gtk_ctree_expand(ctree, node);
		else
			folderview_update_node(folderview, node);
	}
}

#define SET_SPECIAL_FOLDER(ctree, item) \
{ \
	if (item) { \
		GtkCTreeNode *node, *parent, *sibling; \
 \
		node = gtk_ctree_find_by_row_data(ctree, root, item); \
		if (!node) \
			g_warning("%s not found.\n", item->path); \
		else { \
			parent = GTK_CTREE_ROW(node)->parent; \
			if (prev && parent == GTK_CTREE_ROW(prev)->parent) \
				sibling = GTK_CTREE_ROW(prev)->sibling; \
			else \
				sibling = GTK_CTREE_ROW(parent)->children; \
			if (node != sibling) \
				gtk_ctree_move(ctree, node, parent, sibling); \
		} \
 \
		prev = node; \
	} \
}

static void folderview_sort_folders(FolderView *folderview, GtkCTreeNode *root,
				    Folder *folder)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *prev = NULL;

	gtk_sctree_sort_recursive(ctree, root);

	if (GTK_CTREE_ROW(root)->parent) return;

	SET_SPECIAL_FOLDER(ctree, folder->inbox);
	SET_SPECIAL_FOLDER(ctree, folder->outbox);
	SET_SPECIAL_FOLDER(ctree, folder->draft);
	SET_SPECIAL_FOLDER(ctree, folder->queue);
	SET_SPECIAL_FOLDER(ctree, folder->trash);
}

static void folderview_append_folder(FolderView *folderview, Folder *folder)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *root;

	g_return_if_fail(folder != NULL);

	root = gtk_ctree_insert_gnode(ctree, NULL, NULL, folder->node,
				      folderview_gnode_func, folderview);
	gtk_ctree_pre_recursive(ctree, root, folderview_expand_func,
				folderview);
	folderview_sort_folders(folderview, root, folder);
}

void folderview_new_folder(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	switch (item->folder->type) {
	case F_MBOX:
		folderview_new_mbox_folder_cb(folderview, 0, NULL);
		break;
	case F_MH:
	case F_MAILDIR:
		folderview_new_folder_cb(folderview, 0, NULL);
		break;
	case F_IMAP:
		folderview_new_imap_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
	default:
		break;
	}
}

void folderview_rename_folder(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	switch (item->folder->type) {
	case F_MBOX:
		folderview_rename_mbox_folder_cb(folderview, 0, NULL);
	case F_MH:
	case F_MAILDIR:
	case F_IMAP:
		folderview_rename_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
	default:
		break;
	}
}

void folderview_delete_folder(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (!item->path) return;
	if (item->stype != F_NORMAL) return;

	switch (item->folder->type) {
	case F_MH:
	case F_MBOX:
	case F_MAILDIR:
	case F_IMAP:
		folderview_delete_folder_cb(folderview, 0, NULL);
		break;
	case F_NEWS:
	default:
		break;
	}
}


/* callback functions */

static void folderview_button_pressed(GtkWidget *ctree, GdkEventButton *event,
				      FolderView *folderview)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint prev_row = -1, row = -1, column = -1;
	FolderItem *item;
	Folder *folder;
	GtkWidget *popup;
	gboolean mark_all_read   = FALSE;
	gboolean new_folder      = FALSE;
	gboolean rename_folder   = FALSE;
	gboolean move_folder	 = FALSE;
	gboolean delete_folder   = FALSE;
	gboolean update_tree     = FALSE;
	gboolean rescan_tree     = FALSE;
	gboolean remove_tree     = FALSE;
	gboolean search_folder   = FALSE;
	gboolean folder_property = FALSE;
	gboolean folder_processing  = FALSE;
	gboolean folder_scoring  = FALSE;

	if (!event) return;

	if (event->button == 1) {
		folderview->open_folder = TRUE;
		return;
	}

	if (event->button == 2 || event->button == 3) {
		/* right clicked */
		if (clist->selection) {
			GtkCTreeNode *node;

			node = GTK_CTREE_NODE(clist->selection->data);
			if (node)
				prev_row = gtkut_ctree_get_nth_from_node
					(GTK_CTREE(ctree), node);
		}

		if (!gtk_clist_get_selection_info(clist, event->x, event->y,
						  &row, &column))
			return;
		if (prev_row != row) {
			gtk_clist_unselect_all(clist);
			if (event->button == 2)
				folderview_select_node
					(folderview,
					 gtk_ctree_node_nth(GTK_CTREE(ctree),
					 		    row));
			else
				gtk_clist_select_row(clist, row, column);
		}
	}

	if (event->button != 3) return;

	item = gtk_clist_get_row_data(clist, row);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	folder = item->folder;

	if (folderview->mainwin->lock_count == 0) {
		new_folder = TRUE;
		if (item->parent == NULL) {
			update_tree = remove_tree = TRUE;
			if (folder->account)
				folder_property = TRUE;
		} else
			mark_all_read = search_folder = folder_property = TRUE;
			
		if (FOLDER_IS_LOCAL(folder) || FOLDER_TYPE(folder) == F_IMAP || FOLDER_TYPE(folder) == F_MBOX) {
			if (item->parent == NULL)
				update_tree = rescan_tree = TRUE;
			else if (item->stype == F_NORMAL)
				move_folder = rename_folder = delete_folder = folder_scoring = folder_processing = TRUE;
			else if (item->stype == F_INBOX)
				folder_scoring = folder_processing = TRUE;
			else if (item->stype == F_TRASH)
				folder_processing = TRUE;
			else if (item->stype == F_OUTBOX)
				folder_processing = TRUE;
		} else if (FOLDER_TYPE(folder) == F_NEWS) {
			if (item->parent != NULL)
				delete_folder = folder_scoring = folder_processing = TRUE;
		}
		if (item->unread < 1) 
			mark_all_read = FALSE;
	}

#define SET_SENS(factory, name, sens) \
	menu_set_sensitive(folderview->factory, name, sens)
	
	mark_all_read = mark_all_read && 
			(item == folderview->summaryview->folder_item);

	if (FOLDER_IS_LOCAL(folder)) {
		popup = folderview->mail_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(mail_factory, "/Mark all read", mark_all_read);
		SET_SENS(mail_factory, "/Create new folder...", new_folder);
		SET_SENS(mail_factory, "/Rename folder...", rename_folder);
		SET_SENS(mail_factory, "/Move folder...", move_folder);
		SET_SENS(mail_factory, "/Delete folder", delete_folder);
		SET_SENS(mail_factory, "/Check for new messages", update_tree);
		SET_SENS(mail_factory, "/Rebuild folder tree", rescan_tree);
		SET_SENS(mail_factory, "/Remove mailbox", remove_tree);
		SET_SENS(mail_factory, "/Search folder...", search_folder);
		SET_SENS(mail_factory, "/Properties...", folder_property);
		SET_SENS(mail_factory, "/Processing...", folder_processing);
		SET_SENS(mail_factory, "/Scoring...", folder_scoring);
	} else if (FOLDER_TYPE(folder) == F_IMAP) {
		popup = folderview->imap_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(imap_factory, "/Mark all read", mark_all_read);
		SET_SENS(imap_factory, "/Create new folder...", new_folder);
		SET_SENS(imap_factory, "/Rename folder...", rename_folder);
		SET_SENS(imap_factory, "/Move folder...", move_folder);
		SET_SENS(imap_factory, "/Delete folder", delete_folder);
		SET_SENS(imap_factory, "/Check for new messages", update_tree);
		SET_SENS(imap_factory, "/Rebuild folder tree", rescan_tree);
		SET_SENS(imap_factory, "/Remove IMAP4 account", remove_tree);
		SET_SENS(imap_factory, "/Search folder...", search_folder);
		SET_SENS(imap_factory, "/Properties...", folder_property);
		SET_SENS(imap_factory, "/Processing...", folder_processing);
		SET_SENS(imap_factory, "/Scoring...", folder_scoring);
	} else if (FOLDER_TYPE(folder) == F_NEWS) {
		popup = folderview->news_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(news_factory, "/Mark all read", mark_all_read);
		SET_SENS(news_factory, "/Subscribe to newsgroup...", new_folder);
		SET_SENS(news_factory, "/Remove newsgroup", delete_folder);
		SET_SENS(news_factory, "/Check for new messages", update_tree);
		SET_SENS(news_factory, "/Remove news account", remove_tree);
		SET_SENS(news_factory, "/Search folder...", search_folder);
		SET_SENS(news_factory, "/Properties...", folder_property);
		SET_SENS(news_factory, "/Processing...", folder_processing);
		SET_SENS(news_factory, "/Scoring...", folder_scoring);
	} else if (FOLDER_TYPE(folder) == F_MBOX) {
		popup = folderview->mbox_popup;
		menu_set_insensitive_all(GTK_MENU_SHELL(popup));
		SET_SENS(mbox_factory, "/Create new folder...", new_folder);
		SET_SENS(mbox_factory, "/Rename folder...", rename_folder);
		SET_SENS(mbox_factory, "/Move folder...", move_folder);
		SET_SENS(mbox_factory, "/Delete folder", delete_folder);
		SET_SENS(news_factory, "/Properties...", folder_property);
		SET_SENS(mbox_factory, "/Processing...", folder_processing);
		SET_SENS(mbox_factory, "/Scoring...", folder_scoring);
	} else
		return;

#undef SET_SENS

	gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL,
		       event->button, event->time);
}

static void folderview_button_released(GtkWidget *ctree, GdkEventButton *event,
				       FolderView *folderview)
{
	if (!event) return;

	if (event->button == 1 && folderview->open_folder == FALSE &&
	    folderview->opened != NULL) {
		gtkut_ctree_set_focus_row(GTK_CTREE(ctree),
					  folderview->opened);
		gtk_ctree_select(GTK_CTREE(ctree), folderview->opened);
	}
}

static void folderview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				   FolderView *folderview)
{
	if (!event) return;

	switch (event->keyval) {
	case GDK_Return:
		if (folderview->selected) {
			folderview_select_node(folderview,
					       folderview->selected);
		}
		break;
	case GDK_space:
		if (folderview->selected) {
			if (folderview->opened == folderview->selected &&
			    folderview->summaryview->messages == 0)
				folderview_select_next_unread(folderview);
			else
				folderview_select_node(folderview,
						       folderview->selected);
		}
		break;
	default:
		break;
	}
}

static void folderview_selected(GtkCTree *ctree, GtkCTreeNode *row,
				gint column, FolderView *folderview)
{
	static gboolean can_select = TRUE;	/* exclusive lock */
	gboolean opened;
	FolderItem *item;
	gchar *buf;

	folderview->selected = row;

	if (folderview->opened == row) {
		folderview->open_folder = FALSE;
		return;
	}

	if (!can_select || summary_is_locked(folderview->summaryview)) {
		gtkut_ctree_set_focus_row(ctree, folderview->opened);
		gtk_ctree_select(ctree, folderview->opened);
		return;
	}

	if (!folderview->open_folder) return;

	item = gtk_ctree_node_get_row_data(ctree, row);
	if (!item) return;

	can_select = FALSE;

	/* Save cache for old folder */
	/* We don't want to lose all caches if sylpheed crashed */
	if (folderview->opened) {
		FolderItem *olditem;
		
		olditem = gtk_ctree_node_get_row_data(ctree, folderview->opened);
		if (olditem) {
			/* will be null if we just moved the previously opened folder */
			summary_save_prefs_to_folderitem(folderview->summaryview, olditem);
			folder_item_close(olditem);
		}
	}

	/* CLAWS: set compose button type: news folder items 
	 * always have a news folder as parent */
	if (item->folder) 
		toolbar_set_compose_button
			(folderview->mainwin->toolbar,
			 item->folder->type == F_NEWS ? 
			 COMPOSEBUTTON_NEWS : COMPOSEBUTTON_MAIL);

	if (item->path)
		debug_print("Folder %s is selected\n", item->path);

	if (!GTK_CTREE_ROW(row)->children)
		gtk_ctree_expand(ctree, row);
	if (folderview->opened &&
	    !GTK_CTREE_ROW(folderview->opened)->children)
		gtk_ctree_collapse(ctree, folderview->opened);

	/* ungrab the mouse event */
	if (GTK_WIDGET_HAS_GRAB(ctree)) {
		gtk_grab_remove(GTK_WIDGET(ctree));
		if (gdk_pointer_is_grabbed())
			gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}

	/* Open Folder */
	buf = g_strdup_printf(_("Opening Folder %s..."), item->path);
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(folderview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(folderview->mainwin);

	if (folder_item_open(item) != 0) {
		main_window_cursor_normal(folderview->mainwin);
		STATUSBAR_POP(folderview->mainwin);

		alertpanel_error(_("Folder could not be opened."));

		return;
        }

	main_window_cursor_normal(folderview->mainwin);

	/* Show messages */
	summary_set_prefs_from_folderitem(folderview->summaryview, item);
	opened = summary_show(folderview->summaryview, item);

	folder_clean_cache_memory();

	if (!opened) {
		gtkut_ctree_set_focus_row(ctree, folderview->opened);
		gtk_ctree_select(ctree, folderview->opened);
	} else {
		folderview->opened = row;
		if (gtk_ctree_node_is_visible(ctree, row)
		    != GTK_VISIBILITY_FULL)
			gtk_ctree_node_moveto(ctree, row, -1, 0.5, 0);
	}

	STATUSBAR_POP(folderview->mainwin);

	folderview->open_folder = FALSE;
	can_select = TRUE;
}

static void folderview_tree_expanded(GtkCTree *ctree, GtkCTreeNode *node,
				     FolderView *folderview)
{
	FolderItem *item;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	item->collapsed = FALSE;
	folderview_update_node(folderview, node);
}

static void folderview_tree_collapsed(GtkCTree *ctree, GtkCTreeNode *node,
				      FolderView *folderview)
{
	FolderItem *item;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	item->collapsed= TRUE;
	folderview_update_node(folderview, node);
}

static void folderview_popup_close(GtkMenuShell *menu_shell,
				   FolderView *folderview)
{
	if (!folderview->opened) return;

	gtkut_ctree_set_focus_row(GTK_CTREE(folderview->ctree),
				  folderview->opened);
	gtk_ctree_select(GTK_CTREE(folderview->ctree), folderview->opened);
}

static void folderview_col_resized(GtkCList *clist, gint column, gint width,
				   FolderView *folderview)
{
	switch (column) {
	case COL_FOLDER:
		prefs_common.folder_col_folder = width;
		break;
	case COL_NEW:
		prefs_common.folder_col_new = width;
		break;
	case COL_UNREAD:
		prefs_common.folder_col_unread = width;
		break;
	case COL_TOTAL:
		prefs_common.folder_col_total = width;
		break;
	default:
		break;
	}
}

static GtkCTreeNode *folderview_find_by_name(GtkCTree *ctree,
					     GtkCTreeNode *node,
					     const gchar *name)
{
	FolderItem *item;

	if (!node)
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	if (!node)
		return NULL;

	node = GTK_CTREE_ROW(node)->children;

	while (node) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (!folderview_compare_name(item, name))
			return node;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return NULL;
}

static void folderview_update_tree_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	summary_show(folderview->summaryview, NULL);

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if (action == 0)
		folderview_check_new(item->folder);
	else
		folderview_rescan_tree(item->folder);
	
	if (folderview->opened) {
		item = gtk_ctree_node_get_row_data(ctree, folderview->opened);
		if (item)
			folder_update_item(item, TRUE);
	}
}

void folderview_create_folder_node_recursive(FolderView *folderview, FolderItem *item)
{
	GNode *srcnode;

	folderview_create_folder_node(folderview, item);
	
	srcnode = item->folder->node;	
	srcnode = g_node_find(srcnode, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	srcnode = srcnode->children;
	while (srcnode != NULL) {
		if (srcnode && srcnode->data) {
			FolderItem *next_item = (FolderItem*) srcnode->data;
			folderview_create_folder_node_recursive(folderview, next_item);
		}
		srcnode = srcnode->next;
	}
}

void folderview_create_folder_node(FolderView *folderview, FolderItem *item)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	gchar *text[N_FOLDER_COLS] = {NULL, "0", "0", "0"};
	GtkCTreeNode *node, *parent_node;
	
	parent_node = gtk_ctree_find_by_row_data(ctree, NULL, item->parent);

	gtk_clist_freeze(GTK_CLIST(ctree));

	text[COL_FOLDER] = item->name;
	node = gtk_ctree_insert_node(ctree, parent_node, NULL, text,
				     FOLDER_SPACING,
				     folderxpm, folderxpmmask,
				     folderopenxpm, folderopenxpmmask,
				     FALSE, FALSE);
	gtk_ctree_expand(ctree, parent_node);
	gtk_ctree_node_set_row_data(ctree, node, item);
	if (normal_style)
		gtk_ctree_node_set_row_style(ctree, node, normal_style);
	folderview_sort_folders(folderview, folderview->selected, item->folder);

	gtk_clist_thaw(GTK_CLIST(ctree));
}

static void folderview_new_folder_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name, *name_;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;

#ifdef WIN32
	locale_from_utf8(&new_folder);
#endif
	if (item->folder->type != F_MBOX) {
		if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
			alertpanel_error(_("`%c' can't be included in folder name."),
					 G_DIR_SEPARATOR);
			g_free(new_folder);
			return;
		}
	}

	name_ = trim_string(new_folder, 32);
	Xstrdup_a(name, name_, {g_free(new_folder); return;});
	g_free(name_);

	/* find whether the directory already exists */
	if (folderview_find_by_name(ctree, folderview->selected, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."), name);
		g_free(new_folder);
		return;
	}

	new_item = folder_create_folder(item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder `%s'."), name);
		g_free(new_folder);
		return;
	} 
	g_free(new_folder);

	folderview_create_folder_node(folderview, new_item);

	folder_write_list();
}

static void folderview_new_mbox_folder_cb(FolderView *folderview, guint action,
					  GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;

	/* find whether the directory already exists */
	if (folderview_find_by_name(ctree, folderview->selected, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."),
				 new_folder);
		g_free(new_folder);
		return;
	}

	new_item = folder_create_folder(item, new_folder);
	g_free(new_folder);
	if (!new_item) return;

	folderview_create_folder_node(folderview, new_item);

	folder_write_list();
}

static void folderview_rename_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *new_folder;
	gchar *name, *name_;
	gchar *message;
	gchar *old_path;
	gchar *old_id;
	gchar *new_id;
#ifdef WIN32
	gchar *p_path;
#endif

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name_ = trim_string(item->name, 32);
	Xstrdup_a(name, name_, return);
	g_free(name_);
#ifdef WIN32
	p_path = g_strdup(item->path);
	locale_to_utf8(&p_path);
#endif
	message = g_strdup_printf(_("Input new name for `%s':"), name);
	new_folder = input_dialog(_("Rename folder"), message,
#ifdef WIN32
				  g_basename(p_path));
	g_free(p_path);
#else
				  g_basename(item->path));
#endif
	g_free(message);
	if (!new_folder) return;

#ifdef WIN32
	locale_from_utf8(&new_folder);
#endif

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		g_free(new_folder);
		return;
	}

	if (folderview_find_by_name
		(ctree, GTK_CTREE_ROW(folderview->selected)->parent,
		 new_folder)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder `%s' already exists."), name);
		g_free(name);
		g_free(new_folder);
		return;
	}

	Xstrdup_a(old_path, item->path, {g_free(new_folder); return;});
	old_id = folder_item_get_identifier(item);

	if (item->folder->rename_folder(item->folder, item, new_folder) < 0) {
		g_free(old_id);
		g_free(new_folder);
		return;
	}
	g_free(new_folder);

	/* if (FOLDER_TYPE(item->folder) == F_MH)
		prefs_filtering_rename_path(old_path, item->path); */
	new_id = folder_item_get_identifier(item);
	prefs_filtering_rename_path(old_id, new_id);

	g_free(old_id);
	g_free(new_id);

	gtk_clist_freeze(GTK_CLIST(ctree));

	folderview_update_node(folderview, folderview->selected);
	folderview_sort_folders(folderview,
				GTK_CTREE_ROW(folderview->selected)->parent,
				item->folder);
	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		GtkCTreeNode *node = folderview->opened;
		folderview_unselect(folderview);
		folderview_select_node(folderview, node);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_rename_mbox_folder_cb(FolderView *folderview,
					     guint action,
					     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *new_folder;
	gchar *message;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

#ifdef WIN32
	{
		gchar *p_path;
		p_path = g_strdup(item->path);
		locale_to_utf8(&p_path);

		message = g_strdup_printf(_("Input new name for `%s':"),
					  g_basename(p_path));
		new_folder = input_dialog(_("Rename folder"), message,
					  g_basename(p_path));
	}
#else
	message = g_strdup_printf(_("Input new name for `%s':"),
				  g_basename(item->path));
	new_folder = input_dialog(_("Rename folder"), message,
				  g_basename(item->path));
#endif
	g_free(message);
	if (!new_folder) return;

#ifdef WIN32
	new_folder = g_strdup(new_folder);
	locale_from_utf8(&new_folder);
#endif

	if (folderview_find_by_name
		(ctree, GTK_CTREE_ROW(folderview->selected)->parent,
		 new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."),
				 new_folder);
		g_free(new_folder);
		return;
	}

	if (item->folder->rename_folder(item->folder, item, new_folder) < 0) {
		g_free(new_folder);
		return;
	}
	g_free(new_folder);

	gtk_clist_freeze(GTK_CLIST(ctree));

	folderview_update_node(folderview, folderview->selected);
	folderview_sort_folders(folderview,
				GTK_CTREE_ROW(folderview->selected)->parent,
				item->folder);
	if (folderview->opened == folderview->selected) {
		if (!GTK_CTREE_ROW(folderview->opened)->children)
			gtk_ctree_expand(ctree, folderview->opened);
		summary_show(folderview->summaryview, item);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_delete_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *message, *name, *name_;
	AlertValue avalue;
	gchar *old_path;
	gchar *old_id;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name_ = trim_string(item->name, 32);

	Xstrdup_a(name, name_, return);
	g_free(name_);
	message = g_strdup_printf
		(_("All folder(s) and message(s) under `%s' will be deleted.\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel(_("Delete folder"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	Xstrdup_a(old_path, item->path, return);
	old_id = folder_item_get_identifier(item);

	if (item->folder->remove_folder(item->folder, item) < 0) {
		alertpanel_error(_("Can't remove the folder `%s'."), name);
		if (folderview->opened == folderview->selected)
			summary_show(folderview->summaryview,
				     folderview->summaryview->folder_item);
		g_free(old_id);
		return;
	}

	/* if (FOLDER_TYPE(item->folder) == F_MH)
		prefs_filtering_delete_path(old_path); */

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	gtk_ctree_remove_node(ctree, folderview->selected);
	folder_write_list();

	prefs_filtering_delete_path(old_id);
	g_free(old_id);

}

static void folderview_remove_mailbox_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node;
	FolderItem *item;
	gchar *name, *name_;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;
	node = folderview->selected;
	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (item->parent) return;

	name_ = trim_string(item->folder->name, 32);
	Xstrdup_a(name, name_, return);
	g_free(name_);
	message = g_strdup_printf
		(_("Really remove the mailbox `%s' ?\n"
		   "(The messages are NOT deleted from the disk)"), name);
	avalue = alertpanel(_("Remove mailbox"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	folder_destroy(item->folder);
	summary_clear_all(folderview->summaryview);
	folderview_unselect(folderview);
	gtk_ctree_remove_node(ctree, node);
	folder_write_list();
}

static void folderview_new_imap_folder_cb(FolderView *folderview, guint action,
					  GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name, *name_;
	gchar *p;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_IMAP);
	g_return_if_fail(item->folder->account != NULL);

	new_folder = input_dialog
		(_("New folder"),
		 _("Input the name of new folder:\n"
		   "(if you want to create a folder to store subfolders,\n"
		   " append `/' at the end of the name)"),
		 _("NewFolder"));
	if (!new_folder) return;

	if ((p = strchr(new_folder, G_DIR_SEPARATOR)) != NULL &&
	    *(p + 1) != '\0') {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		g_free(new_folder);
		return;
	}

	name_ = trim_string(new_folder, 32);
	Xstrdup_a(name, name_, return);
	g_free(name_);

	/* find whether the directory already exists */
	if (folderview_find_by_name(ctree, folderview->selected, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."), name);
		g_free(new_folder);
		return;
	}

	new_item = folder_create_folder(item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder `%s'."), name);
		g_free(new_folder);
		return;
	}
	g_free(new_folder);

	folderview_create_folder_node(folderview, new_item);

	folder_write_list();
}

static void folderview_rm_imap_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	PrefsAccount *account;
	gchar *name, *name_;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_IMAP);
	g_return_if_fail(item->folder->account != NULL);

	name_ = trim_string(item->folder->name, 32);
	Xstrdup_a(name, name_, return);
	g_free(name_);
	message = g_strdup_printf(_("Really delete IMAP4 account `%s'?"), name);
	avalue = alertpanel(_("Delete IMAP4 account"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);

	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folder_destroy(item->folder);
	account_destroy(account);
	gtk_ctree_remove_node(ctree, folderview->selected);
	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_new_news_group_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	gchar *text[N_FOLDER_COLS] = {NULL, "0", "0", "0"};
	GtkCTreeNode *servernode, *node;
	Folder *folder;
	FolderItem *item;
	FolderItem *rootitem;
	FolderItem *newitem;
	GSList *new_subscr;
	GSList *cur;
	GNode *gnode;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	folder = item->folder;
	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->type == F_NEWS);
	g_return_if_fail(folder->account != NULL);

	if (GTK_CTREE_ROW(folderview->selected)->parent != NULL)
		servernode = GTK_CTREE_ROW(folderview->selected)->parent;
	else
		servernode = folderview->selected;

	rootitem = gtk_ctree_node_get_row_data(ctree, servernode);

	new_subscr = grouplist_dialog(folder);

	/* remove unsubscribed newsgroups */
	for (gnode = folder->node->children; gnode != NULL; ) {
		GNode *next = gnode->next;

		item = FOLDER_ITEM(gnode->data);
		if (g_slist_find_custom(new_subscr, item->path,
					(GCompareFunc)g_strcasecmp) != NULL) {
			gnode = next;
			continue;
		}

		node = gtk_ctree_find_by_row_data(ctree, servernode, item);
		if (!node) {
			gnode = next;
			continue;
		}

		if (folderview->opened == node) {
			summary_clear_all(folderview->summaryview);
			folderview->opened = NULL;
		}

		folder_item_remove(item);
		gtk_ctree_remove_node(ctree, node);

		gnode = next;
	}

	gtk_clist_freeze(GTK_CLIST(ctree));

	/* add subscribed newsgroups */
	for (cur = new_subscr; cur != NULL; cur = cur->next) {
		gchar *name = (gchar *)cur->data;

		if (folderview_find_by_name(ctree, servernode, name) != NULL)
			continue;

		text[COL_FOLDER] = name;
		node = gtk_ctree_insert_node(ctree, servernode, NULL, text,
					     FOLDER_SPACING,
					     folderxpm, folderxpmmask,
					     folderopenxpm, folderopenxpmmask,
					     FALSE, FALSE);
		gtk_ctree_expand(ctree, servernode);

		newitem = folder_item_new(folder, name, name);
		folder_item_append(rootitem, newitem);
		gtk_ctree_node_set_row_data(ctree, node, newitem);
	}

	folderview_sort_folders(folderview, servernode, folder);
	gtk_clist_thaw(GTK_CLIST(ctree));

	slist_free_strings(new_subscr);
	g_slist_free(new_subscr);

	folder_write_list();
}

static void folderview_rm_news_group_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *name, *name_;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	name_ = trim_string(item->path, 32);
	Xstrdup_a(name, name_, return);
	g_free(name_);
	message = g_strdup_printf(_("Really delete newsgroup `%s'?"), name);
	avalue = alertpanel(_("Delete newsgroup"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->opened == folderview->selected) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	folder_item_remove(item);
	gtk_ctree_remove_node(ctree, folderview->selected);
	folder_write_list();
}

static void folderview_rm_news_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	PrefsAccount *account;
	gchar *name, *name_;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	name_ = trim_string(item->folder->name, 32);
	Xstrdup_a(name, name_, return);
	g_free(name_);
	message = g_strdup_printf(_("Really delete news account `%s'?"), name);
	avalue = alertpanel(_("Delete news account"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);

	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	account = item->folder->account;
 	folder_destroy(item->folder);
	account_destroy(account);
	gtk_ctree_remove_node(ctree, folderview->selected);
	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void folderview_search_cb(FolderView *folderview, guint action,
				 GtkWidget *widget)
{
	summary_search(folderview->summaryview);
}

static void folderview_property_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

#if CLAWS
	prefs_folder_item_create(folderview, item);
#else
	/*
	 * CLAWS: wait till Hiro has completed his stuff
	 */
	if (item->parent == NULL && item->folder->account)
		account_open(item->folder->account);
	else
		prefs_folder_item_open(item);
#endif	
}

static void folderview_recollapse_nodes(FolderView *folderview, GtkCTreeNode *node)
{
	GSList *list = NULL;
	GSList *done = NULL;
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	
	for (list = folderview->nodes_to_recollapse; list != NULL; list = g_slist_next(list)) {
		if (!gtkut_ctree_node_is_parent(GTK_CTREE_NODE(list->data), node)
		&&  list->data != node) {
			gtk_ctree_collapse(ctree, GTK_CTREE_NODE(list->data));
			done = g_slist_append(done, GTK_CTREE_NODE(list->data));
		}
	}
	for (list = done; list != NULL; list = g_slist_next(list)) {
		folderview->nodes_to_recollapse = g_slist_remove(folderview->nodes_to_recollapse, 
								 list->data);
	}
	g_slist_free(done);
}

static void folderview_move_to_cb(FolderView *folderview) 
{
	FolderItem *from_folder = NULL, *to_folder = NULL;

	if (folderview->selected)
		from_folder = gtk_ctree_node_get_row_data(GTK_CTREE(folderview->ctree), folderview->selected);
	if (!from_folder || from_folder->folder->type == F_NEWS)
		return;

	to_folder = foldersel_folder_sel(from_folder->folder, FOLDER_SEL_MOVE, NULL);
	
	if (!to_folder || to_folder->folder->type == F_NEWS)
		return;

	folderview_move_to(folderview, from_folder, to_folder);
}

static void folderview_move_to(FolderView *folderview, FolderItem *from_folder,
			       FolderItem *to_folder)
{
	FolderItem *from_parent = NULL;
	FolderItem *new_folder = NULL;
	GtkCTreeNode *src_node = NULL;
	gchar *buf;
	gint status;

	src_node = gtk_ctree_find_by_row_data(GTK_CTREE(folderview->ctree), NULL, from_folder);
	from_parent = from_folder->parent;
	buf = g_strdup_printf(_("Moving %s to %s..."), from_folder->name, to_folder->name);
	STATUSBAR_PUSH(folderview->mainwin, buf);
	g_free(buf);
	summary_clear_all(folderview->summaryview);
	folderview->opened = NULL;
	folderview->selected = NULL;
	gtk_widget_set_sensitive(GTK_WIDGET(folderview->ctree), FALSE);
	inc_lock();
	main_window_cursor_wait(folderview->mainwin);
	statusbar_verbosity_set(TRUE);
	if ((status = folder_item_move_to(from_folder, to_folder, &new_folder)) == F_MOVE_OK) {
		statusbar_verbosity_set(FALSE);
		main_window_cursor_normal(folderview->mainwin);
		STATUSBAR_POP(folderview->mainwin);
		if (src_node)
			gtk_ctree_remove_node(GTK_CTREE(folderview->ctree), src_node);
		else 
			debug_print("can't remove src node: is null\n");

		folderview_create_folder_node_recursive(folderview, new_folder);
		folder_update_item(from_parent, TRUE);
		folder_update_item_recursive(new_folder, TRUE); 
		folderview_sort_folders(folderview, 
			gtk_ctree_find_by_row_data(GTK_CTREE(folderview->ctree), 
				NULL, new_folder->parent), new_folder->folder);
		folderview_select(folderview, new_folder);
	} else {
		statusbar_verbosity_set(FALSE);		
		main_window_cursor_normal(folderview->mainwin);
		STATUSBAR_POP(folderview->mainwin);
		switch (status) {
		case F_MOVE_FAILED_DEST_IS_PARENT:
			alertpanel_error(_("Source and destination are the same."));
			break;
		case F_MOVE_FAILED_DEST_IS_CHILD:
			alertpanel_error(_("Can't move a folder to one of its children."));
			break;
		case F_MOVE_FAILED_DEST_OUTSIDE_MAILBOX:
			alertpanel_error(_("Folder moving cannot be done between different mailboxes."));
			break;
		default:
			alertpanel_error(_("Move failed!"));
			break;
		}
	}	
	inc_unlock();		
	gtk_widget_set_sensitive(GTK_WIDGET(folderview->ctree), TRUE);
}

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview)
{
	gint row, column;
	FolderItem *item, *src_item;
	GtkCTreeNode *node = NULL;
	gboolean acceptable = FALSE;
	gint height = folderview->ctree->allocation.height;
	gint total_height = folderview->ctree->requisition.height;
	GtkAdjustment *pos = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(folderview->scrolledwin));
	gfloat vpos = pos->value;

	if (gtk_clist_get_selection_info
		(GTK_CLIST(widget), x - 24, y - 24, &row, &column)) {
		if (y > height - 24 && height + vpos < total_height)
			gtk_adjustment_set_value(pos, (vpos+5 > height ? height : vpos+5));

		if (y < 24 && y > 0)
			gtk_adjustment_set_value(pos, (vpos-5 < 0 ? 0 : vpos-5));

		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
		src_item = folderview->summaryview->folder_item;

		if (item && item->folder && item->path &&
		    src_item && src_item != item) {
			switch (item->folder->type) {
			case F_MH:
			case F_MBOX:
			case F_IMAP:
				acceptable = TRUE;
				break;
			default:
				break;
			}
		} else if (item && item->folder && folder_item_get_path(item) &&
			   src_item && src_item != item) {
			/* a root folder - acceptable only from folderview */
			if (item->folder->type == F_MH || item->folder->type == F_IMAP)
				acceptable = TRUE;
		}
			
	}

	if (acceptable || (src_item && src_item == item)) {
		folderview_recollapse_nodes(folderview, node);
		if (item->collapsed) {
			gtk_ctree_expand(GTK_CTREE(widget), node);
			folderview->nodes_to_recollapse = g_slist_append(
								folderview->nodes_to_recollapse,
					 			node);
		}
	}
		
	if (acceptable) {
		gtk_signal_handler_block_by_func
			(GTK_OBJECT(widget),
			 GTK_SIGNAL_FUNC(folderview_selected), folderview);
		gtk_ctree_select(GTK_CTREE(widget), node);
		gtk_signal_handler_unblock_by_func
			(GTK_OBJECT(widget),
			 GTK_SIGNAL_FUNC(folderview_selected), folderview);
		gdk_drag_status(context, 
					(context->actions == GDK_ACTION_COPY ?
					GDK_ACTION_COPY : GDK_ACTION_MOVE) , time);
	} else {
		gtk_ctree_select(GTK_CTREE(widget), folderview->opened);
		gdk_drag_status(context, 0, time);
	}

	return acceptable;
}

static void folderview_drag_leave_cb(GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     FolderView     *folderview)
{
	gtk_ctree_select(GTK_CTREE(widget), folderview->opened);
}

static void folderview_drag_received_cb(GtkWidget        *widget,
					GdkDragContext   *drag_context,
					gint              x,
					gint              y,
					GtkSelectionData *data,
					guint             info,
					guint             time,
					FolderView       *folderview)
{
	gint row, column;
	FolderItem *item, *src_item;
	GtkCTreeNode *node;

	if ((void *)strstr(data->data, "FROM_OTHER_FOLDER") != (void *)data->data) {
		/* comes from summaryview */
		if (gtk_clist_get_selection_info
			(GTK_CLIST(widget), x - 24, y - 24, &row, &column) == 0)
			return;

		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
		src_item = folderview->summaryview->folder_item;
		
		/* re-check (due to acceptable possibly set for folder moves */
		if (!(item && item->folder && item->path &&
		      src_item && src_item != item && 
		      (item->folder->type == F_MH || item->folder->type == F_IMAP))) {
			return;
		}
		if (item && src_item) {
			switch (drag_context->action) {
				case GDK_ACTION_COPY:
					summary_copy_selected_to(folderview->summaryview, item);
					gtk_drag_finish(drag_context, TRUE, FALSE, time);
					break;
				case GDK_ACTION_MOVE:
				case GDK_ACTION_DEFAULT:
				default:
			if (src_item->folder->type != item->folder->type ||
			    (item->folder->type == F_IMAP &&
			     src_item->folder != item->folder))
				summary_copy_selected_to(folderview->summaryview, item);
			else
				summary_move_selected_to(folderview->summaryview, item);
			gtk_drag_finish(drag_context, TRUE, TRUE, time);
			}
		} else
			gtk_drag_finish(drag_context, FALSE, FALSE, time);
	} else {
		/* comes from folderview */
		char *source;
		
		source = data->data + 17;
		if (gtk_clist_get_selection_info
		    (GTK_CLIST(widget), x - 24, y - 24, &row, &column) == 0
		    || *source == 0) {
			gtk_drag_finish(drag_context, FALSE, FALSE, time);			
			return;
		}
		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
		src_item = folder_find_item_from_identifier(source);

		if (!item || !src_item || src_item->stype != F_NORMAL) {
			gtk_drag_finish(drag_context, FALSE, FALSE, time);			
			return;
		}

		folderview_move_to(folderview, src_item, item);
		gtk_drag_finish(drag_context, TRUE, TRUE, time);
	}
	folderview->nodes_to_recollapse = NULL;
}

static gint folderview_clist_compare(GtkCList *clist,
				     gconstpointer ptr1, gconstpointer ptr2)
{
	FolderItem *item1 = ((GtkCListRow *)ptr1)->data;
	FolderItem *item2 = ((GtkCListRow *)ptr2)->data;

	if (!item1->name)
		return (item2->name != NULL);
	if (!item2->name)
		return -1;

	return g_strcasecmp(item1->name, item2->name);
}

static gint folderview_compare_name(gconstpointer a, gconstpointer b)
{
	const FolderItem *item = a;
	const gchar *name = b;

	if (!item->path) return -1;
	return strcmp2(g_basename(item->path), name);
}

static void folderview_scoring_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	prefs_scoring_open(item);
}

static void folderview_processing_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	prefs_filtering_open(item, NULL, NULL);
}

void folderview_set_target_folder_color(gint color_op) 
{
	gint firstone = 1;
	GList *list;
	FolderView *folderview;

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		gtkut_convert_int_to_gdk_color(color_op, &folderview->color_op);
		if (firstone) {
			bold_tgtfold_style->fg[GTK_STATE_NORMAL] =
				folderview->color_op;
			firstone = 0;
		}
	}
}

void folderview_reflect_prefs_pixmap_theme(FolderView *folderview)
{
	folderview_init(folderview);
	folderview_set_all();
}

GtkTargetEntry folderview_drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_APP, TARGET_DUMMY}
};


static void folderview_start_drag(GtkWidget *widget, gint button, GdkEvent *event,
			          FolderView       *folderview)
{
	GtkTargetList *list;
	GdkDragContext *context;

	g_return_if_fail(folderview != NULL);
	if (folderview->selected == NULL) return;

	list = gtk_target_list_new(folderview_drag_types, 1);

	folderview->nodes_to_recollapse = NULL; /* in case the last drag has been cancelled */
	
	context = gtk_drag_begin(widget, list,
				 GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_DEFAULT, button, event);
	gtk_drag_set_icon_default(context);
}

static void folderview_drag_data_get(GtkWidget        *widget,
				     GdkDragContext   *drag_context,
				     GtkSelectionData *selection_data,
				     guint             info,
				     guint             time,
				     FolderView       *folderview)
{
	FolderItem *item;
	GList *cur;
	gchar *source=NULL;
	
	for (cur = GTK_CLIST(folderview->ctree)->selection;
	     cur != NULL; cur = cur->next) {
		item = gtk_ctree_node_get_row_data
			(GTK_CTREE(folderview->ctree), 
			 GTK_CTREE_NODE(cur->data));
		if (item) {
			source = g_strdup_printf ("FROM_OTHER_FOLDER%s", folder_item_get_identifier(item));
			gtk_selection_data_set(selection_data,
					       selection_data->target, 8,
					       source, strlen(source));
			break;
		} else
			return;
	}
}

