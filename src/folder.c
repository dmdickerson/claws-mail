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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
# include <unistd.h>
#endif
#include <stdlib.h>

#include "intl.h"
#include "folder.h"
#include "session.h"
#include "imap.h"
#include "news.h"
#include "mh.h"
#include "mbox_folder.h"
#include "utils.h"
#include "xml.h"
#include "codeconv.h"
#include "prefs.h"
#include "account.h"
#include "filtering.h"
#include "scoring.h"
#include "prefs_folder_item.h"
#include "procheader.h"
#include "statusbar.h"

/* Dependecies to be removed ?! */
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_folder_item.h"

static GList *folder_list = NULL;

static void folder_init		(Folder		*folder,
				 const gchar	*name);

static gboolean folder_read_folder_func	(GNode		*node,
					 gpointer	 data);
static gchar *folder_get_list_path	(void);
static void folder_write_list_recursive	(GNode		*node,
					 gpointer	 data);
static void folder_update_op_count_rec	(GNode		*node);


static void folder_get_persist_prefs_recursive
					(GNode *node, GHashTable *pptable);
static gboolean persist_prefs_free	(gpointer key, gpointer val, gpointer data);
void folder_item_read_cache		(FolderItem *item);
void folder_item_free_cache		(FolderItem *item);

Folder *folder_new(FolderType type, const gchar *name, const gchar *path)
{
	Folder *folder = NULL;
	FolderItem *item;
#ifdef WIN32
	gchar *Xname, *Xpath;
#endif

	name = name ? name : path;

#ifdef WIN32
	Xname = NULL;
	if (name){
		Xname = g_strdup(name);
		locale_from_utf8(&Xname);
	}

	Xpath = NULL;
	if (path){
		Xpath = g_strdup(path);
		/* locale_from_utf8(&Xpath); */
	}
#endif

	switch (type) {
	case F_MBOX:
#ifdef WIN32
		folder = mbox_folder_new(Xname, Xpath);
#else
		folder = mbox_folder_new(name, path);
#endif
		break;
	case F_MH:
#ifdef WIN32
		folder = mh_folder_new(Xname, Xpath);
#else
		folder = mh_folder_new(name, path);
#endif
		break;
	case F_IMAP:
#ifdef WIN32
		folder = imap_folder_new(Xname, Xpath);
#else
		folder = imap_folder_new(name, path);
#endif
		break;
	case F_NEWS:
#ifdef WIN32
		folder = news_folder_new(Xname, Xpath);
#else
		folder = news_folder_new(name, path);
#endif
		break;
	default:
		return NULL;
	}

	/* Create root folder item */
	item = folder_item_new(folder, name, NULL);
	item->folder = folder;
	folder->node = g_node_new(item);
	folder->data = NULL;

#ifdef WIN32
	if (Xname)
		g_free(Xname);
	if (Xpath)
		g_free(Xpath);
#endif

	return folder;
}

static void folder_init(Folder *folder, const gchar *name)
{
	g_return_if_fail(folder != NULL);

#ifdef WIN32
	{
		gchar *p_name;
		p_name = g_strdup(name);
		locale_to_utf8(&p_name);
		folder_set_name(folder, p_name);
		g_free(p_name);
	}
#else
	folder_set_name(folder, name);
#endif

	/* Init folder data */
	folder->account = NULL;
	folder->inbox = NULL;
	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;

	/* Init Folder functions */
	folder->item_new = NULL;
	folder->item_destroy = NULL;
	folder->fetch_msg = NULL;
	folder->fetch_msginfo = NULL;
	folder->fetch_msginfos = NULL;
	folder->get_num_list = NULL;
	folder->ui_func = NULL;
	folder->ui_func_data = NULL;
	folder->change_flags = NULL;
	folder->check_msgnum_validity = NULL;
}

void folder_local_folder_init(Folder *folder, const gchar *name,
			      const gchar *path)
{
	folder_init(folder, name);
	LOCAL_FOLDER(folder)->rootpath = g_strdup(path);
}

void folder_remote_folder_init(Folder *folder, const gchar *name,
			       const gchar *path)
{
	folder_init(folder, name);
	REMOTE_FOLDER(folder)->session = NULL;
}

void folder_destroy(Folder *folder)
{
	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->destroy != NULL);

	folder->destroy(folder);

	folder_list = g_list_remove(folder_list, folder);

	folder_tree_destroy(folder);
	g_free(folder->name);
	g_free(folder);
}

void folder_local_folder_destroy(LocalFolder *lfolder)
{
	g_return_if_fail(lfolder != NULL);

	g_free(lfolder->rootpath);
}

void folder_remote_folder_destroy(RemoteFolder *rfolder)
{
	g_return_if_fail(rfolder != NULL);

	if (rfolder->session)
		session_destroy(rfolder->session);
}

#if 0
Folder *mbox_folder_new(const gchar *name, const gchar *path)
{
	/* not yet implemented */
	return NULL;
}

Folder *maildir_folder_new(const gchar *name, const gchar *path)
{
	/* not yet implemented */
	return NULL;
}
#endif

FolderItem *folder_item_new(Folder *folder, const gchar *name, const gchar *path)
{
	FolderItem *item = NULL;
#ifdef WIN32
	gchar *Xname, *Xpath;

	Xname = NULL;
	Xpath = NULL;
	if (name){
		Xname = g_strdup(name);
		locale_to_utf8(&Xname);
	}
	if (path){
		Xpath = g_strdup(path);
		/* locale_to_utf8(&Xpath); */
	}
#endif

	if (folder->item_new) {
		item = folder->item_new(folder);
	} else {
		item = g_new0(FolderItem, 1);
	}

	g_return_val_if_fail(item != NULL, NULL);

	item->stype = F_NORMAL;
#ifdef WIN32
	item->name = Xname;
	item->path = Xpath;
#else
	item->name = g_strdup(name);
	item->path = g_strdup(path);
#endif
	item->mtime = 0;
	item->new = 0;
	item->unread = 0;
	item->total = 0;
	item->last_num = -1;
	item->cache = NULL;
	item->no_sub = FALSE;
	item->no_select = FALSE;
	item->collapsed = FALSE;
	item->threaded  = TRUE;
	item->ret_rcpt  = FALSE;
	item->opened    = FALSE;
	item->parent = NULL;
	item->folder = NULL;
	item->account = NULL;
	item->apply_sub = FALSE;
	item->mark_queue = NULL;
	item->data = NULL;
#ifdef WIN32
	item->n_child = calc_child(item->path);
#endif

	item->prefs = prefs_folder_item_new();

	return item;
}

void folder_item_append(FolderItem *parent, FolderItem *item)
{
	GNode *node;

	g_return_if_fail(parent != NULL);
	g_return_if_fail(parent->folder != NULL);
	g_return_if_fail(item != NULL);

	node = parent->folder->node;
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, parent);
	g_return_if_fail(node != NULL);

	item->parent = parent;
	item->folder = parent->folder;
	g_node_append_data(node, item);
}

void folder_item_remove(FolderItem *item)
{
	GNode *node;

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	node = item->folder->node;
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	g_return_if_fail(node != NULL);

	/* TODO: free all FolderItem's first */
	if (item->folder->node == node)
		item->folder->node = NULL;
	g_node_destroy(node);
}

void folder_item_destroy(FolderItem *item)
{
	g_return_if_fail(item != NULL);

	debug_print("Destroying folder item %s\n", item->path);

	if (item->cache)
		folder_item_free_cache(item);
	g_free(item->name);
	g_free(item->path);

	if (item->folder != NULL) {
		if(item->folder->item_destroy) {
			item->folder->item_destroy(item->folder, item);
		} else {
			g_free(item);
		}
	}
}

void folder_set_ui_func(Folder *folder, FolderUIFunc func, gpointer data)
{
	g_return_if_fail(folder != NULL);

	folder->ui_func = func;
	folder->ui_func_data = data;
}

void folder_set_name(Folder *folder, const gchar *name)
{
	g_return_if_fail(folder != NULL);

	g_free(folder->name);
	folder->name = name ? g_strdup(name) : NULL;
	if (folder->node && folder->node->data) {
		FolderItem *item = (FolderItem *)folder->node->data;

		g_free(item->name);
		item->name = name ? g_strdup(name) : NULL;
	}
}

gboolean folder_tree_destroy_func(GNode *node, gpointer data) {
	FolderItem *item = (FolderItem *) node->data;

	folder_item_destroy(item);
	return FALSE;
}

void folder_tree_destroy(Folder *folder)
{
	g_return_if_fail(folder != NULL);
	g_return_if_fail(folder->node != NULL);
	
	prefs_scoring_clear();
	prefs_filtering_clear();

	g_node_traverse(folder->node, G_POST_ORDER, G_TRAVERSE_ALL, -1, folder_tree_destroy_func, NULL);
	if (folder->node)
		g_node_destroy(folder->node);

	folder->inbox = NULL;
	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;
	folder->node = NULL;
}

void folder_add(Folder *folder)
{
	Folder *cur_folder;
	GList *cur;
	gint i;

	g_return_if_fail(folder != NULL);

	for (i = 0, cur = folder_list; cur != NULL; cur = cur->next, i++) {
		cur_folder = FOLDER(cur->data);
		if (folder->type == F_MH) {
			if (cur_folder->type != F_MH) break;
		} else if (folder->type == F_MBOX) {
			if (cur_folder->type != F_MH &&
			    cur_folder->type != F_MBOX) break;
		} else if (folder->type == F_IMAP) {
			if (cur_folder->type != F_MH &&
			    cur_folder->type != F_MBOX &&
			    cur_folder->type != F_IMAP) break;
		} else if (folder->type == F_NEWS) {
			if (cur_folder->type != F_MH &&
			    cur_folder->type != F_MBOX &&
			    cur_folder->type != F_IMAP &&
			    cur_folder->type != F_NEWS) break;
		}
	}

	folder_list = g_list_insert(folder_list, folder, i);
}

GList *folder_get_list(void)
{
	return folder_list;
}

gint folder_read_list(void)
{
	GNode *node;
	XMLNode *xmlnode;
	gchar *path;

	path = folder_get_list_path();
	if (!is_file_exist(path)) return -1;
	node = xml_parse_file(path);
	if (!node) return -1;

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folderlist") != 0) {
		g_warning("wrong folder list\n");
		xml_free_tree(node);
		return -1;
	}

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, 2,
			folder_read_folder_func, NULL);

	xml_free_tree(node);
	if (folder_list)
		return 0;
	else
		return -1;
}

void folder_write_list(void)
{
	GList *list;
	Folder *folder;
	gchar *path;
	PrefFile *pfile;

	path = folder_get_list_path();
	if ((pfile = prefs_write_open(path)) == NULL) return;

	fprintf(pfile->fp, "<?xml version=\"1.0\" encoding=\"%s\"?>\n",
		conv_get_current_charset_str());
	fputs("\n<folderlist>\n", pfile->fp);

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		folder_write_list_recursive(folder->node, pfile->fp);
	}

	fputs("</folderlist>\n", pfile->fp);

	if (prefs_write_close(pfile) < 0)
		g_warning("failed to write folder list.\n");
}

gboolean folder_scan_tree_func(GNode *node, gpointer data)
{
	GHashTable *pptable = (GHashTable *)data;
	FolderItem *item = (FolderItem *)node->data;
	
	folder_item_restore_persist_prefs(item, pptable);

	return FALSE;
}

void folder_scan_tree(Folder *folder)
{
	GHashTable *pptable;
	
	if (!folder->scan_tree)
		return;
	
	pptable = folder_persist_prefs_new(folder);
	folder_tree_destroy(folder);

	folder->scan_tree(folder);

	g_node_traverse(folder->node, G_POST_ORDER, G_TRAVERSE_ALL, -1, folder_scan_tree_func, pptable);
	folder_persist_prefs_free(pptable);

	prefs_matcher_read_config();

	folder_write_list();
}

FolderItem *folder_create_folder(FolderItem *parent, const gchar *name)
{
	FolderItem *new_item;

	new_item = parent->folder->create_folder(parent->folder, parent, name);
	if (new_item)
		new_item->cache = msgcache_new();

	return new_item;
}

struct TotalMsgCount
{
	guint new;
	guint unread;
	guint total;
};

struct FuncToAllFoldersData
{
	FolderItemFunc	function;
	gpointer	data;
};

static gboolean folder_func_to_all_folders_func(GNode *node, gpointer data)
{
	FolderItem *item;
	struct FuncToAllFoldersData *function_data = (struct FuncToAllFoldersData *) data;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);
	g_return_val_if_fail(item != NULL, FALSE);

	function_data->function(item, function_data->data);

	return FALSE;
}

void folder_func_to_all_folders(FolderItemFunc function, gpointer data)
{
	GList *list;
	Folder *folder;
	struct FuncToAllFoldersData function_data;
	
	function_data.function = function;
	function_data.data = data;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		if (folder->node)
			g_node_traverse(folder->node, G_PRE_ORDER,
					G_TRAVERSE_ALL, -1,
					folder_func_to_all_folders_func,
					&function_data);
	}
}

static void folder_count_total_msgs_func(FolderItem *item, gpointer data)
{
	struct TotalMsgCount *count = (struct TotalMsgCount *)data;

	count->new += item->new;
	count->unread += item->unread;
	count->total += item->total;
}

void folder_count_total_msgs(guint *new, guint *unread, guint *total)
{
	struct TotalMsgCount count;

	count.new = count.unread = count.total = 0;

	debug_print("Counting total number of messages...\n");

	folder_func_to_all_folders(folder_count_total_msgs_func, &count);

	*new = count.new;
	*unread = count.unread;
	*total = count.total;
}

Folder *folder_find_from_path(const gchar *path)
{
	GList *list;
	Folder *folder;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if ((folder->type == F_MH || folder->type == F_MBOX) &&
		    !path_cmp(LOCAL_FOLDER(folder)->rootpath, path))
			return folder;
	}

	return NULL;
}

Folder *folder_find_from_name(const gchar *name, FolderType type)
{
	GList *list;
	Folder *folder;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (folder->type == type && strcmp2(name, folder->name) == 0)
			return folder;
	}

	return NULL;
}

static gboolean folder_item_find_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gpointer *d = data;
	const gchar *path = d[0];

	if (path_cmp(path, item->path) != 0)
		return FALSE;

	d[1] = item;

	return TRUE;
}

FolderItem *folder_find_item_from_path(const gchar *path)
{
	Folder *folder;
	gpointer d[2];

	folder = folder_get_default_folder();
	g_return_val_if_fail(folder != NULL, NULL);

	d[0] = (gpointer)path;
	d[1] = NULL;
	g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_item_find_func, d);
	return d[1];
}

static const struct {
	gchar *str;
	FolderType type;
} type_str_table[] = {
	{"#mh"     , F_MH},
	{"#mbox"   , F_MBOX},
	{"#maildir", F_MAILDIR},
	{"#imap"   , F_IMAP},
	{"#news"   , F_NEWS}
};

static gchar *folder_get_type_string(FolderType type)
{
	gint i;

	for (i = 0; i < sizeof(type_str_table) / sizeof(type_str_table[0]);
	     i++) {
		if (type_str_table[i].type == type)
			return type_str_table[i].str;
	}

	return NULL;
}

static FolderType folder_get_type_from_string(const gchar *str)
{
	gint i;

	for (i = 0; i < sizeof(type_str_table) / sizeof(type_str_table[0]);
	     i++) {
		if (g_strcasecmp(type_str_table[i].str, str) == 0)
			return type_str_table[i].type;
	}

	return F_UNKNOWN;
}

gchar *folder_get_identifier(Folder *folder)
{
	gchar *type_str;

	g_return_val_if_fail(folder != NULL, NULL);

	type_str = folder_get_type_string(folder->type);
	return g_strconcat(type_str, "/", folder->name, NULL);
}

gchar *folder_item_get_identifier(FolderItem *item)
{
	gchar *id;
	gchar *folder_id;
#ifdef WIN32
	gchar *p_path;
#endif

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	folder_id = folder_get_identifier(item->folder);
#ifdef WIN32
	p_path = g_strdup(item->path);
	locale_to_utf8(&p_path);
	id = g_strconcat(folder_id, "/", p_path, NULL);
	g_free(p_path);
#else
	id = g_strconcat(folder_id, "/", item->path, NULL);
#endif
	g_free(folder_id);

	return id;
}

FolderItem *folder_find_item_from_identifier(const gchar *identifier)
{
	Folder *folder;
	gpointer d[2];
	gchar *str;
	gchar *p;
	gchar *name;
	gchar *path;
	FolderType type;

	g_return_val_if_fail(identifier != NULL, NULL);

	if (*identifier != '#')
		return folder_find_item_from_path(identifier);

	Xstrdup_a(str, identifier, return NULL);

	p = strchr(str, '/');
	if (!p)
		return folder_find_item_from_path(identifier);
	*p = '\0';
	p++;
	type = folder_get_type_from_string(str);
	if (type == F_UNKNOWN)
		return folder_find_item_from_path(identifier);

	name = p;
	p = strchr(p, '/');
	if (!p)
		return folder_find_item_from_path(identifier);
	*p = '\0';
	p++;

	folder = folder_find_from_name(name, type);
	if (!folder)
		return folder_find_item_from_path(identifier);

	path = p;

	d[0] = (gpointer)path;
	d[1] = NULL;
	g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_item_find_func, d);
	return d[1];
}

Folder *folder_get_default_folder(void)
{
	return folder_list ? FOLDER(folder_list->data) : NULL;
}

FolderItem *folder_get_default_inbox(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->inbox;
}

FolderItem *folder_get_default_outbox(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->outbox;
}

FolderItem *folder_get_default_draft(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->draft;
}

FolderItem *folder_get_default_queue(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->queue;
}

FolderItem *folder_get_default_trash(void)
{
	Folder *folder;

	if (!folder_list) return NULL;
	folder = FOLDER(folder_list->data);
	g_return_val_if_fail(folder != NULL, NULL);
	return folder->trash;
}

#define CREATE_FOLDER_IF_NOT_EXIST(member, dir, type)		\
{								\
	if (!folder->member) {					\
		item = folder_item_new(folder, dir, dir);	\
		item->stype = type;				\
		folder_item_append(rootitem, item);		\
		folder->member = item;				\
	}							\
}

void folder_set_missing_folders(void)
{
	Folder *folder;
	FolderItem *rootitem;
	FolderItem *item;
	GList *list;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (folder->type != F_MH) continue;
		rootitem = FOLDER_ITEM(folder->node->data);
		g_return_if_fail(rootitem != NULL);

		if (folder->inbox && folder->outbox && folder->draft &&
		    folder->queue && folder->trash)
			continue;

		if (folder->create_tree(folder) < 0) {
			g_warning("%s: can't create the folder tree.\n",
				  LOCAL_FOLDER(folder)->rootpath);
			continue;
		}

		CREATE_FOLDER_IF_NOT_EXIST(inbox,  INBOX_DIR,  F_INBOX);
		CREATE_FOLDER_IF_NOT_EXIST(outbox, OUTBOX_DIR, F_OUTBOX);
		CREATE_FOLDER_IF_NOT_EXIST(draft,  DRAFT_DIR,  F_DRAFT);
		CREATE_FOLDER_IF_NOT_EXIST(queue,  QUEUE_DIR,  F_QUEUE);
		CREATE_FOLDER_IF_NOT_EXIST(trash,  TRASH_DIR,  F_TRASH);
	}
}

static gboolean folder_unref_account_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	PrefsAccount *account = data;

	if (item->account == account)
		item->account = NULL;

	return FALSE;
}

void folder_unref_account_all(PrefsAccount *account)
{
	Folder *folder;
	GList *list;

	if (!account) return;

	for (list = folder_list; list != NULL; list = list->next) {
		folder = list->data;
		if (folder->account == account)
			folder->account = NULL;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				folder_unref_account_func, account);
	}
}

#undef CREATE_FOLDER_IF_NOT_EXIST

gchar *folder_get_path(Folder *folder)
{
	gchar *path;

	g_return_val_if_fail(folder != NULL, NULL);

	switch(FOLDER_TYPE(folder)) {

		case F_MH:
			path = g_strdup(LOCAL_FOLDER(folder)->rootpath);
			break;

		case F_IMAP:
			g_return_val_if_fail(folder->account != NULL, NULL);
			path = g_strconcat(get_imap_cache_dir(),
					   G_DIR_SEPARATOR_S,
					   folder->account->recv_server,
					   G_DIR_SEPARATOR_S,
					   folder->account->userid,
					   NULL);
			break;

		case F_NEWS:
			g_return_val_if_fail(folder->account != NULL, NULL);
			path = g_strconcat(get_news_cache_dir(),
					   G_DIR_SEPARATOR_S,
					   folder->account->nntp_server,
					   NULL);
			break;

		default:
			path = NULL;
			break;
	}
	
	return path;
}

gchar *folder_item_get_path(FolderItem *item)
{
	gchar *folder_path;
	gchar *path;

	g_return_val_if_fail(item != NULL, NULL);

	if(FOLDER_TYPE(item->folder) != F_MBOX) {
		folder_path = folder_get_path(item->folder);
		g_return_val_if_fail(folder_path != NULL, NULL);

#ifdef WIN32
		if (folder_path[0] == G_DIR_SEPARATOR || folder_path[1] == ':') {
#else
		if (folder_path[0] == G_DIR_SEPARATOR) {
#endif
			if (item->path)
				path = g_strconcat(folder_path, G_DIR_SEPARATOR_S,
						   item->path, NULL);
			else
				path = g_strdup(folder_path);
		} else {
			if (item->path)
				path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
						   folder_path, G_DIR_SEPARATOR_S,
						   item->path, NULL);
			else
				path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
						   folder_path, NULL);
		}

		g_free(folder_path);
	} else {
		gchar *itempath;

		itempath = mbox_get_virtual_path(item);
		if (itempath == NULL)
			return NULL;
		path = g_strconcat(get_mbox_cache_dir(),
					  G_DIR_SEPARATOR_S, itempath, NULL);
		g_free(itempath);
	}
	return path;
}

void folder_item_set_default_flags(FolderItem *dest, MsgFlags *flags)
{
	if (!(dest->stype == F_OUTBOX ||
	      dest->stype == F_QUEUE  ||
	      dest->stype == F_DRAFT  ||
	      dest->stype == F_TRASH)) {
		flags->perm_flags = MSG_NEW|MSG_UNREAD;
	} else {
		flags->perm_flags = 0;
	}
	flags->tmp_flags = MSG_CACHED;
	if (dest->folder->type == F_MH) {
		if (dest->stype == F_QUEUE) {
			MSG_SET_TMP_FLAGS(*flags, MSG_QUEUED);
		} else if (dest->stype == F_DRAFT) {
			MSG_SET_TMP_FLAGS(*flags, MSG_DRAFT);
		}
	} else if (dest->folder->type == F_IMAP) {
		MSG_SET_TMP_FLAGS(*flags, MSG_IMAP);
	} else if (dest->folder->type == F_NEWS) {
		MSG_SET_TMP_FLAGS(*flags, MSG_NEWS);
	}
}

static gint folder_sort_cache_list_by_msgnum(gconstpointer a, gconstpointer b)
{
	MsgInfo *msginfo_a = (MsgInfo *) a;
	MsgInfo *msginfo_b = (MsgInfo *) b;

	return (msginfo_a->msgnum - msginfo_b->msgnum);
}

static gint folder_sort_folder_list(gconstpointer a, gconstpointer b)
{
	guint gint_a = GPOINTER_TO_INT(a);
	guint gint_b = GPOINTER_TO_INT(b);
	
	return (gint_a - gint_b);
}

gint folder_item_open(FolderItem *item)
{
	if(((item->folder->type == F_IMAP) && !item->no_select) || (item->folder->type == F_NEWS)) {
		folder_item_scan(item);
	}

	/* Processing */
	if(item->prefs->processing != NULL) {
		gchar *buf;
		
		buf = g_strdup_printf(_("Processing (%s)...\n"), item->path);
		debug_print("%s\n", buf);
		g_free(buf);
	
		folder_item_apply_processing(item);

		debug_print("done.\n");
	}

	return 0;
}

void folder_item_close(FolderItem *item)
{
	GSList *mlist, *cur;
	
	g_return_if_fail(item != NULL);
	
	mlist = folder_item_get_msg_list(item);
	
	for (cur = mlist ; cur != NULL ; cur = cur->next) {
		MsgInfo * msginfo;

		msginfo = (MsgInfo *) cur->data;
		if (MSG_IS_NEW(msginfo->flags))
			procmsg_msginfo_unset_flags(msginfo, MSG_NEW, 0);
		procmsg_msginfo_free(msginfo);
	}
	
	folder_update_item(item, FALSE);

	g_slist_free(mlist);
}

gint folder_item_scan(FolderItem *item)
{
	Folder *folder;
	GSList *folder_list = NULL, *cache_list = NULL, *folder_list_cur, *cache_list_cur, *new_list = NULL;
	guint newcnt = 0, unreadcnt = 0, totalcnt = 0;
	guint cache_max_num, folder_max_num, cache_cur_num, folder_cur_num;
	gboolean contentchange = FALSE;
    
	g_return_val_if_fail(item != NULL, -1);
	if (item->path == NULL) return -1;

	folder = item->folder;

	g_return_val_if_fail(folder != NULL, -1);
	g_return_val_if_fail(folder->get_num_list != NULL, -1);

	debug_print("Scanning folder %s for cache changes.\n", item->path);

	/* Get list of messages for folder and cache */
	if (folder->get_num_list(item->folder, item, &folder_list) < 0) {
		debug_print("Error fetching list of message numbers\n");
		return(-1);
	}

	if (!folder->check_msgnum_validity || 
	    folder->check_msgnum_validity(folder, item)) {
		if (!item->cache)
			folder_item_read_cache(item);
		cache_list = msgcache_get_msg_list(item->cache);
	} else {
		if (item->cache)
			msgcache_destroy(item->cache);
		item->cache = msgcache_new();
		cache_list = NULL;
	}

	/* Sort both lists */
    	cache_list = g_slist_sort(cache_list, folder_sort_cache_list_by_msgnum);
	folder_list = g_slist_sort(folder_list, folder_sort_folder_list);

	cache_list_cur = cache_list;
	folder_list_cur = folder_list;

	if (cache_list_cur != NULL) {
		GSList *cache_list_last;
	
		cache_cur_num = ((MsgInfo *)cache_list_cur->data)->msgnum;
		cache_list_last = g_slist_last(cache_list);
		cache_max_num = ((MsgInfo *)cache_list_last->data)->msgnum;
	} else {
		cache_cur_num = G_MAXINT;
		cache_max_num = 0;
	}

	if (folder_list_cur != NULL) {
		GSList *folder_list_last;
	
		folder_cur_num = GPOINTER_TO_INT(folder_list_cur->data);
		folder_list_last = g_slist_last(folder_list);
		folder_max_num = GPOINTER_TO_INT(folder_list_last->data);
	} else {
		folder_cur_num = G_MAXINT;
		folder_max_num = 0;
	}

	while ((cache_cur_num != G_MAXINT) || (folder_cur_num != G_MAXINT)) {
		/*
		 *  Message only exists in the folder
		 *  Remember message for fetching
		 */
		if (folder_cur_num < cache_cur_num) {
			gboolean add = FALSE;

			switch(folder->type) {
				case F_NEWS:
					if (folder_cur_num < cache_max_num)
						break;
					
					if (prefs_common.max_articles == 0) {
						add = TRUE;
					}

					if (folder_max_num <= prefs_common.max_articles) {
						add = TRUE;
					} else if (folder_cur_num > (folder_max_num - prefs_common.max_articles)) {
						add = TRUE;
					}
					break;
				default:
					add = TRUE;
					break;
			}
			
			if (add) {
				new_list = g_slist_prepend(new_list, GINT_TO_POINTER(folder_cur_num));
				debug_print("Remembered message %d for fetching\n", folder_cur_num);
			}

			/* Move to next folder number */
			folder_list_cur = folder_list_cur->next;

			if (folder_list_cur != NULL)
				folder_cur_num = GPOINTER_TO_INT(folder_list_cur->data);
			else
				folder_cur_num = G_MAXINT;

			contentchange = TRUE;

			continue;
		}

		/*
		 *  Message only exists in the cache
		 *  Remove the message from the cache
		 */
		if (cache_cur_num < folder_cur_num) {
			msgcache_remove_msg(item->cache, cache_cur_num);
			debug_print("Removed message %d from cache.\n", cache_cur_num);

			/* Move to next cache number */
			cache_list_cur = cache_list_cur->next;

			if (cache_list_cur != NULL)
				cache_cur_num = ((MsgInfo *)cache_list_cur->data)->msgnum;
			else
				cache_cur_num = G_MAXINT;

			contentchange = TRUE;

			continue;
		}

		/*
		 *  Message number exists in folder and cache!
		 *  Check if the message has been modified
		 */
		if (cache_cur_num == folder_cur_num) {
			MsgInfo *msginfo;

			msginfo = msgcache_get_msg(item->cache, folder_cur_num);
			if (folder->is_msg_changed && folder->is_msg_changed(folder, item, msginfo)) {
				MsgInfo *newmsginfo;

				msgcache_remove_msg(item->cache, msginfo->msgnum);

				if (NULL != (newmsginfo = folder->fetch_msginfo(folder, item, folder_cur_num))) {
					msgcache_add_msg(item->cache, newmsginfo);
					if (MSG_IS_NEW(newmsginfo->flags) && !MSG_IS_IGNORE_THREAD(newmsginfo->flags))
						newcnt++;
					if (MSG_IS_UNREAD(newmsginfo->flags) && !MSG_IS_IGNORE_THREAD(newmsginfo->flags))
						unreadcnt++;
					procmsg_msginfo_free(newmsginfo);
				}					

				debug_print("Updated msginfo for message %d.\n", folder_cur_num);
			} else {
				if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
					newcnt++;
				if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
					unreadcnt++;
			}
			totalcnt++;
			procmsg_msginfo_free(msginfo);

			/* Move to next folder and cache number */
			cache_list_cur = cache_list_cur->next;
			folder_list_cur = folder_list_cur->next;

			if (cache_list_cur != NULL)
				cache_cur_num = ((MsgInfo *)cache_list_cur->data)->msgnum;
			else
				cache_cur_num = G_MAXINT;

			if (folder_list_cur != NULL)
				folder_cur_num = GPOINTER_TO_INT(folder_list_cur->data);
			else
				folder_cur_num = G_MAXINT;

			contentchange = TRUE;

			continue;
		}
	}

	for(cache_list_cur = cache_list; cache_list_cur != NULL; cache_list_cur = g_slist_next(cache_list_cur)) {
		procmsg_msginfo_free((MsgInfo *) cache_list_cur->data);
	}

	g_slist_free(cache_list);
	g_slist_free(folder_list);

	if (folder->fetch_msginfos) {
		GSList *elem;
		GSList *newmsg_list;
		MsgInfo *msginfo;
		
		if (new_list) {
			newmsg_list = folder->fetch_msginfos(folder, item, new_list);
			for (elem = newmsg_list; elem != NULL; elem = g_slist_next(elem)) {
				msginfo = (MsgInfo *) elem->data;
				msgcache_add_msg(item->cache, msginfo);
				if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
					newcnt++;
				if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
					unreadcnt++;
				totalcnt++;
				procmsg_msginfo_free(msginfo);
			}
			g_slist_free(newmsg_list);
		}
	} else if (folder->fetch_msginfo) {
		GSList *elem;
	
		for (elem = new_list; elem != NULL; elem = g_slist_next(elem)) {
			MsgInfo *msginfo;
			guint num;

			num = GPOINTER_TO_INT(elem->data);
			msginfo = folder->fetch_msginfo(folder, item, num);
			if (msginfo != NULL) {
				msgcache_add_msg(item->cache, msginfo);
				if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
				    newcnt++;
				if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
				    unreadcnt++;
				totalcnt++;
				procmsg_msginfo_free(msginfo);
				debug_print("Added newly found message %d to cache.\n", num);
			}
		}
	}

	item->new = newcnt;
	item->unread = unreadcnt;
	item->total = totalcnt;
	
	g_slist_free(new_list);

	folder_update_item(item, contentchange);

	return 0;
}

static void folder_item_scan_foreach_func(gpointer key, gpointer val,
					  gpointer data)
{
	folder_item_scan(FOLDER_ITEM(key));
}

void folder_item_scan_foreach(GHashTable *table)
{
	g_hash_table_foreach(table, folder_item_scan_foreach_func, NULL);
}

void folder_count_total_cache_memusage(FolderItem *item, gpointer data)
{
	gint *memusage = (gint *)data;

	if (item->cache == NULL)
		return;
	
	*memusage += msgcache_get_memory_usage(item->cache);
}

gint folder_cache_time_compare_func(gconstpointer a, gconstpointer b)
{
	FolderItem *fa = (FolderItem *)a;
	FolderItem *fb = (FolderItem *)b;
	
	return (gint) (msgcache_get_last_access_time(fa->cache) - msgcache_get_last_access_time(fb->cache));
}

void folder_find_expired_caches(FolderItem *item, gpointer data)
{
	GSList **folder_item_list = (GSList **)data;
	gint difftime, expiretime;
	
	if (item->cache == NULL)
		return;

	if (item->opened > 0)
		return;

	difftime = (gint) (time(NULL) - msgcache_get_last_access_time(item->cache));
	expiretime = prefs_common.cache_min_keep_time * 60;
	debug_print("Cache unused time: %d (Expire time: %d)\n", difftime, expiretime);
	if (difftime > expiretime) {
		*folder_item_list = g_slist_insert_sorted(*folder_item_list, item, folder_cache_time_compare_func);
	}
}

void folder_item_free_cache(FolderItem *item)
{
	g_return_if_fail(item != NULL);
	
	if (item->cache == NULL)
		return;
	
	if (item->opened > 0)
		return;

	folder_item_write_cache(item);
	msgcache_destroy(item->cache);
	item->cache = NULL;
}

void folder_clean_cache_memory()
{
	gint memusage = 0;

	folder_func_to_all_folders(folder_count_total_cache_memusage, &memusage);	
	debug_print("Total cache memory usage: %d\n", memusage);
	
	if (memusage > (prefs_common.cache_max_mem_usage * 1024)) {
		GSList *folder_item_list = NULL, *listitem;
		
		debug_print("Trying to free cache memory\n");

		folder_func_to_all_folders(folder_find_expired_caches, &folder_item_list);	
		listitem = folder_item_list;
		while((listitem != NULL) && (memusage > (prefs_common.cache_max_mem_usage * 1024))) {
			FolderItem *item = (FolderItem *)(listitem->data);

			debug_print("Freeing cache memory for %s\n", item->path);
			memusage -= msgcache_get_memory_usage(item->cache);
		        folder_item_free_cache(item);
			listitem = listitem->next;
		}
		g_slist_free(folder_item_list);
	}
}

void folder_item_read_cache(FolderItem *item)
{
	gchar *cache_file, *mark_file;
	
	g_return_if_fail(item != NULL);

	cache_file = folder_item_get_cache_file(item);
	mark_file = folder_item_get_mark_file(item);
	item->cache = msgcache_read_cache(item, cache_file);
	if (!item->cache) {
		item->cache = msgcache_new();
		folder_item_scan(item);
	}
	msgcache_read_mark(item->cache, mark_file);
	g_free(cache_file);
	g_free(mark_file);

	folder_clean_cache_memory();
}

void folder_item_write_cache(FolderItem *item)
{
	gchar *cache_file, *mark_file;
	PrefsFolderItem *prefs;
	gint filemode = 0;
	gchar *id;
	
	if (!item || !item->path || !item->cache)
		return;

	id = folder_item_get_identifier(item);
#ifdef WIN32
	locale_from_utf8(&id);
#endif
	debug_print("Save cache for folder %s\n", id);
	g_free(id);

	cache_file = folder_item_get_cache_file(item);
	mark_file = folder_item_get_mark_file(item);
	if (msgcache_write(cache_file, mark_file, item->cache) < 0) {
		prefs = item->prefs;
    		if (prefs && prefs->enable_folder_chmod && prefs->folder_chmod) {
			/* for cache file */
			filemode = prefs->folder_chmod;
			if (filemode & S_IRGRP) filemode |= S_IWGRP;
			if (filemode & S_IROTH) filemode |= S_IWOTH;
			chmod(cache_file, filemode);
		}
        }

	g_free(cache_file);
	g_free(mark_file);
}

MsgInfo *folder_item_fetch_msginfo(FolderItem *item, gint num)
{
	Folder *folder;
	MsgInfo *msginfo;
	
	g_return_val_if_fail(item != NULL, NULL);
	
	folder = item->folder;
	if (!item->cache)
		folder_item_read_cache(item);
	
	if ((msginfo = msgcache_get_msg(item->cache, num)) != NULL)
		return msginfo;
	
	g_return_val_if_fail(folder->fetch_msginfo, NULL);
	if ((msginfo = folder->fetch_msginfo(folder, item, num)) != NULL) {
		msgcache_add_msg(item->cache, msginfo);
		return msginfo;
	}
	
	return NULL;
}

MsgInfo *folder_item_fetch_msginfo_by_id(FolderItem *item, const gchar *msgid)
{
	Folder *folder;
	MsgInfo *msginfo;
	
	g_return_val_if_fail(item != NULL, NULL);
	
	folder = item->folder;
	if (!item->cache)
		folder_item_read_cache(item);
	
	if ((msginfo = msgcache_get_msg_by_id(item->cache, msgid)) != NULL)
		return msginfo;

	return NULL;
}

GSList *folder_item_get_msg_list(FolderItem *item)
{
	g_return_val_if_fail(item != NULL, NULL);
	
	if (item->cache == 0)
		folder_item_read_cache(item);

	g_return_val_if_fail(item->cache != NULL, NULL);
	
	return msgcache_get_msg_list(item->cache);
}

gchar *folder_item_fetch_msg(FolderItem *item, gint num)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, NULL);

	folder = item->folder;

	g_return_val_if_fail(folder->fetch_msg != NULL, NULL);

	return folder->fetch_msg(folder, item, num);
}

gint folder_item_add_msg(FolderItem *dest, const gchar *file,
			 gboolean remove_source)
{
	Folder *folder;
	gint num;
	MsgInfo *msginfo;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	folder = dest->folder;

	g_return_val_if_fail(folder->add_msg != NULL, -1);

	if (!dest->cache)
		folder_item_read_cache(dest);

	num = folder->add_msg(folder, dest, file, remove_source);

        if (num > 0) {
    		msginfo = folder->fetch_msginfo(folder, dest, num);

		if (msginfo != NULL) {
			if (MSG_IS_NEW(msginfo->flags))
			        dest->new++;
		        if (MSG_IS_UNREAD(msginfo->flags))
				dest->unread++;
			dest->total++;
			dest->need_update = TRUE;

            		msgcache_add_msg(dest->cache, msginfo);

    			procmsg_msginfo_free(msginfo);
		}

                dest->last_num = num;
        }

	return num;
}

/*
gint folder_item_move_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->move_msg(folder, dest, msginfo);
	if (num > 0) dest->last_num = num;

	return num;
}
*/
		
FolderItem *folder_item_move_recursive (FolderItem *src, FolderItem *dest) 
{
	GSList *mlist;
	GSList *cur;
	FolderItem *new_item;
	FolderItem *next_item;
	GNode *srcnode;
	int cnt = 0;
	mlist = folder_item_get_msg_list(src);

	/* move messages */
	debug_print("Moving %s to %s\n", src->path, dest->path);
	new_item = folder_create_folder(dest, g_basename(src->path));
	if (new_item == NULL) {
		printf("Can't create folder\n");
		return NULL;
	}
	
	statusbar_print_all(_("Moving %s to %s..."), src->name, new_item->path);

	if (new_item->folder == NULL)
		new_item->folder = dest->folder;

	/* move messages */
	for (cur = mlist ; cur != NULL ; cur = cur->next) {
		MsgInfo * msginfo;
		cnt++;
		if (cnt%500)
			statusbar_print_all(_("Moving %s to %s (%d%%)..."), src->name, 
					new_item->path,
					100*cnt/g_slist_length(mlist));
		msginfo = (MsgInfo *) cur->data;
		folder_item_move_msg(new_item, msginfo);
		if (cnt%500)
			statusbar_pop_all();
	}
	
	/*copy prefs*/
	prefs_folder_item_copy_prefs(src, new_item);
	new_item->collapsed = src->collapsed;
	new_item->threaded  = src->threaded;
	new_item->ret_rcpt  = src->ret_rcpt;
	new_item->hide_read_msgs = src->hide_read_msgs;
	new_item->sort_key  = src->sort_key;
	new_item->sort_type = src->sort_type;

	prefs_matcher_write_config();
	
	/* recurse */
	srcnode = src->folder->node;	
	srcnode = g_node_find(srcnode, G_PRE_ORDER, G_TRAVERSE_ALL, src);
	srcnode = srcnode->children;
	while (srcnode != NULL) {
		if (srcnode && srcnode->data) {
			next_item = (FolderItem*) srcnode->data;
			if (folder_item_move_recursive(next_item, new_item) == NULL)
				return NULL;
		}
		srcnode = srcnode->next;
	}
	return new_item;
}

FolderItem *folder_item_move_to(FolderItem *src, FolderItem *dest)
{
	FolderItem *tmp = dest->parent;
	char * src_identifier, * dst_identifier, * new_identifier;
	char * phys_srcpath, * phys_dstpath;
	GNode *src_node;
	
	while (tmp) {
		if (tmp == src) {
			alertpanel_error(_("Can't move a folder to one of its children."));
			return NULL;
		}
		tmp = tmp->parent;
	}
	
	tmp = src->parent;
	
	src_identifier = folder_item_get_identifier(src);
	dst_identifier = folder_item_get_identifier(dest);
	
	if(dst_identifier == NULL && dest->folder && dest->parent == NULL) {
		/* dest can be a root folder */
		dst_identifier = folder_get_identifier(dest->folder);
	}
	if (src_identifier == NULL || dst_identifier == NULL) {
		printf("Can't get identifiers\n");
		return NULL;
	}

	phys_srcpath = folder_item_get_path(src);
	phys_dstpath = g_strconcat(folder_item_get_path(dest),G_DIR_SEPARATOR_S,g_basename(phys_srcpath),NULL);

	if (src->parent == dest) {
		alertpanel_error(_("Source and destination are the same."));
		g_free(src_identifier);
		g_free(dst_identifier);
		g_free(phys_srcpath);
		g_free(phys_dstpath);
		return NULL;
	}
	debug_print("moving \"%s\" to \"%s\"\n", phys_srcpath, phys_dstpath);
	if ((tmp = folder_item_move_recursive(src, dest)) == NULL) {
		alertpanel_error(_("Move failed!"));
		return NULL;
	}
	
	/* update rules */
	src->folder->remove_folder(src->folder, src);
	src_node = g_node_find(src->folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, src);
	if (src_node) 
		g_node_destroy(src_node);
	else
		debug_print("can't remove node: it's null!\n");
	/* not to much worry if remove fails, move has been done */
	
	debug_print("updating rules ....\n");
	new_identifier = g_strconcat(dst_identifier, G_DIR_SEPARATOR_S, g_basename(phys_srcpath), NULL);
	prefs_filtering_rename_path(src_identifier, new_identifier);

	folder_write_list();

	g_free(src_identifier);
	g_free(dst_identifier);
	g_free(new_identifier);
	g_free(phys_srcpath);
	g_free(phys_dstpath);

	return tmp;
}

gint folder_item_move_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;
	gint num;
	Folder *src_folder;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	folder = dest->folder;

	g_return_val_if_fail(folder->remove_msg != NULL, -1);
	g_return_val_if_fail(folder->copy_msg != NULL, -1);

	if (!dest->cache) folder_item_read_cache(dest);

	src_folder = msginfo->folder->folder;

	num = folder->copy_msg(folder, dest, msginfo);
	
	if (num != -1) {
		MsgInfo *newmsginfo;
    
		/* Add new msginfo to dest folder */
		if (NULL != (newmsginfo = folder->fetch_msginfo(folder, dest, num))) {
			newmsginfo->flags.perm_flags = msginfo->flags.perm_flags;
			
			if (dest->stype == F_OUTBOX || dest->stype == F_QUEUE  ||
			    dest->stype == F_DRAFT  || dest->stype == F_TRASH)
				MSG_UNSET_PERM_FLAGS(newmsginfo->flags,
						     MSG_NEW|MSG_UNREAD|MSG_DELETED);
			msgcache_add_msg(dest->cache, newmsginfo);

			if (MSG_IS_NEW(newmsginfo->flags))
				dest->new++;
			if (MSG_IS_UNREAD(newmsginfo->flags))
				dest->unread++;
			dest->total++;
			dest->need_update = TRUE;

			procmsg_msginfo_free(newmsginfo);
		}

		/* remove source message from it's folder */
		if (src_folder->remove_msg) {
			src_folder->remove_msg(src_folder, msginfo->folder,
					       msginfo->msgnum);
			msgcache_remove_msg(msginfo->folder->cache, msginfo->msgnum);

			if (MSG_IS_NEW(msginfo->flags))
				msginfo->folder->new--;
			if (MSG_IS_UNREAD(msginfo->flags))
				msginfo->folder->unread--;
			msginfo->folder->total--;
			msginfo->folder->need_update = TRUE;
		}
	}
	
	if (folder->finished_copy)
		folder->finished_copy(folder, dest);

	return num;
}

/*
gint folder_item_move_msgs_with_dest(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->move_msgs_with_dest(folder, dest, msglist);
	if (num > 0) dest->last_num = num;
	else dest->op_count = 0;

	return num;
}
*/

gint folder_item_move_msgs_with_dest(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	FolderItem *item;
	GSList *newmsgnums = NULL;
	GSList *l, *l2;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	folder = dest->folder;

	g_return_val_if_fail(folder->copy_msg != NULL, -1);
	g_return_val_if_fail(folder->remove_msg != NULL, -1);

	/* 
	 * Copy messages to destination folder and 
	 * store new message numbers in newmsgnums
	 */
	item = NULL;
	for (l = msglist ; l != NULL ; l = g_slist_next(l)) {
		MsgInfo * msginfo = (MsgInfo *) l->data;

		if (!item && msginfo->folder != NULL)
			item = msginfo->folder;

		num = folder->copy_msg(folder, dest, msginfo);
		newmsgnums = g_slist_append(newmsgnums, GINT_TO_POINTER(num));
	}

	/* Read cache for dest folder */
	if (!dest->cache) folder_item_read_cache(dest);
	
	/* 
	 * Fetch new MsgInfos for new messages in dest folder,
	 * add them to the msgcache and update folder message counts
	 */
	l2 = newmsgnums;
	for (l = msglist; l != NULL; l = g_slist_next(l)) {
		MsgInfo *msginfo = (MsgInfo *) l->data;

		num = GPOINTER_TO_INT(l2->data);

		if (num != -1) {
			MsgInfo *newmsginfo;

			newmsginfo = folder->fetch_msginfo(folder, dest, num);
			if (newmsginfo) {
				newmsginfo->flags.perm_flags = msginfo->flags.perm_flags;
				if (dest->stype == F_OUTBOX ||
				    dest->stype == F_QUEUE  ||
				    dest->stype == F_DRAFT  ||
				    dest->stype == F_TRASH)
					MSG_UNSET_PERM_FLAGS(newmsginfo->flags,
							     MSG_NEW|MSG_UNREAD|MSG_DELETED);
				msgcache_add_msg(dest->cache, newmsginfo);

				if (MSG_IS_NEW(newmsginfo->flags))
					dest->new++;
				if (MSG_IS_UNREAD(newmsginfo->flags))
					dest->unread++;
				dest->total++;
				dest->need_update = TRUE;

				procmsg_msginfo_free(newmsginfo);
			}
		}
		l2 = g_slist_next(l2);
	}

	/*
	 * Remove source messages from their folders if
	 * copying was successfull and update folder
	 * message counts
	 */
	l2 = newmsgnums;
	for (l = msglist; l != NULL; l = g_slist_next(l)) {
		MsgInfo *msginfo = (MsgInfo *) l->data;

		num = GPOINTER_TO_INT(l2->data);
		
		if (num != -1) {
			item->folder->remove_msg(item->folder,
						 msginfo->folder,
						 msginfo->msgnum);
			if (!item->cache)
				folder_item_read_cache(item);
			msgcache_remove_msg(item->cache, msginfo->msgnum);

			if (MSG_IS_NEW(msginfo->flags))
				msginfo->folder->new--;
			if (MSG_IS_UNREAD(msginfo->flags))
				msginfo->folder->unread--;
			msginfo->folder->total--;			
			msginfo->folder->need_update = TRUE;
		}

		l2 = g_slist_next(l2);
	}


	if (folder->finished_copy)
		folder->finished_copy(folder, dest);

	g_slist_free(newmsgnums);
	return dest->last_num;
}

/*
gint folder_item_copy_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->copy_msg(folder, dest, msginfo);
	if (num > 0) dest->last_num = num;

	return num;
}
*/

gint folder_item_copy_msg(FolderItem *dest, MsgInfo *msginfo)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msginfo != NULL, -1);

	folder = dest->folder;

	g_return_val_if_fail(folder->copy_msg != NULL, -1);

	if (!dest->cache) folder_item_read_cache(dest);
	
	num = folder->copy_msg(folder, dest, msginfo);
	if (num != -1) {
		MsgInfo *newmsginfo;

		if (NULL != (newmsginfo = folder->fetch_msginfo(folder, dest, num))) {
			newmsginfo->flags.perm_flags = msginfo->flags.perm_flags;
			if (dest->stype == F_OUTBOX ||
			    dest->stype == F_QUEUE  ||
			    dest->stype == F_DRAFT  ||
			    dest->stype == F_TRASH)
				MSG_UNSET_PERM_FLAGS(newmsginfo->flags,
						     MSG_NEW|MSG_UNREAD|MSG_DELETED);
			msgcache_add_msg(dest->cache, newmsginfo);

			if (MSG_IS_NEW(newmsginfo->flags))
				dest->new++;
			if (MSG_IS_UNREAD(newmsginfo->flags))
				dest->unread++;
			dest->total++;
			dest->need_update = TRUE;

			procmsg_msginfo_free(newmsginfo);
		}			
	}

	if (folder->finished_copy)
		folder->finished_copy(folder, dest);

	return num;
}

/*
gint folder_item_copy_msgs_with_dest(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	gint num;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	folder = dest->folder;
	if (dest->last_num < 0) folder->scan(folder, dest);

	num = folder->copy_msgs_with_dest(folder, dest, msglist);
	if (num > 0) dest->last_num = num;
	else dest->op_count = 0;

	return num;
}
*/

gint folder_item_copy_msgs_with_dest(FolderItem *dest, GSList *msglist)
{
	Folder *folder;
	gint num;
	GSList *newmsgnums = NULL;
	GSList *l, *l2;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(msglist != NULL, -1);

	folder = dest->folder;
 
	g_return_val_if_fail(folder->copy_msg != NULL, -1);

	/* 
	 * Copy messages to destination folder and 
	 * store new message numbers in newmsgnums
	 */
	for (l = msglist ; l != NULL ; l = g_slist_next(l)) {
		MsgInfo * msginfo = (MsgInfo *) l->data;

		num = folder->copy_msg(folder, dest, msginfo);
		newmsgnums = g_slist_append(newmsgnums, GINT_TO_POINTER(num));
	}

	/* Read cache for dest folder */
	if (!dest->cache) folder_item_read_cache(dest);

	/* 
	 * Fetch new MsgInfos for new messages in dest folder,
	 * add them to the msgcache and update folder message counts
	 */
	l2 = newmsgnums;
	for (l = msglist; l != NULL; l = g_slist_next(l)) {
		MsgInfo *msginfo = (MsgInfo *) l->data;

		num = GPOINTER_TO_INT(l2->data);

		if (num != -1) {
			MsgInfo *newmsginfo;

			newmsginfo = folder->fetch_msginfo(folder, dest, num);
			if (newmsginfo) {
				newmsginfo->flags.perm_flags = msginfo->flags.perm_flags;
				if (dest->stype == F_OUTBOX ||
				    dest->stype == F_QUEUE  ||
				    dest->stype == F_DRAFT  ||
				    dest->stype == F_TRASH)
					MSG_UNSET_PERM_FLAGS(newmsginfo->flags,
							     MSG_NEW|MSG_UNREAD|MSG_DELETED);
				msgcache_add_msg(dest->cache, newmsginfo);

				if (MSG_IS_NEW(newmsginfo->flags))
					dest->new++;
				if (MSG_IS_UNREAD(newmsginfo->flags))
					dest->unread++;
				dest->total++;
				dest->need_update = TRUE;

				procmsg_msginfo_free(newmsginfo);
			}
		}
		l2 = g_slist_next(l2);
	}
	
	if (folder->finished_copy)
		folder->finished_copy(folder, dest);

	g_slist_free(newmsgnums);
	return dest->last_num;
}

gint folder_item_remove_msg(FolderItem *item, gint num)
{
	Folder *folder;
	gint ret;
	MsgInfo *msginfo;

	g_return_val_if_fail(item != NULL, -1);

	folder = item->folder;
	if (!item->cache) folder_item_read_cache(item);

	ret = folder->remove_msg(folder, item, num);

	msginfo = msgcache_get_msg(item->cache, num);
	if (msginfo != NULL) {
		if (MSG_IS_NEW(msginfo->flags))
			item->new--;
		if (MSG_IS_UNREAD(msginfo->flags))
			item->unread--;
		procmsg_msginfo_free(msginfo);
		msgcache_remove_msg(item->cache, num);
	}
	item->total--;
	item->need_update = TRUE;

	return ret;
}

gint folder_item_remove_msgs(FolderItem *item, GSList *msglist)
{
	Folder *folder;
	gint ret = 0;

	g_return_val_if_fail(item != NULL, -1);
	
	folder = item->folder;
	if (folder->remove_msgs) {
		ret = folder->remove_msgs(folder, item, msglist);
		if (ret == 0)
			folder->scan(folder);
		return ret;
	}

	if (!item->cache) folder_item_read_cache(item);

	while (msglist != NULL) {
		MsgInfo *msginfo = (MsgInfo *)msglist->data;

		ret = folder_item_remove_msg(item, msginfo->msgnum);
		if (ret != 0) break;
		msgcache_remove_msg(item->cache, msginfo->msgnum);
		msglist = msglist->next;
	}

	return ret;
}

gint folder_item_remove_all_msg(FolderItem *item)
{
	Folder *folder;
	gint result;

	g_return_val_if_fail(item != NULL, -1);

	folder = item->folder;

	g_return_val_if_fail(folder->remove_all_msg != NULL, -1);

	result = folder->remove_all_msg(folder, item);

	if (result == 0) {
		if (folder->finished_remove)
			folder->finished_remove(folder, item);

		folder_item_free_cache(item);
		item->cache = msgcache_new();

		item->new = 0;
		item->unread = 0;
		item->total = 0;
		item->need_update = TRUE;
	}

	return result;
}

gboolean folder_item_is_msg_changed(FolderItem *item, MsgInfo *msginfo)
{
	Folder *folder;

	g_return_val_if_fail(item != NULL, FALSE);

	folder = item->folder;

	g_return_val_if_fail(folder->is_msg_changed != NULL, -1);

	return folder->is_msg_changed(folder, item, msginfo);
}

gchar *folder_item_get_cache_file(FolderItem *item)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, CACHE_FILE, NULL);
	g_free(path);

	return file;
}

gchar *folder_item_get_mark_file(FolderItem *item)
{
	gchar *path;
	gchar *file;

	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(item->path != NULL, NULL);

	path = folder_item_get_path(item);
	g_return_val_if_fail(path != NULL, NULL);
	if (!is_dir_exist(path))
		make_dir_hier(path);
	file = g_strconcat(path, G_DIR_SEPARATOR_S, MARK_FILE, NULL);
	g_free(path);

	return file;
}

static gboolean folder_build_tree(GNode *node, gpointer data)
{
	Folder *folder = FOLDER(data);
	FolderItem *item;
	XMLNode *xmlnode;
	GList *list;
	SpecialFolderItemType stype = F_NORMAL;
	const gchar *name = NULL;
	const gchar *path = NULL;
	PrefsAccount *account = NULL;
	gboolean no_sub = FALSE, no_select = FALSE, collapsed = FALSE, 
		 threaded = TRUE, apply_sub = FALSE;
	gboolean ret_rcpt = FALSE, hidereadmsgs = FALSE; /* CLAWS */
	FolderSortKey sort_key = SORT_BY_NONE;
	FolderSortType sort_type = SORT_ASCENDING;
	gint new = 0, unread = 0, total = 0;
	time_t mtime = 0;

	g_return_val_if_fail(node->data != NULL, FALSE);
	if (!node->parent) return FALSE;

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folderitem") != 0) {
		g_warning("tag name != \"folderitem\"\n");
		return FALSE;
	}

	list = xmlnode->tag->attr;
	for (; list != NULL; list = list->next) {
		XMLAttr *attr = list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "type")) {
			if (!strcasecmp(attr->value, "normal"))
				stype = F_NORMAL;
			else if (!strcasecmp(attr->value, "inbox"))
				stype = F_INBOX;
			else if (!strcasecmp(attr->value, "outbox"))
				stype = F_OUTBOX;
			else if (!strcasecmp(attr->value, "draft"))
				stype = F_DRAFT;
			else if (!strcasecmp(attr->value, "queue"))
				stype = F_QUEUE;
			else if (!strcasecmp(attr->value, "trash"))
				stype = F_TRASH;
		} else if (!strcmp(attr->name, "name"))
			name = attr->value;
		else if (!strcmp(attr->name, "path"))
			path = attr->value;
		else if (!strcmp(attr->name, "mtime"))
			mtime = strtoul(attr->value, NULL, 10);
		else if (!strcmp(attr->name, "new"))
			new = atoi(attr->value);
		else if (!strcmp(attr->name, "unread"))
			unread = atoi(attr->value);
		else if (!strcmp(attr->name, "total"))
			total = atoi(attr->value);
		else if (!strcmp(attr->name, "no_sub"))
			no_sub = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "no_select"))
			no_select = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "collapsed"))
			collapsed = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "threaded"))
			threaded =  *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "hidereadmsgs"))
			hidereadmsgs =  *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "reqretrcpt"))
			ret_rcpt =  *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "sort_key")) {
			if (!strcmp(attr->value, "none"))
				sort_key = SORT_BY_NONE;
			else if (!strcmp(attr->value, "number"))
				sort_key = SORT_BY_NUMBER;
			else if (!strcmp(attr->value, "size"))
				sort_key = SORT_BY_SIZE;
			else if (!strcmp(attr->value, "date"))
				sort_key = SORT_BY_DATE;
			else if (!strcmp(attr->value, "from"))
				sort_key = SORT_BY_FROM;
			else if (!strcmp(attr->value, "subject"))
				sort_key = SORT_BY_SUBJECT;
			else if (!strcmp(attr->value, "score"))
				sort_key = SORT_BY_SCORE;
			else if (!strcmp(attr->value, "label"))
				sort_key = SORT_BY_LABEL;
			else if (!strcmp(attr->value, "mark"))
				sort_key = SORT_BY_MARK;
			else if (!strcmp(attr->value, "unread"))
				sort_key = SORT_BY_UNREAD;
			else if (!strcmp(attr->value, "mime"))
				sort_key = SORT_BY_MIME;
			else if (!strcmp(attr->value, "locked"))
				sort_key = SORT_BY_LOCKED;
		} else if (!strcmp(attr->name, "sort_type")) {
			if (!strcmp(attr->value, "ascending"))
				sort_type = SORT_ASCENDING;
			else
				sort_type = SORT_DESCENDING;
		} else if (!strcmp(attr->name, "account_id")) {
			account = account_find_from_id(atoi(attr->value));
			if (!account) g_warning("account_id: %s not found\n",
						attr->value);
		} else if (!strcmp(attr->name, "apply_sub"))
			apply_sub = *attr->value == '1' ? TRUE : FALSE;
	}

	item = folder_item_new(folder, name, path);
	item->stype = stype;
	item->mtime = mtime;
	item->new = new;
	item->unread = unread;
	item->total = total;
	item->no_sub = no_sub;
	item->no_select = no_select;
	item->collapsed = collapsed;
	item->threaded  = threaded;
	item->hide_read_msgs  = hidereadmsgs;
	item->ret_rcpt  = ret_rcpt;
	item->sort_key  = sort_key;
	item->sort_type = sort_type;
	item->parent = FOLDER_ITEM(node->parent->data);
	item->folder = folder;
	switch (stype) {
	case F_INBOX:  folder->inbox  = item; break;
	case F_OUTBOX: folder->outbox = item; break;
	case F_DRAFT:  folder->draft  = item; break;
	case F_QUEUE:  folder->queue  = item; break;
	case F_TRASH:  folder->trash  = item; break;
	default:       break;
	}
	item->account = account;
	item->apply_sub = apply_sub;
	prefs_folder_item_read_config(item);

	node->data = item;
	xml_free_node(xmlnode);

	return FALSE;
}

static gboolean folder_read_folder_func(GNode *node, gpointer data)
{
	Folder *folder;
	XMLNode *xmlnode;
	GList *list;
	FolderType type = F_UNKNOWN;
	const gchar *name = NULL;
	const gchar *path = NULL;
	PrefsAccount *account = NULL;
	gboolean collapsed = FALSE, threaded = TRUE, apply_sub = FALSE;
	gboolean ret_rcpt = FALSE; /* CLAWS */

	if (g_node_depth(node) != 2) return FALSE;
	g_return_val_if_fail(node->data != NULL, FALSE);

	xmlnode = node->data;
	if (strcmp2(xmlnode->tag->tag, "folder") != 0) {
		g_warning("tag name != \"folder\"\n");
		return TRUE;
	}
	g_node_unlink(node);
	list = xmlnode->tag->attr;
	for (; list != NULL; list = list->next) {
		XMLAttr *attr = list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "type")) {
			if (!strcasecmp(attr->value, "mh"))
				type = F_MH;
			else if (!strcasecmp(attr->value, "mbox"))
				type = F_MBOX;
			else if (!strcasecmp(attr->value, "maildir"))
				type = F_MAILDIR;
			else if (!strcasecmp(attr->value, "imap"))
				type = F_IMAP;
			else if (!strcasecmp(attr->value, "news"))
				type = F_NEWS;
		} else if (!strcmp(attr->name, "name"))
			name = attr->value;
		else if (!strcmp(attr->name, "path"))
			path = attr->value;
		else if (!strcmp(attr->name, "collapsed"))
			collapsed = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "threaded"))
			threaded = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "account_id")) {
			account = account_find_from_id(atoi(attr->value));
			if (!account) g_warning("account_id: %s not found\n",
						attr->value);
		} else if (!strcmp(attr->name, "apply_sub"))
			apply_sub = *attr->value == '1' ? TRUE : FALSE;
		else if (!strcmp(attr->name, "reqretrcpt"))
			ret_rcpt = *attr->value == '1' ? TRUE : FALSE;
	}

	folder = folder_new(type, name, path);
	g_return_val_if_fail(folder != NULL, FALSE);
	folder->account = account;
	if (account && (type == F_IMAP || type == F_NEWS))
		account->folder = REMOTE_FOLDER(folder);
	node->data = folder->node->data;
	g_node_destroy(folder->node);
	folder->node = node;
	folder_add(folder);
	FOLDER_ITEM(node->data)->collapsed = collapsed;
	FOLDER_ITEM(node->data)->threaded  = threaded;
	FOLDER_ITEM(node->data)->account   = account;
	FOLDER_ITEM(node->data)->apply_sub = apply_sub;
	FOLDER_ITEM(node->data)->ret_rcpt  = ret_rcpt;

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_build_tree, folder);

	return FALSE;
}

static gchar *folder_get_list_path(void)
{
	static gchar *filename = NULL;

	if (!filename)
		filename =  g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					FOLDER_LIST, NULL);

	return filename;
}

#ifdef WIN32
#define PUT_ESCAPE_STR(fp, attr, str)			\
{							\
	gchar *p_str;					\
	p_str = g_strdup(str);				\
	locale_from_utf8(&p_str);			\
	xml_file_put_escape_str(fp, p_str);		\
	g_free(p_str);					\
}
#else
#define PUT_ESCAPE_STR(fp, attr, str)			\
{							\
	fputs(" " attr "=\"", fp);			\
	xml_file_put_escape_str(fp, str);		\
	fputs("\"", fp);				\
}                                                       
#endif

static void folder_write_list_recursive(GNode *node, gpointer data)
{
	FILE *fp = (FILE *)data;
	FolderItem *item;
	gint i, depth;
	static gchar *folder_type_str[] = {"mh", "mbox", "maildir", "imap",
					   "news", "unknown"};
	static gchar *folder_item_stype_str[] = {"normal", "inbox", "outbox",
						 "draft", "queue", "trash"};
	static gchar *sort_key_str[] = {"none", "number", "size", "date",
					"from", "subject", "score", "label",
					"mark", "unread", "mime", "locked" };
	g_return_if_fail(node != NULL);
	g_return_if_fail(fp != NULL);

	item = FOLDER_ITEM(node->data);
	g_return_if_fail(item != NULL);

	depth = g_node_depth(node);
	for (i = 0; i < depth; i++)
		fputs("    ", fp);
	if (depth == 1) {
		Folder *folder = item->folder;

		fprintf(fp, "<folder type=\"%s\"", folder_type_str[folder->type]);
		if (folder->name)
			PUT_ESCAPE_STR(fp, "name", folder->name);
		if (folder->type == F_MH || folder->type == F_MBOX)
			PUT_ESCAPE_STR(fp, "path",
				       LOCAL_FOLDER(folder)->rootpath);
		if (item->collapsed && node->children)
			fputs(" collapsed=\"1\"", fp);
		if (folder->account)
			fprintf(fp, " account_id=\"%d\"",
				folder->account->account_id);
		if (item->apply_sub)
			fputs(" apply_sub=\"1\"", fp);
		if (item->ret_rcpt) 
			fputs(" reqretrcpt=\"1\"", fp);
	} else {
		fprintf(fp, "<folderitem type=\"%s\"",
			folder_item_stype_str[item->stype]);
		if (item->name)
			PUT_ESCAPE_STR(fp, "name", item->name);
		if (item->path)
			PUT_ESCAPE_STR(fp, "path", item->path);

		if (item->no_sub)
			fputs(" no_sub=\"1\"", fp);
		if (item->no_select)
			fputs(" no_select=\"1\"", fp);
		if (item->collapsed && node->children)
			fputs(" collapsed=\"1\"", fp);
		if (item->threaded)
			fputs(" threaded=\"1\"", fp);
		else
			fputs(" threaded=\"0\"", fp);
		if (item->hide_read_msgs)
			fputs(" hidereadmsgs=\"1\"", fp);
		else
			fputs(" hidereadmsgs=\"0\"", fp);
		if (item->ret_rcpt)
			fputs(" reqretrcpt=\"1\"", fp);

		if (item->sort_key != SORT_BY_NONE) {
			fprintf(fp, " sort_key=\"%s\"",
				sort_key_str[item->sort_key]);
			if (item->sort_type == SORT_ASCENDING)
				fprintf(fp, " sort_type=\"ascending\"");
			else
				fprintf(fp, " sort_type=\"descending\"");
		}

		fprintf(fp,
			" mtime=\"%lu\" new=\"%d\" unread=\"%d\" total=\"%d\"",
			item->mtime, item->new, item->unread, item->total);

		if (item->account)
			fprintf(fp, " account_id=\"%d\"",
				item->account->account_id);
		if (item->apply_sub)
			fputs(" apply_sub=\"1\"", fp);
	}

	if (node->children) {
		GNode *child;
		fputs(">\n", fp);

		child = node->children;
		while (child) {
			GNode *cur;

			cur = child;
			child = cur->next;
			folder_write_list_recursive(cur, data);
		}

		for (i = 0; i < depth; i++)
			fputs("    ", fp);
		fprintf(fp, "</%s>\n", depth == 1 ? "folder" : "folderitem");
	} else
		fputs(" />\n", fp);
}

static void folder_update_op_count_rec(GNode *node)
{
	FolderItem *fitem = FOLDER_ITEM(node->data);

	if (g_node_depth(node) > 0) {
		if (fitem->op_count > 0) {
			fitem->op_count = 0;
			folder_update_item(fitem, FALSE);
		}
		if (node->children) {
			GNode *child;

			child = node->children;
			while (child) {
				GNode *cur;

				cur = child;
				child = cur->next;
				folder_update_op_count_rec(cur);
			}
		}
	}
}

void folder_update_op_count() {
	GList *cur;
	Folder *folder;

	for (cur = folder_list; cur != NULL; cur = cur->next) {
		folder = cur->data;
		folder_update_op_count_rec(folder->node);
	}
}

typedef struct _type_str {
	gchar * str;
	gint type;
} type_str;


/*
static gchar * folder_item_get_tree_identifier(FolderItem * item)
{
	if (item->parent != NULL) {
		gchar * path;
		gchar * id;

		path = folder_item_get_tree_identifier(item->parent);
		if (path == NULL)
			return NULL;

		id = g_strconcat(path, "/", item->name, NULL);
		g_free(path);

		return id;
	}
	else {
		return g_strconcat("/", item->name, NULL);
	}
}
*/

/* CLAWS: temporary local folder for filtering */
static Folder *processing_folder;
static FolderItem *processing_folder_item;

static void folder_create_processing_folder(void)
{
#define PROCESSING_FOLDER ".processing"	
	Folder	   *tmpparent;
	gchar      *tmpname;

	tmpparent = folder_get_default_folder();
	g_assert(tmpparent);
	debug_print("tmpparentroot %s\n", LOCAL_FOLDER(tmpparent)->rootpath);
	if (LOCAL_FOLDER(tmpparent)->rootpath[0] == '/')
		tmpname = g_strconcat(LOCAL_FOLDER(tmpparent)->rootpath,
				      G_DIR_SEPARATOR_S, PROCESSING_FOLDER,
				      NULL);
	else
		tmpname = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				      LOCAL_FOLDER(tmpparent)->rootpath,
				      G_DIR_SEPARATOR_S, PROCESSING_FOLDER,
				      NULL);

	processing_folder = folder_new(F_MH, "PROCESSING", LOCAL_FOLDER(tmpparent)->rootpath);
	g_assert(processing_folder);

	if (!is_dir_exist(tmpname)) {
		debug_print("*TMP* creating %s\n", tmpname);
		processing_folder_item = processing_folder->create_folder(processing_folder,
									  processing_folder->node->data,
									  PROCESSING_FOLDER);
		g_assert(processing_folder_item);									  
	}
	else {
		debug_print("*TMP* already created\n");
		processing_folder_item = folder_item_new(processing_folder, ".processing", ".processing");
		g_assert(processing_folder_item);
		folder_item_append(processing_folder->node->data, processing_folder_item);
	}
	g_free(tmpname);
}

FolderItem *folder_get_default_processing(void)
{
	if (!processing_folder_item) {
		folder_create_processing_folder();
	}
	return processing_folder_item;
}

/* folder_persist_prefs_new() - return hash table with persistent
 * settings (and folder name as key). 
 * (note that in claws other options are in the PREFS_FOLDER_ITEM_RC
 * file, so those don't need to be included in PersistPref yet) 
 */
GHashTable *folder_persist_prefs_new(Folder *folder)
{
	GHashTable *pptable;

	g_return_val_if_fail(folder, NULL);
	pptable = g_hash_table_new(g_str_hash, g_str_equal);
	folder_get_persist_prefs_recursive(folder->node, pptable);
	return pptable;
}

void folder_persist_prefs_free(GHashTable *pptable)
{
	g_return_if_fail(pptable);
	g_hash_table_foreach_remove(pptable, persist_prefs_free, NULL);
	g_hash_table_destroy(pptable);
}

const PersistPrefs *folder_get_persist_prefs(GHashTable *pptable, const char *name)
{
	if (pptable == NULL || name == NULL) return NULL;
	return g_hash_table_lookup(pptable, name);
}

void folder_item_restore_persist_prefs(FolderItem *item, GHashTable *pptable)
{
	const PersistPrefs *pp;
	gchar *id = folder_item_get_identifier(item);

	pp = folder_get_persist_prefs(pptable, id); 
	g_free(id);

	if (!pp) return;

	/* CLAWS: since not all folder properties have been migrated to 
	 * folderlist.xml, we need to call the old stuff first before
	 * setting things that apply both to Main and Claws. */
	prefs_folder_item_read_config(item); 
	 
	item->collapsed = pp->collapsed;
	item->threaded  = pp->threaded;
	item->ret_rcpt  = pp->ret_rcpt;
	item->hide_read_msgs = pp->hide_read_msgs;
	item->sort_key  = pp->sort_key;
	item->sort_type = pp->sort_type;
}

static void folder_get_persist_prefs_recursive(GNode *node, GHashTable *pptable)
{
	FolderItem *item = FOLDER_ITEM(node->data);
	PersistPrefs *pp;
	GNode *child, *cur;
	gchar *id;

	g_return_if_fail(node != NULL);
	g_return_if_fail(item != NULL);

	/* NOTE: item->path == NULL means top level folder; not interesting
	 * to store preferences of that one.  */
	if (item->path) {
		id = folder_item_get_identifier(item);
		pp = g_new0(PersistPrefs, 1);
		g_return_if_fail(pp != NULL);
		pp->collapsed = item->collapsed;
		pp->threaded  = item->threaded;
		pp->ret_rcpt  = item->ret_rcpt;	
		pp->hide_read_msgs = item->hide_read_msgs;
		pp->sort_key  = item->sort_key;
		pp->sort_type = item->sort_type;
		g_hash_table_insert(pptable, id, pp);
	}

	if (node->children) {
		child = node->children;
		while (child) {
			cur = child;
			child = cur->next;
			folder_get_persist_prefs_recursive(cur, pptable);
		}
	}	
}

static gboolean persist_prefs_free(gpointer key, gpointer val, gpointer data)
{
	if (key) 
		g_free(key);
	if (val) 
		g_free(val);
	return TRUE;	
}

void folder_item_apply_processing(FolderItem *item)
{
	GSList *processing_list;
	GSList *mlist, *cur;
	
	g_return_if_fail(item != NULL);
	
	processing_list = item->prefs->processing;
	if (processing_list == NULL)
		return;

	mlist = folder_item_get_msg_list(item);
	
	for (cur = mlist ; cur != NULL ; cur = cur->next) {
		MsgInfo * msginfo;

		msginfo = (MsgInfo *) cur->data;
		filter_message_by_msginfo(processing_list, msginfo);
		procmsg_msginfo_free(msginfo);
	}
	
	folder_update_items_when_required(FALSE);

	g_slist_free(mlist);
}

GSList *folder_item_update_callbacks_list = NULL;
gint	folder_item_update_callbacks_nextid = 0;

struct FolderItemUpdateCallback
{
	gint			id;
	FolderItemUpdateFunc	func;
	gpointer		data;
};

gint folder_item_update_callback_register(FolderItemUpdateFunc func, gpointer data)
{
	struct FolderItemUpdateCallback *callback;

	g_return_val_if_fail(func != NULL, -1);

	folder_item_update_callbacks_nextid++;

	callback = g_new0(struct FolderItemUpdateCallback, 1);
	callback->id = folder_item_update_callbacks_nextid;
	callback->func = func;
	callback->data = data;

	folder_item_update_callbacks_list =
		g_slist_append(folder_item_update_callbacks_list, callback);

	return folder_item_update_callbacks_nextid;
}

void folder_item_update_callback_unregister(gint id)
{
	GSList *list, *next;

	for (list = folder_item_update_callbacks_list; list != NULL; list = next) {
    		struct FolderItemUpdateCallback *callback;

		next = list->next;

		callback = list->data;
		if (callback->id == id) {
			folder_item_update_callbacks_list =
				g_slist_remove(folder_item_update_callbacks_list, callback);
			g_free(callback);
		}
	}
}

static void folder_item_update_callback_execute(FolderItem *item, gboolean contentchange)
{
	GSList *list;

	for (list = folder_item_update_callbacks_list; list != NULL; list = list->next) {
    		struct FolderItemUpdateCallback *callback;

		callback = list->data;
		callback->func(item, contentchange, callback->data);
	}
}

void folder_update_item(FolderItem *item, gboolean contentchange)
{
	folder_item_update_callback_execute(item, contentchange);
}

static void folder_update_item_func(FolderItem *item, gpointer data)
{
	gboolean contentchange = GPOINTER_TO_INT(data);
	
	if (item->need_update) {
		folder_item_update_callback_execute(item, contentchange);
		item->need_update = FALSE;
	}
}

void folder_update_items_when_required(gboolean contentchange)
{
	folder_func_to_all_folders(folder_update_item_func, GINT_TO_POINTER(contentchange));
}

void folder_update_item_recursive(FolderItem *item, gboolean update_summary)
{
	GNode *node = item->folder->node;	
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	node = node->children;
	folder_item_update_callback_execute(item, update_summary);
	while (node != NULL) {
		if (node && node->data) {
			FolderItem *next_item = (FolderItem*) node->data;
			folder_item_update_callback_execute(next_item, update_summary);
		}
		node = node->next;
	}
}

#undef PUT_ESCAPE_STR
