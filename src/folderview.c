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
#include "inputdialog.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "menu.h"
#include "procmsg.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "folder.h"
#include "grouplist_dialog.h"

#include "pixmaps/inbox.xpm"
#include "pixmaps/outbox.xpm"
#include "pixmaps/dir-close.xpm"
#include "pixmaps/dir-open.xpm"
#include "pixmaps/trash.xpm"

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

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->folderview_cid, str); \
	gtkut_widget_wait_for_draw(mainwin->hbox_stat); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->folderview_cid); \
}

static GList *folderview_list = NULL;

static GdkFont *normalfont;
static GdkFont *boldfont;

static GdkPixmap *inboxxpm;
static GdkBitmap *inboxxpmmask;
static GdkPixmap *outboxxpm;
static GdkBitmap *outboxxpmmask;
static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;
static GdkPixmap *folderopenxpm;
static GdkBitmap *folderopenxpmmask;
static GdkPixmap *trashxpm;
static GdkBitmap *trashxpmmask;

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
static void folderview_rm_imap_folder_cb (FolderView	*folderview,
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
static void folderview_scoring_cb(FolderView *folderview, guint action,
				  GtkWidget *widget);

static GtkItemFactoryEntry folderview_mbox_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_mbox_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_mbox_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _mailbox"),	NULL, folderview_remove_mailbox_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Property..."),		NULL, NULL, 0, NULL},
	{N_("/_Scoring..."),		NULL, folderview_scoring_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_mail_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_folder_cb,    0, NULL},
	{N_("/_Rename folder..."),	NULL, folderview_rename_folder_cb, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_delete_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _mailbox"),	NULL, folderview_remove_mailbox_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Property..."),		NULL, NULL, 0, NULL},
	{N_("/_Scoring..."),		NULL, folderview_scoring_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_imap_popup_entries[] =
{
	{N_("/Create _new folder..."),	NULL, folderview_new_imap_folder_cb, 0, NULL},
	{N_("/_Rename folder..."),	NULL, NULL, 0, NULL},
	{N_("/_Delete folder"),		NULL, folderview_rm_imap_folder_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _IMAP4 server"),	NULL, folderview_rm_imap_server_cb, 0, NULL},
	{N_("/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Property..."),		NULL, NULL, 0, NULL},
	{N_("/_Scoring..."),		NULL, folderview_scoring_cb, 0, NULL}
};

static GtkItemFactoryEntry folderview_news_popup_entries[] =
{
	{N_("/_Subscribe to newsgroup..."),
					 NULL, folderview_new_news_group_cb, 0, NULL},
	{N_("/_Remove newsgroup"),	 NULL, folderview_rm_news_group_cb, 0, NULL},
	{N_("/---"),			 NULL, NULL, 0, "<Separator>"},
	{N_("/Remove _news server"),	 NULL, folderview_rm_news_server_cb, 0, NULL},
	{N_("/---"),			 NULL, NULL, 0, "<Separator>"},
	{N_("/_Property..."),		 NULL, NULL, 0, NULL},
	{N_("/_Scoring..."),		NULL, folderview_scoring_cb, 0, NULL}
};


FolderView *folderview_create(void)
{
	FolderView *folderview;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	gchar *titles[N_FOLDER_COLS] = {_("Folder"), _("New"),
					_("Unread"), _("#")};
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

	debug_print(_("Creating folder view...\n"));
	folderview = g_new0(FolderView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_widget_set_usize(scrolledwin,
			     prefs_common.folderview_width,
			     prefs_common.folderview_height);

	ctree = gtk_ctree_new_with_titles(N_FOLDER_COLS, COL_FOLDER, titles);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_NEW,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_UNREAD,
					   GTK_JUSTIFY_RIGHT);
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
			  GDK_ACTION_MOVE);
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

	gtk_widget_show_all(scrolledwin);

	folderview_list = g_list_append(folderview_list, folderview);

	return folderview;
}

void folderview_init(FolderView *folderview)
{
	GtkWidget *ctree = folderview->ctree;

	PIXMAP_CREATE(ctree, inboxxpm, inboxxpmmask, inbox_xpm);
	PIXMAP_CREATE(ctree, outboxxpm, outboxxpmmask, outbox_xpm);
	PIXMAP_CREATE(ctree, folderxpm, folderxpmmask, DIRECTORY_CLOSE_XPM);
	PIXMAP_CREATE(ctree, folderopenxpm, folderopenxpmmask,
		      DIRECTORY_OPEN_XPM);
	PIXMAP_CREATE(ctree, trashxpm, trashxpmmask, trash_xpm);

	if (!normalfont)
		normalfont = gdk_fontset_load(NORMAL_FONT);
	if (!boldfont)
		boldfont = gdk_fontset_load(BOLD_FONT);
}

void folderview_set(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	MainWindow *mainwin = folderview->mainwin;

	debug_print(_("Setting folder info...\n"));
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

void folderview_select(FolderView *folderview, FolderItem *item)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node;

	if (!item) return;

	node = gtk_ctree_find_by_row_data(ctree, NULL, item);
	if (node) folderview_select_node(folderview, node);
}

static void folderview_select_node(FolderView *folderview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	g_return_if_fail(node != NULL);

	folderview->open_folder = TRUE;
	gtk_ctree_select(ctree, node);
	gtkut_ctree_set_focus_row(ctree, node);
	if (folderview->summaryview->messages > 0)
		gtk_widget_grab_focus(folderview->summaryview->ctree);
	else
		gtk_widget_grab_focus(folderview->ctree);

	while ((node = gtkut_ctree_find_collapsed_parent(ctree, node))
	       != NULL)
		gtk_ctree_expand(ctree, node);
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

void folderview_update_msg_num(FolderView *folderview, GtkCTreeNode *row,
			       gint new, gint unread, gint total)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	static GtkCTreeNode *prev_row = NULL;
	FolderItem *item;

	if (!row) return;

	item = gtk_ctree_node_get_row_data(ctree, row);
	if (!item) return;
	if (prev_row     == row    &&
	    item->new    == new    &&
	    item->unread == unread &&
	    item->total  == total)
		return;

	prev_row = row;

	item->new    = new;
	item->unread = unread;
	item->total  = total;

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

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		MainWindow *mainwin = folderview->mainwin;
		gchar *str;

		if (item->path)
			str = g_strdup_printf(_("Scanning folder %s%c%s ..."),
					      LOCAL_FOLDER(folder)->rootpath,
					      G_DIR_SEPARATOR,
					      item->path);
		else
			str = g_strdup_printf(_("Scanning folder %s ..."),
					      LOCAL_FOLDER(folder)->rootpath);

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

void folderview_update_all(void)
{
	GList *list;
	GtkWidget *window;

	window = label_window_create(_("Updating all folders..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (!folder->scan_tree) continue;
		folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
		folder->scan_tree(folder);
		folder_set_ui_func(folder, NULL, NULL);
	}

	folder_write_list();

	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;

		folderview_set(folderview);
	}

	gtk_widget_destroy(window);
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
	GtkStyle *style, *prev_style, *ctree_style;
	GtkCTreeNode *parent;
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
		xpm = openxpm = inboxxpm;
		mask = openmask = inboxxpmmask;
		name = g_strdup(_("Inbox"));
		break;
	case F_OUTBOX:
		xpm = openxpm = outboxxpm;
		mask = openmask = outboxxpmmask;
		name = g_strdup(_("Outbox"));
		break;
	case F_QUEUE:
		xpm = openxpm = outboxxpm;
		mask = openmask = outboxxpmmask;
		name = g_strdup(_("Queue"));
		break;
	case F_TRASH:
		xpm = openxpm = trashxpm;
		mask = openmask = trashxpmmask;
		name = g_strdup(_("Trash"));
		break;
	case F_DRAFT:
		xpm = folderxpm;
		mask = folderxpmmask;
		openxpm = folderopenxpm;
		openmask = folderopenxpmmask;
		name = g_strdup(_("Draft"));
		break;
	default:
		xpm = folderxpm;
		mask = folderxpmmask;
		openxpm = folderopenxpm;
		openmask = folderopenxpmmask;
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
		} else
			name = g_strdup(item->name);
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
			str = g_strdup_printf("%s (%d%s)", name, item->unread,
					      add_unread_mark ? "+" : "");
		else
			str = g_strdup_printf("%s (+)", name);
		gtk_ctree_set_node_info(ctree, node, str, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
		g_free(str);
	} else
		gtk_ctree_set_node_info(ctree, node, name, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
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

	if (item->stype == F_TRASH) return;

	ctree_style = gtk_widget_get_style(GTK_WIDGET(ctree));
	prev_style = gtk_ctree_node_get_row_style(ctree, node);
	if (!prev_style)
		prev_style = ctree_style;
	style = gtk_style_copy(prev_style);
	if (!style) return;

	if (item->stype == F_QUEUE) {
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

	if (use_bold && boldfont)
		style->font = boldfont;
	else
		style->font = normalfont;

	if (use_color) {
		style->fg[GTK_STATE_NORMAL]   = folderview->color_new;
		style->fg[GTK_STATE_SELECTED] = folderview->color_new;
	} else {
		style->fg[GTK_STATE_NORMAL] =
			ctree_style->fg[GTK_STATE_NORMAL];
		style->fg[GTK_STATE_SELECTED] =
			ctree_style->fg[GTK_STATE_SELECTED];
	}

	gtk_ctree_node_set_row_style(ctree, node, style);

	parent = node;
	while ((parent = gtkut_ctree_find_collapsed_parent(ctree, parent))
	       != NULL)
		folderview_update_node(folderview, parent);
}

void folderview_update_item(FolderItem *item, gboolean update_summary)
{
	GList *list;
	FolderView *folderview;
	GtkCTree *ctree;
	GtkCTreeNode *node;

	g_return_if_fail(item != NULL);

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		ctree = GTK_CTREE(folderview->ctree);

		node = gtk_ctree_find_by_row_data(ctree, NULL, item);
		if (node) {
			folderview_update_node(folderview, node);
			if (update_summary && folderview->opened == node)
				summary_show(folderview->summaryview,
					     item, FALSE);
		}
	}
}

static void folderview_update_item_foreach_func(gpointer key, gpointer val,
						gpointer data)
{
	folderview_update_item((FolderItem *)key, FALSE);
}

void folderview_update_item_foreach(GHashTable *table)
{
	g_hash_table_foreach(table, folderview_update_item_foreach_func, NULL);
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
	if (GTK_CTREE_ROW(node)->children)
		gtk_ctree_expand(ctree, node);
}

#define SET_SPECIAL_FOLDER(ctree, item) \
{ \
	if (item) { \
		GtkCTreeNode *node, *sibling; \
 \
		node = gtk_ctree_find_by_row_data(ctree, root, item); \
		if (!node) \
			g_warning("%s not found.\n", item->path); \
		else { \
			if (!prev) \
				sibling = GTK_CTREE_ROW(root)->children; \
			else \
				sibling = GTK_CTREE_ROW(prev)->sibling; \
			if (node != sibling) \
				gtk_ctree_move(ctree, node, root, sibling); \
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

	gtk_ctree_sort_recursive(ctree, root);

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
		folderview_rename_folder_cb(folderview, 0, NULL);
		break;
	case F_IMAP:
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
		folderview_delete_folder_cb(folderview, 0, NULL);
		break;
	case F_IMAP:
		folderview_rm_imap_folder_cb(folderview, 0, NULL);
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

	menu_set_insensitive_all(GTK_MENU_SHELL(folderview->mail_popup));
	menu_set_insensitive_all(GTK_MENU_SHELL(folderview->imap_popup));
	menu_set_insensitive_all(GTK_MENU_SHELL(folderview->news_popup));
	menu_set_insensitive_all(GTK_MENU_SHELL(folderview->mbox_popup));

	if (folder->type == F_MH && item->parent == NULL) {
		menu_set_sensitive(folderview->mail_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->mail_factory,
				   "/Remove mailbox", TRUE);
	} else if (folder->type == F_MH && item->stype != F_NORMAL) {
		menu_set_sensitive(folderview->mail_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->mail_factory,
				   "/Scoring...", TRUE);
	} else if (folder->type == F_MH) {
		menu_set_sensitive(folderview->mail_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->mail_factory,
				   "/Rename folder...", TRUE);
		menu_set_sensitive(folderview->mail_factory,
				   "/Delete folder", TRUE);
		menu_set_sensitive(folderview->mail_factory,
				   "/Scoring...", TRUE);
	} else if (folder->type == F_IMAP && item->parent == NULL) {
		menu_set_sensitive(folderview->imap_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->imap_factory,
				   "/Remove IMAP4 server", TRUE);
	} else if (folder->type == F_IMAP && item->stype != F_NORMAL) {
		menu_set_sensitive(folderview->imap_factory,
				   "/Create new folder...", TRUE);
	} else if (folder->type == F_IMAP) {
		menu_set_sensitive(folderview->imap_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->imap_factory,
				   "/Delete folder", TRUE);
		menu_set_sensitive(folderview->imap_factory,
				   "/Scoring...", TRUE);
	} else if (folder->type == F_NEWS && item->parent == NULL) {
		menu_set_sensitive(folderview->news_factory,
				   "/Subscribe to newsgroup...", TRUE);
		menu_set_sensitive(folderview->news_factory,
				   "/Remove news server", TRUE);
	} else if (folder->type == F_NEWS) {
		menu_set_sensitive(folderview->news_factory,
				   "/Subscribe to newsgroup...", TRUE);
		menu_set_sensitive(folderview->news_factory,
				   "/Remove newsgroup", TRUE);
		menu_set_sensitive(folderview->news_factory,
				   "/Scoring...", TRUE);
	}
	if (folder->type == F_MBOX && item->parent == NULL) {
		menu_set_sensitive(folderview->mbox_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->mbox_factory,
				   "/Remove mailbox", TRUE);
	} else if (folder->type == F_MBOX && item->stype != F_NORMAL) {
		menu_set_sensitive(folderview->mbox_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->mbox_factory,
				   "/Scoring...", TRUE);
	} else if (folder->type == F_MBOX) {
		menu_set_sensitive(folderview->mbox_factory,
				   "/Create new folder...", TRUE);
		menu_set_sensitive(folderview->mbox_factory,
				   "/Rename folder...", TRUE);
		menu_set_sensitive(folderview->mbox_factory,
				   "/Delete folder", TRUE);
		menu_set_sensitive(folderview->mbox_factory,
				   "/Scoring...", TRUE);
	}

	if (folder->type == F_MH)
		gtk_menu_popup(GTK_MENU(folderview->mail_popup), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	else if (folder->type == F_IMAP)
		gtk_menu_popup(GTK_MENU(folderview->imap_popup), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	else if (folder->type == F_NEWS)
		gtk_menu_popup(GTK_MENU(folderview->news_popup), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	else if (folder->type == F_MBOX)
		gtk_menu_popup(GTK_MENU(folderview->mbox_popup), NULL, NULL,
			       NULL, NULL, event->button, event->time);
}

static void folderview_button_released(GtkWidget *ctree, GdkEventButton *event,
				       FolderView *folderview)
{
	if (!event) return;

	if (event->button == 1 && folderview->open_folder == FALSE &&
	    folderview->opened != NULL) {
		gtk_ctree_select(GTK_CTREE(ctree), folderview->opened);
		gtkut_ctree_set_focus_row(GTK_CTREE(ctree),
					  folderview->opened);
	}
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static void folderview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				   FolderView *folderview)
{
	if (!event) return;

	switch (event->keyval) {
	case GDK_Return:
	case GDK_space:
		if (folderview->selected) {
			folderview_select_node(folderview,
					       folderview->selected);
		}
		break;
	case GDK_v:
	case GDK_V:
	case GDK_g:
	case GDK_G:
	case GDK_x:
	case GDK_X:
	case GDK_w:
	case GDK_D:
	case GDK_Q:
		BREAK_ON_MODIFIER_KEY();
		summary_pass_key_press_event(folderview->summaryview, event);
	default:
	}
}

static void folderview_selected(GtkCTree *ctree, GtkCTreeNode *row,
				gint column, FolderView *folderview)
{
	static gboolean can_select = TRUE;	/* exclusive lock */
	gboolean opened;
	FolderItem *item;

	folderview->selected = row;

	if (folderview->opened == row) {
		folderview->open_folder = FALSE;
		return;
	}

	if (!can_select) {
		gtk_ctree_select(ctree, folderview->opened);
		gtkut_ctree_set_focus_row(ctree, folderview->opened);
		return;
	}

	if (!folderview->open_folder) return;

	item = gtk_ctree_node_get_row_data(ctree, row);
	if (!item) return;

	can_select = FALSE;

	if (item->path)
		debug_print(_("Folder %s is selected\n"), item->path);

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

	opened = summary_show(folderview->summaryview, item, FALSE);

	if (!opened) {
		gtk_ctree_select(ctree, folderview->opened);
		gtkut_ctree_set_focus_row(ctree, folderview->opened);
	} else
		folderview->opened = row;

	folderview->open_folder = FALSE;
	can_select = TRUE;
}

static void folderview_tree_expanded(GtkCTree *ctree, GtkCTreeNode *node,
				     FolderView *folderview)
{
	folderview_update_node(folderview, node);
}

static void folderview_tree_collapsed(GtkCTree *ctree, GtkCTreeNode *node,
				      FolderView *folderview)
{
	folderview_update_node(folderview, node);
}

static void folderview_popup_close(GtkMenuShell *menu_shell,
				   FolderView *folderview)
{
	if (!folderview->opened) return;

	gtk_ctree_select(GTK_CTREE(folderview->ctree), folderview->opened);
	gtkut_ctree_set_focus_row(GTK_CTREE(folderview->ctree),
				  folderview->opened);
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

static void folderview_new_folder_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	gchar *text[N_FOLDER_COLS] = {NULL, "0", "0", "0"};
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	GtkCTreeNode *node;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		g_free(new_folder);
		return;
	}

	/* find whether the directory already exists */
	if (folderview_find_by_name(ctree, folderview->selected, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."),
				 new_folder);
		g_free(new_folder);
		return;
	}

	new_item = item->folder->create_folder(item->folder, item, new_folder);
	g_free(new_folder);
	if (!new_item) return;

	gtk_clist_freeze(GTK_CLIST(ctree));

	text[COL_FOLDER] = new_item->name;
	node = gtk_ctree_insert_node(ctree, folderview->selected, NULL, text,
				     FOLDER_SPACING,
				     folderxpm, folderxpmmask,
				     folderopenxpm, folderopenxpmmask,
				     FALSE, FALSE);
	gtk_ctree_expand(ctree, folderview->selected);
	gtk_ctree_node_set_row_data(ctree, node, new_item);
	folderview_sort_folders(folderview, folderview->selected, item->folder);

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_new_mbox_folder_cb(FolderView *folderview, guint action,
					  GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	gchar *text[N_FOLDER_COLS] = {NULL, "0", "0", "0"};
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	GtkCTreeNode *node;

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

	new_item = item->folder->create_folder(item->folder, item, new_folder);
	g_free(new_folder);
	if (!new_item) return;

	gtk_clist_freeze(GTK_CLIST(ctree));

	text[COL_FOLDER] = new_item->name;
	node = gtk_ctree_insert_node(ctree, folderview->selected, NULL, text,
				     FOLDER_SPACING,
				     folderxpm, folderxpmmask,
				     folderopenxpm, folderopenxpmmask,
				     FALSE, FALSE);
	gtk_ctree_expand(ctree, folderview->selected);
	gtk_ctree_node_set_row_data(ctree, node, new_item);
	folderview_sort_folders(folderview, folderview->selected, item->folder);

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_rename_folder_cb(FolderView *folderview, guint action,
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

	message = g_strdup_printf(_("Input new name for `%s':"),
				  g_basename(item->path));
	new_folder = input_dialog(_("Rename folder"), message,
				  g_basename(item->path));
	g_free(message);
	if (!new_folder) return;

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("`%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		g_free(new_folder);
		return;
	}

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
		summary_show(folderview->summaryview, item, FALSE);
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

	message = g_strdup_printf(_("Input new name for `%s':"),
				  g_basename(item->path));
	new_folder = input_dialog(_("Rename folder"), message,
				  g_basename(item->path));
	g_free(message);
	if (!new_folder) return;

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
		summary_show(folderview->summaryview, item, FALSE);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_delete_folder_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	message = g_strdup_printf
		(_("All folder(s) and message(s) under `%s' will be deleted.\n"
		   "Do you really want to delete?"),
		 g_basename(item->path));
	avalue = alertpanel(_("Delete folder"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	if (item->folder->remove_folder(item->folder, item) < 0) {
		alertpanel_error(_("Can't remove the folder `%s'."),
				 item->path);
		return;
	}

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	gtk_ctree_remove_node(ctree, folderview->selected);
	folder_write_list();
}

static void folderview_remove_mailbox_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node;
	FolderItem *item;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;
	node = folderview->selected;
	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (item->parent) return;

	message = g_strdup_printf
		(_("Really remove the mailbox `%s' ?\n"
		   "(The messages are NOT deleted from disk)"),
		 item->folder->name);
	avalue = alertpanel(_("Remove folder"), message,
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
	gchar *text[N_FOLDER_COLS] = {NULL, "0", "0", "0"};
	GtkCTreeNode *node;
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
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

	/* find whether the directory already exists */
	if (folderview_find_by_name(ctree, folderview->selected, new_folder)) {
		alertpanel_error(_("The folder `%s' already exists."),
				 new_folder);
		g_free(new_folder);
		return;
	}

	new_item = item->folder->create_folder(item->folder, item, new_folder);
	g_free(new_folder);
	if (!new_item) return;

	gtk_clist_freeze(GTK_CLIST(ctree));

	text[COL_FOLDER] = new_item->name;
	node = gtk_ctree_insert_node(ctree, folderview->selected, NULL, text,
				     FOLDER_SPACING,
				     folderxpm, folderxpmmask,
				     folderopenxpm, folderopenxpmmask,
				     FALSE, FALSE);
	gtk_ctree_expand(ctree, folderview->selected);
	gtk_ctree_node_set_row_data(ctree, node, new_item);
	folderview_sort_folders(folderview, folderview->selected, item->folder);

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_rm_imap_folder_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_IMAP);
	g_return_if_fail(item->folder->account != NULL);

	message = g_strdup_printf(_("Really delete folder `%s'?"),
				  g_basename(item->path));
	avalue = alertpanel(_("Delete folder"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	if (item->folder->remove_folder(item->folder, item) < 0) {
		alertpanel_error(_("Can't remove the folder `%s'."),
				 item->path);
		return;
	}

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	gtk_ctree_remove_node(ctree, folderview->selected);
	folder_write_list();
}

static void folderview_rm_imap_server_cb(FolderView *folderview, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_IMAP);
	g_return_if_fail(item->folder->account != NULL);

	message = g_strdup_printf(_("Really delete IMAP4 server `%s'?"),
				  item->folder->name);
	avalue = alertpanel(_("Delete IMAP4 server"), message,
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

	account_destroy(item->folder->account);
	folder_destroy(item->folder);
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
	FolderItem *item;
	FolderItem *newitem;
	gchar *new_group;
	const gchar *server;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	new_group = grouplist_dialog(item);
	if (!new_group) return;

	if (GTK_CTREE_ROW(folderview->selected)->parent != NULL)
		servernode = GTK_CTREE_ROW(folderview->selected)->parent;
	else
		servernode = folderview->selected;

	if (folderview_find_by_name(ctree, servernode, new_group)) {
		alertpanel_error(_("The newsgroup `%s' already exists."),
				 new_group);
		g_free(new_group);
		return;
	}

	gtk_clist_freeze(GTK_CLIST(ctree));

	text[COL_FOLDER] = new_group;
	node = gtk_ctree_insert_node(ctree, servernode, NULL, text,
				     FOLDER_SPACING,
				     folderxpm, folderxpmmask,
				     folderopenxpm, folderopenxpmmask,
				     FALSE, FALSE);
	gtk_ctree_expand(ctree, servernode);

	item = gtk_ctree_node_get_row_data(ctree, servernode);
	server = item->folder->account->nntp_server;

	newitem = folder_item_new(new_group, new_group);
	g_free(new_group);
	folder_item_append(item, newitem);
	gtk_ctree_node_set_row_data(ctree, node, newitem);
	gtk_ctree_sort_node(ctree, servernode);

	gtk_clist_thaw(GTK_CLIST(ctree));

	folder_write_list();
}

static void folderview_rm_news_group_cb(FolderView *folderview, guint action,
					GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	message = g_strdup_printf(_("Really delete newsgroup `%s'?"),
				  g_basename(item->path));
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
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(item->folder->type == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	message = g_strdup_printf(_("Really delete news server `%s'?"),
				  item->folder->name);
	avalue = alertpanel(_("Delete news server"), message,
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

	account_destroy(item->folder->account);
	folder_destroy(item->folder);
	gtk_ctree_remove_node(ctree, folderview->selected);
	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview)
{
	gint row, column;
	FolderItem *item, *current_item;
	GtkCTreeNode *node = NULL;
	gboolean acceptable = FALSE;

	if (gtk_clist_get_selection_info(GTK_CLIST(widget),
					 x - 24, y - 24, &row, &column)) {
		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
		current_item = folderview->summaryview->folder_item;
		if (item != NULL &&
		    item->path != NULL &&
		    current_item != NULL &&
		    current_item != item) {
			switch (item->folder->type){
			case F_MH:
				if (current_item->folder->type == F_MH)
				    acceptable = TRUE;
				break;
			case F_IMAP:
				if (current_item->folder->account == item->folder->account)
				    acceptable = TRUE;
				break;
			default:
			}
		}
	}

	if (acceptable) {
		gtk_ctree_select(GTK_CTREE(widget), node);
		gdk_drag_status(context, context->suggested_action, time);
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
	FolderItem *item;
	GtkCTreeNode *node;

	if (gtk_clist_get_selection_info(GTK_CLIST(widget),
					 x - 24, y - 24, &row, &column) == 0)
		return;

	node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
	item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
	if (item != NULL) {
		summary_move_selected_to(folderview->summaryview, item);
		gtk_drag_finish(drag_context, TRUE, TRUE, time);
	} else
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
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
