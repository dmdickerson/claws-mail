/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#ifndef __SUMMARY_H__
#define __SUMMARY_H__

#include <regex.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkdnd.h>

typedef struct _SummaryView		SummaryView;
typedef struct _SummaryColumnState	SummaryColumnState;

typedef enum
{
	S_COL_MARK,
	S_COL_STATUS,
	S_COL_MIME,
	S_COL_SUBJECT,
	S_COL_FROM,
	S_COL_DATE,
	S_COL_SIZE,
	S_COL_NUMBER,
	S_COL_SCORE,
	S_COL_LOCKED
} SummaryColumnType;

#define N_SUMMARY_COLS	10

typedef enum
{
	SUMMARY_NONE,
	SUMMARY_SELECTED_NONE,
	SUMMARY_SELECTED_SINGLE,
	SUMMARY_SELECTED_MULTIPLE
} SummarySelection;

typedef enum
{
	TARGET_MAIL_URI_LIST,
	TARGET_DUMMY
} TargetInfo;

typedef enum
{
	S_SEARCH_SUBJECT,
	S_SEARCH_FROM,
	S_SEARCH_TO,
	S_SEARCH_EXTENDED
} SummarySearchType;

#include "mainwindow.h"
#include "folderview.h"
#include "headerview.h"
#include "messageview.h"
#include "compose.h"
#include "folder.h"
#include "gtksctree.h"
#include "prefs_filtering.h"

extern GtkTargetEntry summary_drag_types[1];

struct _SummaryColumnState
{
	SummaryColumnType type;
	gboolean visible;
};

struct _SummaryView
{
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *hbox;
	GtkWidget *hbox_l;
	GtkWidget *hbox_search;
	GtkWidget *folder_pixmap;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkWidget *toggle_search;
	GtkWidget *quick_search_pixmap;
	GtkWidget *popupmenu;
	GtkWidget *colorlabel_menu;
	GtkWidget *search_type_opt;
	GtkWidget *search_type;
	GtkWidget *search_string;
	GtkWidget *search_description;

	GtkItemFactory *popupfactory;

	GtkWidget *window;

	GtkCTreeNode *selected;
	GtkCTreeNode *displayed;

	gboolean display_msg;

	GdkColor color_important;
	SummaryColumnState col_state[N_SUMMARY_COLS];
	gint col_pos[N_SUMMARY_COLS];

	GdkColor color_marked;
	GdkColor color_dim;

	guint lock_count;

	MainWindow   *mainwin;
	FolderView   *folderview;
	HeaderView   *headerview;
	MessageView  *messageview;

	FolderItem *folder_item;

	/* summaryview prefs */
	gint important_score;
	FolderSortKey sort_key;
	FolderSortType sort_type;
	guint threaded;
	guint thread_collapsed;

	/* Extra data for summaryview */
	regex_t *simplify_subject_preg;

	/* current message status */
	gint   unreadmarked;
	off_t  total_size;
	gint   deleted;
	gint   moved;
	gint   copied;

/*
private:
*/
	/* table for looking up message-id */
	GHashTable *msgid_table;
	GHashTable *subject_table;

	/* list for moving/deleting messages */
	GSList *mlist;
	int msginfo_update_callback_id;

	GtkTargetList *target_list; /* DnD */
};

SummaryView	*summary_create(void);

void summary_init		  (SummaryView		*summaryview);
gboolean summary_show		  (SummaryView		*summaryview,
				   FolderItem		*fitem);
void summary_clear_list		  (SummaryView		*summaryview);
void summary_clear_all		  (SummaryView		*summaryview);

void summary_lock		  (SummaryView		*summaryview);
void summary_unlock		  (SummaryView		*summaryview);
gboolean summary_is_locked	  (SummaryView		*summaryview);

SummarySelection summary_get_selection_type	(SummaryView	*summaryview);
GSList *summary_get_selected_msg_list		(SummaryView	*summaryview);

void summary_select_prev_unread	  (SummaryView		*summaryview);
void summary_select_next_unread	  (SummaryView		*summaryview);
void summary_select_prev_new	  (SummaryView		*summaryview);
void summary_select_next_new	  (SummaryView		*summaryview);
void summary_select_prev_marked	  (SummaryView		*summaryview);
void summary_select_next_marked	  (SummaryView		*summaryview);
void summary_select_prev_labeled  (SummaryView		*summaryview);
void summary_select_next_labeled  (SummaryView		*summaryview);
void summary_select_by_msgnum	  (SummaryView		*summaryview,
				   guint		 msgnum);
guint summary_get_current_msgnum  (SummaryView		*summaryview);
void summary_select_node	  (SummaryView		*summaryview,
				   GtkCTreeNode		*node,
				   gboolean		 display_msg,
				   gboolean		 do_refresh);

void summary_thread_build	  (SummaryView		*summaryview);
void summary_unthread		  (SummaryView		*summaryview);

void summary_expand_threads	  (SummaryView		*summaryview);
void summary_collapse_threads	  (SummaryView		*summaryview);
void summary_toggle_ignore_thread (SummaryView		*summaryview);

void summary_filter		  (SummaryView		*summaryview);
void summary_filter_open	  (SummaryView		*summaryview,
				   PrefsFilterType	 type);

void summary_sort		  (SummaryView		*summaryview,
				   FolderSortKey	 sort_key,
				   FolderSortType	 sort_type);

void summary_delete		  (SummaryView		*summaryview);
void summary_delete_duplicated	  (SummaryView		*summaryview);

void summary_cancel               (SummaryView          *summaryview);

gboolean summary_execute	  (SummaryView		*summaryview);

void summary_attract_by_subject	  (SummaryView		*summaryview);

gint summary_write_cache	  (SummaryView		*summaryview);

void summary_pass_key_press_event (SummaryView		*summaryview,
				   GdkEventKey		*event);

void summary_display_msg_selected (SummaryView		*summaryview,
				   gboolean		 all_headers);
void summary_redisplay_msg	  (SummaryView		*summaryview);
void summary_open_msg		  (SummaryView		*summaryview);
void summary_view_source	  (SummaryView		*summaryview);
void summary_reedit		  (SummaryView		*summaryview);
void summary_step		  (SummaryView		*summaryview,
				   GtkScrollType	 type);
void summary_toggle_view	  (SummaryView		*summaryview);
void summary_set_marks_selected	  (SummaryView		*summaryview);

void summary_move_selected_to	  (SummaryView		*summaryview,
				   FolderItem		*to_folder);
void summary_move_to		  (SummaryView		*summaryview);
void summary_copy_selected_to	  (SummaryView		*summaryview,
				   FolderItem		*to_folder);
GSList *summary_get_selection	  (SummaryView 		*summaryview);
void summary_copy_to		  (SummaryView		*summaryview);
void summary_save_as		  (SummaryView		*summaryview);
void summary_print		  (SummaryView		*summaryview);
void summary_mark		  (SummaryView		*summaryview);
void summary_unmark		  (SummaryView		*summaryview);
void summary_mark_as_unread	  (SummaryView		*summaryview);
void summary_mark_as_read	  (SummaryView		*summaryview);
void summary_msgs_lock		  (SummaryView		*summaryview);
void summary_msgs_unlock	  (SummaryView		*summaryview);
void summary_mark_all_read	  (SummaryView		*summaryview);
void summary_add_address	  (SummaryView		*summaryview);
void summary_select_all		  (SummaryView		*summaryview);
void summary_unselect_all	  (SummaryView		*summaryview);
void summary_select_thread	  (SummaryView		*summaryview);

void summary_set_colorlabel	  (SummaryView		*summaryview,
				   guint		 labelcolor,
				   GtkWidget		*widget);
void summary_set_colorlabel_color (GtkCTree		*ctree,
				   GtkCTreeNode		*node,
				   guint		 labelcolor);

void summary_set_column_order	  (SummaryView		*summaryview);

#if 0 /* OLD PROCESSING */
void processing_apply();
#endif

void summary_toggle_show_read_messages
				  (SummaryView *summaryview);

void summary_toggle_view_real	  (SummaryView	*summaryview);

void summary_reflect_prefs_pixmap_theme
                                  (SummaryView *summaryview);

void summary_harvest_address      (SummaryView *summaryview);
void summary_set_prefs_from_folderitem
                                  (SummaryView *summaryview, FolderItem *item);
void summary_save_prefs_to_folderitem
                                  (SummaryView *summaryview, FolderItem *item);

#endif /* __SUMMARY_H__ */
