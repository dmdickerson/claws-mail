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
#ifdef WIN32
#include <defs.h>
#endif

#include "prefs.h"
#include "utils.h"

/*!
 *\brief	Open preferences file for reading
 *
 *\param	path Filename with path of preferences file to read
 *
 *\return	PrefFile * preferences file struct
 */
PrefFile *prefs_read_open(const gchar *path)
{
	PrefFile *pfile;
	FILE *fp;

	g_return_val_if_fail(path != NULL, NULL);

	if ((fp = fopen(path, "rb")) == NULL) {
		FILE_OP_ERROR(path, "fopen");
		return NULL;
	}

	pfile = g_new(PrefFile, 1);
	pfile->fp = fp;
	pfile->orig_fp = NULL;
	pfile->path = g_strdup(path);
	pfile->writing = FALSE;

	return pfile;
}

/*!
 *\brief	Open preferences file for writing
 *		Prefs are written to a temp file: Call prefs_write_close()
 *		to rename this to the final filename
 *
 *\param	path Filename with path of preferences file to write
 *
 *\return	PrefFile * preferences file struct
 */
PrefFile *prefs_write_open(const gchar *path)
{
	PrefFile *pfile;
	gchar *tmppath;
	FILE *fp;

	g_return_val_if_fail(path != NULL, NULL);

	if (prefs_is_readonly(path)) {
		g_warning("no permission - %s\n", path);
		return NULL;
	}

	tmppath = g_strconcat(path, ".tmp", NULL);
	if ((fp = fopen(tmppath, "wb")) == NULL) {
		FILE_OP_ERROR(tmppath, "fopen");
		g_free(tmppath);
		return NULL;
	}

	if (change_file_mode_rw(fp, tmppath) < 0)
		FILE_OP_ERROR(tmppath, "chmod");

	g_free(tmppath);

	pfile = g_new(PrefFile, 1);
	pfile->fp = fp;
	pfile->orig_fp = NULL;
	pfile->path = g_strdup(path);
	pfile->writing = TRUE;

	return pfile;
}

/*!
 *\brief	Close and free preferences file
 *		Creates final file from temp, creates backup
 *
 *\param	pfile Preferences file struct
 *
 *\return	0 on success, -1 on failure
 */
#define PREFS_FILE_FREE() \
{ \
  if (path)	g_free(path); \
  if (tmppath)	g_free(tmppath); \
  if (bakpath)	g_free(bakpath); \
  if (pfile)	g_free(pfile); \
}
gint prefs_file_close(PrefFile *pfile)
{
	FILE *fp, *orig_fp;
	gchar *path;
	gchar *tmppath;
	gchar *bakpath = NULL;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(pfile != NULL, -1);

	fp = pfile->fp;
	orig_fp = pfile->orig_fp;
	path = pfile->path;

	if (!pfile->writing) {
		fclose(fp);
		g_free(pfile);
		g_free(path);
		return 0;
	}

	if (orig_fp) {
    		while (fgets(buf, sizeof(buf), orig_fp) != NULL) {
			/* next block */
			if (buf[0] == '[') {
				if (fputs(buf, fp)  == EOF) {
					g_warning("failed to write configuration to file\n");
					fclose(orig_fp);
					prefs_file_close_revert(pfile);
				
					return -1;
				}
				break;
			}
		}
		
		while (fgets(buf, sizeof(buf), orig_fp) != NULL)
			if (fputs(buf, fp) == EOF) {
				g_warning("failed to write configuration to file\n");
				fclose(orig_fp);
				prefs_file_close_revert(pfile);			
				
				return -1;
			}
	}

	tmppath = g_strconcat(path, ".tmp", NULL);
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmppath, "fclose");
		unlink(tmppath);
		g_free(path);
		g_free(tmppath);
		return -1;
	}

	if (is_file_exist(path)) {
		bakpath = g_strconcat(path, ".bak", NULL);
		if (Xrename(path, bakpath) < 0) {
			FILE_OP_ERROR(path, "rename");
			unlink(tmppath);
			g_free(path);
			g_free(tmppath);
			g_free(bakpath);
			return -1;
		}
	}

	if (Xrename(tmppath, path) < 0) {
		FILE_OP_ERROR(tmppath, "rename");
		unlink(tmppath);
		g_free(path);
		g_free(tmppath);
		g_free(bakpath);
		return -1;
	}

	g_free(pfile);
	g_free(path);
	g_free(tmppath);
	g_free(bakpath);
	return 0;
}
#undef PREFS_FILE_FREE

/*!
 *\brief	Close and free preferences file, delete temp file
 *
 *\param	pfile Preferences file struct
 */
gint prefs_file_close_revert(PrefFile *pfile)
{
	gchar *tmppath;

	g_return_val_if_fail(pfile != NULL, -1);

	if (pfile->orig_fp)
		fclose(pfile->orig_fp);
	if (pfile->writing)
		tmppath = g_strconcat(pfile->path, ".tmp", NULL);
	fclose(pfile->fp);
	if (pfile->writing) {
		if (unlink(tmppath) < 0) FILE_OP_ERROR(tmppath, "unlink");
		g_free(tmppath);
	}
	g_free(pfile->path);
	g_free(pfile);

	return 0;
}

/*!
 *\brief	Check if "path" is a file and readonly
 */
gboolean prefs_is_readonly(const gchar * path)
{
	if (path == NULL)
		return TRUE;

	return (access(path, W_OK) != 0 && access(path, F_OK) == 0);
}

/*!
 *\brief	Check if "rcfile" is in rcdir, a file and readonly
 */
gboolean prefs_rc_is_readonly(const gchar * rcfile)
{
	gboolean result;
	gchar * rcpath;

	if (rcfile == NULL)
		return TRUE;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, rcfile, NULL);
	result = prefs_is_readonly(rcpath);
	g_free(rcpath);

	return result;
}

/*!
 *\brief	Selects current section in preferences file
 *		Creates section if file is written
 *
 *\param	pfile Preferences file struct
 *
 *\return	0 on success, -1 on failure
 */
gint prefs_set_block_label(PrefFile *pfile, const gchar *label)
{
	gchar *block_label;
	gchar buf[BUFFSIZE];
	
	block_label = g_strdup_printf("[%s]", label);
	if (!pfile->writing) {
		while (fgets(buf, sizeof(buf), pfile->fp) != NULL) {
			gint val;
			
			val = strncmp(buf, block_label, strlen(block_label));
			if (val == 0) {
				debug_print("Found %s\n", block_label);
				break;
			}
		}
	} else {
		if ((pfile->orig_fp = fopen(pfile->path, "rb")) != NULL) {
			gboolean block_matched = FALSE;

			while (fgets(buf, sizeof(buf), pfile->orig_fp) != NULL) {
				gint val;
				
				val = strncmp(buf, block_label, strlen(block_label));
				if (val == 0) {
					debug_print("Found %s\n", block_label);
					block_matched = TRUE;
					break;
				} else {
					if (fputs(buf, pfile->fp) == EOF) {
						g_warning("failed to write configuration to file\n");
						fclose(pfile->orig_fp);
						prefs_file_close_revert(pfile);
						g_free(block_label);
						
						return -1;
					}
				}
			}
			
			if (!block_matched) {
				fclose(pfile->orig_fp);
				pfile->orig_fp = NULL;
			}
			
			if (fputs(block_label, pfile->fp) == EOF ||
			    fputc('\n', pfile->fp) == EOF) {
				g_warning("failed to write configuration to file\n");
				fclose(pfile->orig_fp);
				prefs_file_close_revert(pfile);
				g_free(block_label);
						
				return -1;
			}
		}
	}

	g_free(block_label);

	return 0;
}
