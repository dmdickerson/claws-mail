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

#ifndef __PROCMIME_H__
#define __PROCMIME_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

typedef struct _MimeType	MimeType;
typedef struct _MimeInfo	MimeInfo;

typedef enum
{
	ENC_7BIT,
	ENC_8BIT,
	ENC_BINARY,
	ENC_QUOTED_PRINTABLE,
	ENC_BASE64,
	ENC_X_UUENCODE,
	ENC_UNKNOWN
} EncodingType;

typedef enum
{
	MIMETYPE_TEXT,
	MIMETYPE_IMAGE,
	MIMETYPE_AUDIO,
	MIMETYPE_VIDEO,
	MIMETYPE_APPLICATION,
	MIMETYPE_MESSAGE,
	MIMETYPE_MULTIPART,
	MIMETYPE_UNKNOWN,
} MimeMediaType;

#include <glib.h>
#include <stdio.h>

#include "procmsg.h"
#include "privacy.h"

struct _MimeType
{
	gchar *type;
	gchar *sub_type;

	gchar *extension;
};

/*
 * An example of MimeInfo structure:
 *
 * 1: +- message/rfc822			(root)
 *       |
 * 2:    +- multipart/mixed		(children of 1)
 *          |
 * 3:       +- multipart/alternative	(children of 2)
 *          |  |
 * 4:       |  +- text/plain		(children of 3)
 *          |  |
 * 5:       |  +- text/html		(next of 4)
 *          |
 * 6:       +- message/rfc822		(next of 3)
 *          |   |
 * 7:       |   ...			(children of 6)
 *          |
 * 8:       +- image/jpeg		(next of 6)
 */

struct _MimeInfo
{
	gchar *encoding;

	gchar *name;

	gchar *content_disposition;

	/* Internal data */
	gchar *filename;
	gboolean tmpfile;

	GNode *node;

	/* --- NEW MIME STUFF --- */
	/* Content-Type */
	MimeMediaType 	 type;
	gchar		*subtype;

	GHashTable	*parameters;

	/* Content-Transfer-Encoding */
	EncodingType	 encoding_type;

	/* Content-Description */
	gchar		*description;

	/* Content-ID */
	gchar		*id;

	guint		 offset;
	guint		 length;

	/* Privacy */
	PrivacyData	*privacy;
};

#define IS_BOUNDARY(s, bnd, len) \
	(bnd && s[0] == '-' && s[1] == '-' && !strncmp(s + 2, bnd, len))

/* MimeInfo handling */

MimeInfo *procmime_mimeinfo_new		(void);
void procmime_mimeinfo_free_all		(MimeInfo	*mimeinfo);

MimeInfo *procmime_mimeinfo_insert	(MimeInfo	*parent,
					 MimeInfo	*mimeinfo);
void procmime_mimeinfo_replace		(MimeInfo	*old_mimeinfo,
					 MimeInfo	*new_mimeinfo);

MimeInfo *procmime_mimeinfo_parent	(MimeInfo	*mimeinfo);
MimeInfo *procmime_mimeinfo_next	(MimeInfo	*mimeinfo);

MimeInfo *procmime_scan_message		(MsgInfo	*msginfo);
void procmime_scan_multipart_message	(MimeInfo	*mimeinfo,
					 FILE		*fp);
const gchar *procmime_mimeinfo_get_parameter
					(MimeInfo	*mimeinfo,
					 const gchar	*name);

/* scan headers */

void procmime_scan_encoding		(MimeInfo	*mimeinfo,
					 const gchar	*encoding);
void procmime_scan_content_type		(MimeInfo	*mimeinfo,
					 const gchar	*content_type);
void procmime_scan_content_disposition	(MimeInfo	*mimeinfo,
					 const gchar	*content_disposition);
void procmime_scan_content_description	(MimeInfo	*mimeinfo,
					 const gchar	*content_description);
void procmime_scan_subject              (MimeInfo       *mimeinfo,
			                 const gchar    *subject);
MimeInfo *procmime_scan_mime_header	(FILE		*fp);

gboolean procmime_decode_content	(MimeInfo	*mimeinfo);
gint procmime_get_part			(const gchar	*outfile,
					 MimeInfo	*mimeinfo);
FILE *procmime_get_text_content		(MimeInfo	*mimeinfo);
FILE *procmime_get_first_text_content	(MsgInfo	*msginfo);

gboolean procmime_find_string_part	(MimeInfo	*mimeinfo,
					 const gchar	*filename,
					 const gchar	*str,
					 gboolean	 case_sens);
gboolean procmime_find_string		(MsgInfo	*msginfo,
					 const gchar	*str,
					 gboolean	 case_sens);

gchar *procmime_get_tmp_file_name	(MimeInfo	*mimeinfo);

gchar *procmime_get_mime_type		(const gchar	*filename);

GList *procmime_get_mime_type_list	(void);

EncodingType procmime_get_encoding_for_charset	(const gchar	*charset);
EncodingType procmime_get_encoding_for_file	(const gchar	*file);
const gchar *procmime_get_encoding_str		(EncodingType	 encoding);
MimeInfo *procmime_scan_file			(gchar		*filename);
MimeInfo *procmime_scan_queue_file		(gchar 		*filename);
const gchar *procmime_get_type_str		(MimeMediaType 	 type);

void renderer_read_config(void);
void renderer_write_config(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PROCMIME_H__ */
