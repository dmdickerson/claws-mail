/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef __ESMTP_H__
#define __ESMTP_H__

#include <glib.h>
#include "socket.h"

typedef enum
{
	SMTPAUTH_LOGIN      = 1,
	SMTPAUTH_CRAM_MD5   = 2,
	SMTPAUTH_DIGEST_MD5 = 3
} SMTPAuthType;

gint esmtp_auth_login(SockInfo *sock);
gint esmtp_auth_cram_md5(SockInfo *sock);
gint esmtp_auth(SockInfo *sock, SMTPAuthType authtype,
		const gchar *userid, const gchar *passwd,
		gboolean use_smtp_auth);
gint esmtp_ok(SockInfo *sock);

#endif /* __ESMTP_H__ */
