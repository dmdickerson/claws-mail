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

#include <glib.h>
#include <stdio.h>

typedef struct _MimeType	MimeType;
typedef struct _MimeInfo	MimeInfo;

#include "procmsg.h"

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
	MIME_TEXT,
	MIME_TEXT_HTML,
	MIME_MESSAGE_RFC822,
	MIME_APPLICATION,
	MIME_APPLICATION_OCTET_STREAM,
	MIME_MULTIPART,
	MIME_IMAGE,
	MIME_AUDIO,
	MIME_TEXT_ENRICHED,
	MIME_UNKNOWN
} ContentType;

struct _MimeType
{
	gchar *type;
	gchar *sub_type;

	gchar *extension;
};

/*
 * An example of MimeInfo structure:
 *
 * multipart/mixed            root  <-+ parent
 *                                    |
 *   multipart/alternative      children <-+ parent
 *                                         |
 *     text/plain                 children  --+
 *                                            |
 *     text/html                  next      <-+
 *
 *   message/rfc822             next  <-+ main
 *                                      |
 *                                sub (capsulated message)
 *
 *   image/jpeg                 next
 */

struct _MimeInfo
{
	gchar *encoding;

	EncodingType encoding_type;
	ContentType  mime_type;

	gchar *content_type;
	gchar *charset;
	gchar *name;
	gchar *boundary;

	gchar *content_disposition;
	gchar *filename;
	gchar *description;

	glong fpos;
	guint size;

	MimeInfo *main;
	MimeInfo *sub;

	MimeInfo *next;
	MimeInfo *parent;
	MimeInfo *children;

#if USE_GPGME
	MimeInfo *plaintext;
	gchar *plaintextfile;
	gchar *sigstatus;
	gchar *sigstatus_full;
	gboolean sig_ok;
#endif

	gint level;
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

MimeInfo *procmime_mimeinfo_next	(MimeInfo	*mimeinfo);

MimeInfo *procmime_scan_message		(MsgInfo	*msginfo);
void procmime_scan_multipart_message	(MimeInfo	*mimeinfo,
					 FILE		*fp);

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

FILE *procmime_decode_content		(FILE		*outfp,
					 FILE		*infp,
					 MimeInfo	*mimeinfo);
gint procmime_get_part			(const gchar	*outfile,
					 const gchar	*infile,
					 MimeInfo	*mimeinfo);
FILE *procmime_get_text_content		(MimeInfo	*mimeinfo,
					 FILE		*infp);
FILE *procmime_get_first_text_content	(MsgInfo	*msginfo);

gboolean procmime_find_string_part	(MimeInfo	*mimeinfo,
					 const gchar	*filename,
					 const gchar	*str,
					 gboolean	 case_sens);
gboolean procmime_find_string		(MsgInfo	*msginfo,
					 const gchar	*str,
					 gboolean	 case_sens);

gchar *procmime_get_tmp_file_name	(MimeInfo	*mimeinfo);

ContentType procmime_scan_mime_type	(const gchar	*mime_type);
gchar *procmime_get_mime_type		(const gchar	*filename);

GList *procmime_get_mime_type_list	(void);

EncodingType procmime_get_encoding_for_charset	(const gchar	*charset);
EncodingType procmime_get_encoding_for_file	(const gchar	*file);
const gchar *procmime_get_encoding_str		(EncodingType	 encoding);

void renderer_read_config(void);
void renderer_write_config(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PROCMIME_H__ */
