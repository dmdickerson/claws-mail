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

#include "intl.h"
#include "msgcache.h"
#include "utils.h"
#include "procmsg.h"

MsgCache *msgcache_new()
{
	return g_hash_table_new(NULL, NULL);
}

static gboolean msgcache_msginfo_free_func(gpointer num, gpointer msginfo, gpointer user_data)
{
	procmsg_msginfo_free((MsgInfo *)msginfo);
	return TRUE;
}											  

void msgcache_destroy(MsgCache *cache)
{
	g_hash_table_foreach_remove(cache, msgcache_msginfo_free_func, NULL);
	g_hash_table_destroy(cache);
}

void msgcache_add_msg(MsgCache *cache, MsgInfo *msginfo) 
{
	MsgInfo *newmsginfo;
	
	newmsginfo = procmsg_msginfo_new_ref(msginfo);
	g_hash_table_insert(cache, GINT_TO_POINTER(msginfo->msgnum), newmsginfo);

	debug_print(_("Cache size: %d\n"), g_hash_table_size(cache));
}

void msgcache_remove_msg(MsgCache *cache, guint num)
{
	MsgInfo *msginfo;

	msginfo = (MsgInfo *)g_hash_table_lookup(cache, GINT_TO_POINTER(num));
	if(!msginfo)
		return;

	procmsg_msginfo_free(msginfo);
	g_hash_table_remove(cache, GINT_TO_POINTER(num));

	debug_print(_("Cache size: %d\n"), g_hash_table_size(cache));
}

void msgcache_update_msg(MsgCache *cache, MsgInfo *msginfo)
{
	MsgInfo *oldmsginfo, *newmsginfo;
	
	g_return_val_if_fail(cache != NULL, NULL);

	oldmsginfo = g_hash_table_lookup(cache, GINT_TO_POINTER(msginfo->msgnum));
	if(msginfo) {
		g_hash_table_remove(cache, GINT_TO_POINTER(oldmsginfo->msgnum));
		procmsg_msginfo_free(oldmsginfo);
	}

	newmsginfo = procmsg_msginfo_new_ref(msginfo);
	g_hash_table_insert(cache, GINT_TO_POINTER(msginfo->msgnum), newmsginfo);
	
	return;
}

static gint msgcache_read_cache_data_str(FILE *fp, gchar **str)
{
	gchar buf[BUFFSIZE];
	gint ret = 0;
	size_t len;

	if (fread(&len, sizeof(len), 1, fp) == 1) {
		if (len < 0)
			ret = -1;
		else {
			gchar *tmp = NULL;

			while (len > 0) {
				size_t size = MIN(len, BUFFSIZE - 1);

				if (fread(buf, size, 1, fp) != 1) {
					ret = -1;
					if (tmp) g_free(tmp);
					*str = NULL;
					break;
				}

				buf[size] = '\0';
				if (tmp) {
					*str = g_strconcat(tmp, buf, NULL);
					g_free(tmp);
					tmp = *str;
				} else
					tmp = *str = g_strdup(buf);

				len -= size;
			}
		}
	} else
		ret = -1;

	if (ret < 0)
		g_warning(_("Cache data is corrupted\n"));

	return ret;
}


#define READ_CACHE_DATA(data, fp) \
{ \
	if (msgcache_read_cache_data_str(fp, &data) < 0) { \
		procmsg_msginfo_free(msginfo); \
		break; \
	} \
}

#define READ_CACHE_DATA_INT(n, fp) \
{ \
	if (fread(&n, sizeof(n), 1, fp) != 1) { \
		g_warning(_("Cache data is corrupted\n")); \
		procmsg_msginfo_free(msginfo); \
		break; \
	} \
}

MsgCache *msgcache_read(const gchar *cache_file, const gchar *mark_file, FolderItem *item)
{
	MsgCache *cache;
	FILE *fp;
	MsgInfo *msginfo;
	MsgFlags default_flags;
	MsgPermFlags perm_flags;
	gchar file_buf[BUFFSIZE];
	gint ver;
	guint num;

	g_return_val_if_fail(cache_file != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	cache = msgcache_new();

	if ((fp = fopen(cache_file, "r")) == NULL) {
		debug_print(_("\tNo cache file\n"));
		return cache;
	}
	setvbuf(fp, file_buf, _IOFBF, sizeof(file_buf));

	debug_print(_("\tReading message cache from %s and %s...\n"), cache_file, mark_file);

	/* compare cache version */
	if (fread(&ver, sizeof(ver), 1, fp) != 1 ||
	    CACHE_VERSION != ver) {
		debug_print(_("Cache version is different. Discarding it.\n"));
		fclose(fp);
		return cache;
	}

	g_hash_table_freeze(cache);

	while (fread(&num, sizeof(num), 1, fp) == 1) {
		msginfo = procmsg_msginfo_new();
		msginfo->msgnum = num;
		READ_CACHE_DATA_INT(msginfo->size, fp);
		READ_CACHE_DATA_INT(msginfo->mtime, fp);
		READ_CACHE_DATA_INT(msginfo->date_t, fp);
		READ_CACHE_DATA_INT(msginfo->flags.tmp_flags, fp);

		READ_CACHE_DATA(msginfo->fromname, fp);

		READ_CACHE_DATA(msginfo->date, fp);
		READ_CACHE_DATA(msginfo->from, fp);
		READ_CACHE_DATA(msginfo->to, fp);
		READ_CACHE_DATA(msginfo->cc, fp);
		READ_CACHE_DATA(msginfo->newsgroups, fp);
		READ_CACHE_DATA(msginfo->subject, fp);
		READ_CACHE_DATA(msginfo->msgid, fp);
		READ_CACHE_DATA(msginfo->inreplyto, fp);
		READ_CACHE_DATA(msginfo->references, fp);

/*
		MSG_SET_PERM_FLAGS(msginfo->flags, default_flags.perm_flags);
		MSG_SET_TMP_FLAGS(msginfo->flags, default_flags.tmp_flags);
*/
		msginfo->folder = item;

		g_hash_table_insert(cache, GINT_TO_POINTER(msginfo->msgnum), msginfo);
	}
	fclose(fp);

	if ((fp = fopen(mark_file, "r")) == NULL)
		debug_print(_("Mark file not found.\n"));
	else if (fread(&ver, sizeof(ver), 1, fp) != 1 || MARK_VERSION != ver) {
		debug_print(_("Mark version is different (%d != %d). "
			      "Discarding it.\n"), ver, MARK_VERSION);
		fclose(fp);
		fp = NULL;
	} else {
		while (fread(&num, sizeof(num), 1, fp) == 1) {
			if (fread(&perm_flags, sizeof(perm_flags), 1, fp) != 1) break;

			msginfo = g_hash_table_lookup(cache, GUINT_TO_POINTER(num));
			if(msginfo) {
				msginfo->flags.perm_flags = perm_flags;
			}
		}
	}
	fclose(fp);

	g_hash_table_thaw(cache);

	debug_print(_("done. (%d items read)\n"), g_hash_table_size(cache));

	return cache;
}

#define WRITE_CACHE_DATA_INT(n, fp) \
	fwrite(&n, sizeof(n), 1, fp)

#define WRITE_CACHE_DATA(data, fp) \
{ \
	gint len; \
 \
	if (data == NULL || (len = strlen(data)) == 0) { \
		len = 0; \
		WRITE_CACHE_DATA_INT(len, fp); \
	} else { \
		len = strlen(data); \
		WRITE_CACHE_DATA_INT(len, fp); \
		fwrite(data, len, 1, fp); \
	} \
}

void msgcache_write_cache(MsgInfo *msginfo, FILE *fp)
{
	MsgTmpFlags flags = msginfo->flags.tmp_flags & MSG_CACHED_FLAG_MASK;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(msginfo->size, fp);
	WRITE_CACHE_DATA_INT(msginfo->mtime, fp);
	WRITE_CACHE_DATA_INT(msginfo->date_t, fp);
	WRITE_CACHE_DATA_INT(flags, fp);

	WRITE_CACHE_DATA(msginfo->fromname, fp);

	WRITE_CACHE_DATA(msginfo->date, fp);
	WRITE_CACHE_DATA(msginfo->from, fp);
	WRITE_CACHE_DATA(msginfo->to, fp);
	WRITE_CACHE_DATA(msginfo->cc, fp);
	WRITE_CACHE_DATA(msginfo->newsgroups, fp);
	WRITE_CACHE_DATA(msginfo->subject, fp);
	WRITE_CACHE_DATA(msginfo->msgid, fp);
	WRITE_CACHE_DATA(msginfo->inreplyto, fp);
	WRITE_CACHE_DATA(msginfo->references, fp);
}

static void msgcache_write_flags(MsgInfo *msginfo, FILE *fp)
{
	MsgPermFlags flags = msginfo->flags.perm_flags;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(flags, fp);
}

struct write_fps
{
	FILE *cache_fp;
	FILE *mark_fp;
};

static void msgcache_write_func(gpointer key, gpointer value, gpointer user_data)
{
	MsgInfo *msginfo;
	struct write_fps *write_fps;
	
	msginfo = (MsgInfo *)value;
	write_fps = user_data;

	msgcache_write_cache(msginfo, write_fps->cache_fp);
	msgcache_write_flags(msginfo, write_fps->mark_fp);
}

gint msgcache_write(const gchar *cache_file, const gchar *mark_file, MsgCache *cache)
{
	FILE *fp;
	struct write_fps write_fps;
	gint ver;

	g_return_val_if_fail(cache_file != NULL, -1);

	debug_print(_("\tWriting message cache to %s and %s...\n"), cache_file, mark_file);

	if ((fp = fopen(cache_file, "w")) == NULL) {
		FILE_OP_ERROR(cache_file, "fopen");
		return -1;
	}
	if (change_file_mode_rw(fp, cache_file) < 0)
		FILE_OP_ERROR(cache_file, "chmod");

	ver = CACHE_VERSION;
	WRITE_CACHE_DATA_INT(ver, fp);	
	write_fps.cache_fp = fp;

	if ((fp = fopen(mark_file, "w")) == NULL) {
		FILE_OP_ERROR(mark_file, "fopen");
		fclose(write_fps.cache_fp);
		return -1;
	}

	ver = MARK_VERSION;
	WRITE_CACHE_DATA_INT(ver, fp);
	write_fps.mark_fp = fp;

	g_hash_table_foreach((GHashTable *)cache, msgcache_write_func, (gpointer)&write_fps);

	fclose(write_fps.cache_fp);
	fclose(write_fps.mark_fp);

	debug_print(_("done.\n"));
}

MsgInfo *msgcache_get_msg(MsgCache *cache, guint num)
{
	MsgInfo *msginfo;

	g_return_val_if_fail(cache != NULL, NULL);

	msginfo = g_hash_table_lookup(cache, GINT_TO_POINTER(num));
	if(!msginfo)
		return NULL;
	
	return procmsg_msginfo_new_ref(msginfo);
}

static void msgcache_get_msg_list_func(gpointer key, gpointer value, gpointer user_data)
{
	GSList **listptr = user_data;
	MsgInfo *msginfo = value;

	*listptr = g_slist_prepend(*listptr, procmsg_msginfo_new_ref(msginfo));
}

GSList *msgcache_get_msg_list(MsgCache *cache)
{
	GSList *msg_list = NULL;

	g_hash_table_foreach((GHashTable *)cache, msgcache_get_msg_list_func, (gpointer)&msg_list);	

	return msg_list;
}
