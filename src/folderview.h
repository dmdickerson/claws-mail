/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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

#ifndef __FOLDERVIEW_H__
#define __FOLDERVIEW_H__

typedef struct _FolderView	FolderView;
typedef struct _FolderViewPopup	FolderViewPopup;

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>

#include "mainwindow.h"
#include "summaryview.h"
#include "folder.h"

struct _FolderView
{
	GtkWidget *scrolledwin;
	GtkWidget *ctree;

	GHashTable *popups;

	GtkCTreeNode *selected;
	GtkCTreeNode *opened;

	gboolean open_folder;

	GdkColor color_new;
	GdkColor color_op;

	MainWindow   *mainwin;
	SummaryView  *summaryview;

	gint folder_update_callback_id;
	gint folder_item_update_callback_id;
	
	/* DND states */
	GSList *nodes_to_recollapse;
	guint   drag_timer;		/* timer id */
	FolderItem *drag_item;		/* dragged item */
	GtkCTreeNode *drag_node;	/* drag node */
	
	GtkTargetList *target_list; /* DnD */
};

struct _FolderViewPopup
{
	gchar		 *klass;
	gchar		 *path;
	GSList		 *entries;
	void		(*set_sensitivity)	(GtkItemFactory *menu, FolderItem *item);
};

void folderview_initialize		(void);
FolderView *folderview_create		(void);
void folderview_init			(FolderView	*folderview);
void folderview_set			(FolderView	*folderview);
void folderview_set_all			(void);
void folderview_select			(FolderView	*folderview,
					 FolderItem	*item);
void folderview_unselect		(FolderView	*folderview);
FolderItem *folderview_get_selected	(FolderView 	*folderview);
void folderview_select_next_unread	(FolderView	*folderview);
void folderview_update_msg_num		(FolderView	*folderview,
					 GtkCTreeNode	*row);

void folderview_append_item		(FolderItem	*item);

void folderview_rescan_tree		(Folder		*folder);
void folderview_rescan_all		(void);
gint folderview_check_new		(Folder		*folder);
void folderview_check_new_all		(void);

void folderview_update_item_foreach	(GHashTable	*table,
					 gboolean	 update_summary);
void folderview_update_all_updated	(gboolean	 update_summary);

void folderview_move_folder		(FolderView 	*folderview,
					 FolderItem 	*from_folder,
					 FolderItem 	*to_folder);

void folderview_set_target_folder_color (gint		color_op);

void folderview_reflect_prefs_pixmap_theme	(FolderView *folderview);

void folderview_register_popup		(FolderViewPopup	*fpopup);
void folderview_unregister_popup	(FolderViewPopup	*fpopup);

#endif /* __FOLDERVIEW_H__ */
