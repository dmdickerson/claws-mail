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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <ctype.h>
#include <time.h>

#include "intl.h"
#include "mbox.h"
#include "procmsg.h"
#include "folder.h"
#include "filter.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "utils.h"
#include "filtering.h"

#define MSGBUFSIZE	8192

#define FPUTS_TO_TMP_ABORT_IF_FAIL(s) \
{ \
	if (fputs(s, tmp_fp) == EOF) { \
		g_warning(_("can't write to temporary file\n")); \
		fclose(tmp_fp); \
		fclose(mbox_fp); \
		unlink(tmp_file); \
		return -1; \
	} \
}

gint proc_mbox(FolderItem *dest, const gchar *mbox, GHashTable *folder_table)
{
	FILE *mbox_fp;
	gchar buf[MSGBUFSIZE], from_line[MSGBUFSIZE];
	gchar *tmp_file;
	gint msgs = 0;
	FolderItem *inbox;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	debug_print(_("Getting messages from %s into %s...\n"), mbox, dest->path);

	if ((mbox_fp = fopen(mbox, "r")) == NULL) {
		FILE_OP_ERROR(mbox, "fopen");
		return -1;
	}

	/* ignore empty lines on the head */
	do {
		if (fgets(buf, sizeof(buf), mbox_fp) == NULL) {
			g_warning(_("can't read mbox file.\n"));
			fclose(mbox_fp);
			return -1;
		}
	} while (buf[0] == '\n' || buf[0] == '\r');

	if (strncmp(buf, "From ", 5) != 0) {
		g_warning(_("invalid mbox format: %s\n"), mbox);
		fclose(mbox_fp);
		return -1;
	}

	strcpy(from_line, buf);
	if (fgets(buf, sizeof(buf), mbox_fp) == NULL) {
		g_warning(_("malformed mbox: %s\n"), mbox);
		fclose(mbox_fp);
		return -1;
	}

	tmp_file = get_tmp_file();
	inbox    = folder_get_default_inbox();

	do {
		FILE *tmp_fp;
		FolderItem *dropfolder;
		gchar *startp, *endp, *rpath;
		gint empty_line;
		gint val;
		gboolean is_next_msg = FALSE;
		gint msgnum;

		if ((tmp_fp = fopen(tmp_file, "w")) == NULL) {
			FILE_OP_ERROR(tmp_file, "fopen");
			g_warning(_("can't open temporary file\n"));
			fclose(mbox_fp);
			return -1;
		}
		if (change_file_mode_rw(tmp_fp, tmp_file) < 0)
			FILE_OP_ERROR(tmp_file, "chmod");

		/* convert unix From into Return-Path */
		/*
		startp = from_line + 5;
		endp = strchr(startp, ' ');
		if (endp == NULL)
			rpath = g_strdup(startp);
		else
			rpath = g_strndup(startp, endp - startp);
		g_strstrip(rpath);
		g_snprintf(from_line, sizeof(from_line),
			   "Return-Path: %s\n", rpath);
		g_free(rpath);
		*/

		FPUTS_TO_TMP_ABORT_IF_FAIL(from_line);
		FPUTS_TO_TMP_ABORT_IF_FAIL(buf);
		from_line[0] = '\0';

		empty_line = 0;

		while (fgets(buf, sizeof(buf), mbox_fp) != NULL) {
			if (buf[0] == '\n' || buf[0] == '\r') {
				empty_line++;
				buf[0] = '\0';
				continue;
			}

			/* From separator */
			while (!strncmp(buf, "From ", 5)) {
				strcpy(from_line, buf);
				if (fgets(buf, sizeof(buf), mbox_fp) == NULL) {
					buf[0] = '\0';
					break;
				}

				if (is_header_line(buf)) {
					is_next_msg = TRUE;
					break;
				} else if (!strncmp(buf, "From ", 5)) {
					continue;
				} else if (!strncmp(buf, ">From ", 6)) {
					g_memmove(buf, buf + 1, strlen(buf));
					is_next_msg = TRUE;
					break;
				} else {
					g_warning(_("unescaped From found:\n%s"),
						  from_line);
					break;
				}
			}
			if (is_next_msg) break;

			if (empty_line > 0) {
				while (empty_line--)
					FPUTS_TO_TMP_ABORT_IF_FAIL("\n");
				empty_line = 0;
			}

			if (from_line[0] != '\0') {
				FPUTS_TO_TMP_ABORT_IF_FAIL(from_line);
				from_line[0] = '\0';
			}

			if (buf[0] != '\0') {
				if (!strncmp(buf, ">From ", 6)) {
					FPUTS_TO_TMP_ABORT_IF_FAIL(buf + 1);
				} else
					FPUTS_TO_TMP_ABORT_IF_FAIL(buf);

				buf[0] = '\0';
			}
		}

		if (empty_line > 0) {
			while (--empty_line)
				FPUTS_TO_TMP_ABORT_IF_FAIL("\n");
		}

		if (fclose(tmp_fp) == EOF) {
			FILE_OP_ERROR(tmp_file, "fclose");
			g_warning(_("can't write to temporary file\n"));
			fclose(mbox_fp);
			unlink(tmp_file);
			return -1;
		}

		if (folder_table) {
			if (global_processing == NULL) {
				/* old filtering */
				dropfolder = filter_get_dest_folder
					(prefs_common.fltlist, tmp_file);
				if (!dropfolder ||
				    !strcmp(dropfolder->path, FILTER_NOT_RECEIVE))
					dropfolder = dest;
				val = GPOINTER_TO_INT(g_hash_table_lookup
						      (folder_table, dropfolder));
				if (val == 0) {
					g_hash_table_insert(folder_table, dropfolder,
							    GINT_TO_POINTER(1));
				}
			}
			else {
				/* CLAWS: new filtering */
				dropfolder = folder_get_default_processing();
			}
		} else
			dropfolder = dest;

			
		if ((msgnum = folder_item_add_msg(dropfolder, tmp_file, TRUE)) < 0) {
			fclose(mbox_fp);
			unlink(tmp_file);
			return -1;
		}

		if (global_processing) {
			/* CLAWS: new filtering */
			if (folder_table) {
				filter_message(global_processing, inbox,
					       msgnum, folder_table);
			}
		}

		msgs++;
	} while (from_line[0] != '\0');

	fclose(mbox_fp);
	debug_print(_("%d messages found.\n"), msgs);

	return msgs;
}

gint lock_mbox(const gchar *base, LockType type)
{
	gint retval = 0;

	if (type == LOCK_FILE) {
		gchar *lockfile, *locklink;
		gint retry = 0;
		FILE *lockfp;

		lockfile = g_strdup_printf("%s.%d", base, getpid());
		if ((lockfp = fopen(lockfile, "w")) == NULL) {
			FILE_OP_ERROR(lockfile, "fopen");
			g_warning(_("can't create lock file %s\n"), lockfile);
			g_warning(_("use 'flock' instead of 'file' if possible.\n"));
			g_free(lockfile);
			return -1;
		}

		fprintf(lockfp, "%d\n", getpid());
		fclose(lockfp);

		locklink = g_strconcat(base, ".lock", NULL);
		while (link(lockfile, locklink) < 0) {
			FILE_OP_ERROR(lockfile, "link");
			if (retry >= 5) {
				g_warning(_("can't create %s\n"), lockfile);
				unlink(lockfile);
				g_free(lockfile);
				return -1;
			}
			if (retry == 0)
				g_warning(_("mailbox is owned by another"
					    " process, waiting...\n"));
			retry++;
			sleep(5);
		}
		unlink(lockfile);
		g_free(lockfile);
	} else if (type == LOCK_FLOCK) {
		gint lockfd;

#if HAVE_FLOCK
		if ((lockfd = open(base, O_RDONLY)) < 0) {
#else
		if ((lockfd = open(base, O_RDWR)) < 0) {
#endif
			FILE_OP_ERROR(base, "open");
			return -1;
		}
#if HAVE_FLOCK
		if (flock(lockfd, LOCK_EX|LOCK_NB) < 0) {
			perror("flock");
#else
#if HAVE_LOCKF
		if (lockf(lockfd, F_TLOCK, 0) < 0) {
			perror("lockf");
#else
		{
#endif
#endif /* HAVE_FLOCK */
			g_warning(_("can't lock %s\n"), base);
			if (close(lockfd) < 0)
				perror("close");
			return -1;
		}
		retval = lockfd;
	} else {
		g_warning(_("invalid lock type\n"));
		return -1;
	}

	return retval;
}

gint unlock_mbox(const gchar *base, gint fd, LockType type)
{
	if (type == LOCK_FILE) {
		gchar *lockfile;

		lockfile = g_strconcat(base, ".lock", NULL);
		if (unlink(lockfile) < 0) {
			FILE_OP_ERROR(lockfile, "unlink");
			g_free(lockfile);
			return -1;
		}
		g_free(lockfile);

		return 0;
	} else if (type == LOCK_FLOCK) {
#if HAVE_FLOCK
		if (flock(fd, LOCK_UN) < 0) {
			perror("flock");
#else
#if HAVE_LOCKF
		if (lockf(fd, F_ULOCK, 0) < 0) {
			perror("lockf");
#else
		{
#endif
#endif /* HAVE_FLOCK */
			g_warning(_("can't unlock %s\n"), base);
			if (close(fd) < 0)
				perror("close");
			return -1;
		}

		if (close(fd) < 0) {
			perror("close");
			return -1;
		}

		return 0;
	}

	g_warning(_("invalid lock type\n"));
	return -1;
}

gint copy_mbox(const gchar *src, const gchar *dest)
{
	return copy_file(src, dest);
}

void empty_mbox(const gchar *mbox)
{
	if (truncate(mbox, 0) < 0) {
		FILE *fp;

		FILE_OP_ERROR(mbox, "truncate");
		if ((fp = fopen(mbox, "w")) == NULL) {
			FILE_OP_ERROR(mbox, "fopen");
			g_warning(_("can't truncate mailbox to zero.\n"));
			return;
		}
		fclose(fp);
	}
}

/* read all messages in SRC, and store them into one MBOX file. */
gint export_to_mbox(FolderItem *src, const gchar *mbox)
{
	GSList *mlist;
	GSList *cur;
	MsgInfo *msginfo;
	FILE *msg_fp;
	FILE *mbox_fp;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(src != NULL, -1);
	g_return_val_if_fail(src->folder != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	debug_print(_("Exporting messages from %s into %s...\n"),
		    src->path, mbox);

	if ((mbox_fp = fopen(mbox, "w")) == NULL) {
		FILE_OP_ERROR(mbox, "fopen");
		return -1;
	}

	if(!src->cache)
		folder_item_read_cache(src);
	mlist = msgcache_get_msg_list(src->cache);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;

		msg_fp = procmsg_open_message(msginfo);
		if (!msg_fp) {
			procmsg_msginfo_free(msginfo);
			continue;
		}

		strncpy2(buf,
			 msginfo->from ? msginfo->from :
			 cur_account && cur_account->address ?
			 cur_account->address : "unknown",
			 sizeof(buf));
		extract_address(buf);

		fprintf(mbox_fp, "From %s %s",
			buf, ctime(&msginfo->date_t));

		while (fgets(buf, sizeof(buf), msg_fp) != NULL) {
			if (!strncmp(buf, "From ", 5))
				fputc('>', mbox_fp);
			fputs(buf, mbox_fp);
		}
		fputc('\n', mbox_fp);

		fclose(msg_fp);
		procmsg_msginfo_free(msginfo);
	}

	g_slist_free(mlist);

	fclose(mbox_fp);

	return 0;
}
