/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2004 Colin Leroy
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
#ifndef __PARTIAL_DOWNLOAD_H__
#define __PARTIAL_DOWNLOAD_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <time.h>

#include "procmsg.h"

typedef enum {
	POP3_PARTIAL_DLOAD_UNKN	= 0,
	POP3_PARTIAL_DLOAD_DLOAD= 1,
	POP3_PARTIAL_DLOAD_DELE = 2
} PartialDownloadAction;

typedef enum {
	POP3_TOTALLY_RECEIVED	= 0,
	POP3_PARTIALLY_RECEIVED = 1,
	POP3_MUST_COMPLETE_RECV = 2
} PartialDownloadStatus;

gint   partial_msg_in_uidl_list	(MsgInfo	*msginfo);
int    partial_mark_for_download(MsgInfo	*msginfo);
int    partial_mark_for_delete	(MsgInfo	*msginfo);
int    partial_unmark		(MsgInfo	*msginfo);
gchar *partial_get_filename	(const gchar 	*server,
				 const gchar	*login, 
				 const gchar 	*muidl);
void   partial_delete_old	(const gchar 	*file);

#endif /* __PARTIAL_DOWNLOAD_H__ */
