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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "intl.h"
#include "smtp.h"
#include "socket.h"
#include "md5.h"
#include "base64.h"
#include "utils.h"
#include "log.h"

static gint verbose = 1;

#define UI_UPDATE(session, phase) \
{ \
	if (SESSION(session)->ui_func) \
		SESSION(session)->ui_func(SESSION(session), phase); \
}

static gint smtp_starttls(SMTPSession *session);
static gint smtp_auth_cram_md5(SMTPSession *session, gchar *buf, gint len);
static gint smtp_auth_login(SMTPSession *session, gchar *buf, gint len);

static gint smtp_ok(SockInfo *sock, gchar *buf, gint len);

Session *smtp_session_new(void)
{
	SMTPSession *session;

	session = g_new0(SMTPSession, 1);
	SESSION(session)->type             = SESSION_SMTP;
	SESSION(session)->server           = NULL;
	SESSION(session)->sock             = NULL;
	SESSION(session)->connected        = FALSE;
	SESSION(session)->phase            = SESSION_READY;
	SESSION(session)->last_access_time = 0;
	SESSION(session)->data             = NULL;

	SESSION(session)->destroy          = smtp_session_destroy;
	SESSION(session)->ui_func          = NULL;

	session->avail_auth_type           = 0;
	session->user                      = NULL;
	session->pass                      = NULL;

	return SESSION(session);
}

void smtp_session_destroy(Session *session)
{
	sock_close(session->sock);
	session->sock = NULL;

	g_free(SMTP_SESSION(session)->user);
	g_free(SMTP_SESSION(session)->pass);
}

#if USE_OPENSSL
gint smtp_connect(SMTPSession *session, const gchar *server, gushort port,
		  const gchar *domain, const gchar *user, const gchar *pass,
		  SSLType ssl_type)
#else
gint smtp_connect(SMTPSession *session, const gchar *server, gushort port,
		  const gchar *domain, const gchar *user, const gchar *pass)
#endif
{
	SockInfo *sock;
	gboolean use_esmtp;
	SMTPAuthType avail_auth_type = 0;
	gint val;

	g_return_val_if_fail(session != NULL, SM_ERROR);
	g_return_val_if_fail(server != NULL, SM_ERROR);

#if USE_OPENSSL
	use_esmtp = user != NULL || ssl_type == SSL_STARTTLS;
#else
	use_esmtp = user != NULL;
#endif

	SESSION(session)->server = g_strdup(server);
	session->user = user ? g_strdup(user) : NULL;
	session->pass = pass ? g_strdup(pass) : user ? g_strdup("") : NULL;

	UI_UPDATE(session, SMTP_CONNECT);

	if ((sock = sock_connect(server, port)) == NULL) {
		log_warning(_("Can't connect to SMTP server: %s:%d\n"),
			    server, port);
		return SM_ERROR;
	}

#if USE_OPENSSL
	if (ssl_type == SSL_TUNNEL && !ssl_init_socket(sock)) {
		log_warning(_("SSL connection failed"));
		sock_close(sock);
		return SM_ERROR;
	}
#endif

	if (smtp_ok(sock, NULL, 0) != SM_OK) {
		log_warning(_("Error occurred while connecting to %s:%d\n"),
			    server, port);
		sock_close(sock);
		return SM_ERROR;
	}

	SESSION(session)->sock = sock;
	SESSION(session)->connected = TRUE;

	if (!domain)
		domain = get_domain_name();

	if (use_esmtp)
		val = smtp_ehlo(session, domain, &avail_auth_type);
	else
		val = smtp_helo(session, domain);
	if (val != SM_OK) {
		log_warning(use_esmtp?	_("Error occurred while sending EHLO\n"):
					_("Error occurred while sending HELO\n"));
		return val;
	}

#if USE_OPENSSL
	/* if we have a user to authenticate and no auth methods, but starttls,
	   try to starttls */
	if (ssl_type == SSL_NONE && avail_auth_type == SMTPAUTH_TLS_AVAILABLE 
	    && user != NULL)
		ssl_type = SSL_STARTTLS;

	if (ssl_type == SSL_STARTTLS) {
		val = smtp_starttls(session);
		if (val != SM_OK) {
			log_warning(_("Error occurred while sending STARTTLS\n"));
			return val;
		}
		if (!ssl_init_socket_with_method(sock, SSL_METHOD_TLSv1)) {
			return SM_ERROR;
		}
		val = smtp_ehlo(session, domain, &avail_auth_type);
		if (val != SM_OK) {
			log_warning(_("Error occurred while sending EHLO\n"));
			return val;
		}
	}
#endif

	session->avail_auth_type = avail_auth_type;

	return 0;
}

gint smtp_from(SMTPSession *session, const gchar *from)
{
	gchar buf[MSGBUFSIZE];

	g_return_val_if_fail(session != NULL, SM_ERROR);
	g_return_val_if_fail(from != NULL, SM_ERROR);

	UI_UPDATE(session, SMTP_FROM);

	if (strchr(from, '<'))
		g_snprintf(buf, sizeof(buf), "MAIL FROM: %s", from);
	else
		g_snprintf(buf, sizeof(buf), "MAIL FROM: <%s>", from);

	sock_printf(SESSION(session)->sock, "%s\r\n", buf);
	if (verbose)
		log_print("SMTP> %s\n", buf);

	return smtp_ok(SESSION(session)->sock, NULL, 0);
}

gint smtp_auth(SMTPSession *session, SMTPAuthType forced_auth_type)
{
	gchar buf[MSGBUFSIZE];
	SMTPAuthType authtype = 0;
	guchar hexdigest[33];
	gchar *challenge, *response, *response64;
	gint challengelen;
	SockInfo *sock;

	g_return_val_if_fail(session != NULL, SM_ERROR);
	g_return_val_if_fail(session->user != NULL, SM_ERROR);

	UI_UPDATE(session, SMTP_AUTH);

	sock = SESSION(session)->sock;

	if ((forced_auth_type == SMTPAUTH_CRAM_MD5 ||
	     (forced_auth_type == 0 &&
	      (session->avail_auth_type & SMTPAUTH_CRAM_MD5) != 0)) &&
	    smtp_auth_cram_md5(session, buf, sizeof(buf)) == SM_OK)
		authtype = SMTPAUTH_CRAM_MD5;
	else if ((forced_auth_type == SMTPAUTH_LOGIN ||
		  (forced_auth_type == 0 &&
		   (session->avail_auth_type & SMTPAUTH_LOGIN) != 0)) &&
		 smtp_auth_login(session, buf, sizeof(buf)) == SM_OK)
		authtype = SMTPAUTH_LOGIN;
	else {
		log_warning(_("SMTP AUTH not available\n"));
		return SM_AUTHFAIL;
	}

	switch (authtype) {
	case SMTPAUTH_LOGIN:
		if (!strncmp(buf, "334 ", 4))
			base64_encode(buf, session->user, strlen(session->user));
		else
			/* Server rejects AUTH */
			g_snprintf(buf, sizeof(buf), "*");

		sock_printf(sock, "%s\r\n", buf);
		if (verbose) log_print("ESMTP> [USERID]\n");

		smtp_ok(sock, buf, sizeof(buf));

		if (!strncmp(buf, "334 ", 4))
			base64_encode(buf, session->pass, strlen(session->pass));
		else
			/* Server rejects AUTH */
			g_snprintf(buf, sizeof(buf), "*");

		sock_printf(sock, "%s\r\n", buf);
		if (verbose) log_print("ESMTP> [PASSWORD]\n");
		break;
	case SMTPAUTH_CRAM_MD5:
		if (!strncmp(buf, "334 ", 4)) {
			challenge = g_malloc(strlen(buf + 4) + 1);
			challengelen = base64_decode(challenge, buf + 4, -1);
			challenge[challengelen] = '\0';
			if (verbose)
				log_print("ESMTP< [Decoded: %s]\n", challenge);

			g_snprintf(buf, sizeof(buf), "%s", session->pass);
			md5_hex_hmac(hexdigest, challenge, challengelen,
				     buf, strlen(session->pass));
			g_free(challenge);

			response = g_strdup_printf
				("%s %s", session->user, hexdigest);
			if (verbose)
				log_print("ESMTP> [Encoded: %s]\n", response);

			response64 = g_malloc((strlen(response) + 3) * 2 + 1);
			base64_encode(response64, response, strlen(response));
			g_free(response);

			sock_printf(sock, "%s\r\n", response64);
			if (verbose) log_print("ESMTP> %s\n", response64);
			g_free(response64);
		} else {
			/* Server rejects AUTH */
			g_snprintf(buf, sizeof(buf), "*");
			sock_printf(sock, "%s\r\n", buf);
			if (verbose)
				log_print("ESMTP> %s\n", buf);
		}
		break;
	case SMTPAUTH_DIGEST_MD5:
        default:
        	/* stop smtp_auth when no correct authtype */
		g_snprintf(buf, sizeof(buf), "*");
		sock_printf(sock, "%s\r\n", buf);
		if (verbose) log_print("ESMTP> %s\n", buf);
		break;
	}

	return smtp_ok(sock, NULL, 0);
}

gint smtp_ehlo(SMTPSession *session, const gchar *hostname,
	       SMTPAuthType *avail_auth_type)
{
	SockInfo *sock;
	gchar buf[MSGBUFSIZE];

	UI_UPDATE(session, SMTP_EHLO);

	sock = SESSION(session)->sock;

	*avail_auth_type = 0;

	sock_printf(sock, "EHLO %s\r\n", hostname);
	if (verbose)
		log_print("ESMTP> EHLO %s\n", hostname);

	while ((sock_gets(sock, buf, sizeof(buf) - 1)) != -1) {
		if (strlen(buf) < 4)
			return SM_ERROR;
		strretchomp(buf);

		if (verbose)
			log_print("ESMTP< %s\n", buf);

		if (strncmp(buf, "250-", 4) == 0) {
			gchar *p = buf;
			p += 4;
			if (g_strncasecmp(p, "AUTH", 4) == 0) {
				p += 5;
				if (strcasestr(p, "LOGIN"))
					*avail_auth_type |= SMTPAUTH_LOGIN;
				if (strcasestr(p, "CRAM-MD5"))
					*avail_auth_type |= SMTPAUTH_CRAM_MD5;
				if (strcasestr(p, "DIGEST-MD5"))
					*avail_auth_type |= SMTPAUTH_DIGEST_MD5;
			} else if (g_strncasecmp(p, "STARTTLS", 8) == 0) {
				p += 9;
				*avail_auth_type |= SMTPAUTH_TLS_AVAILABLE;
			}
		} else if ((buf[0] == '1' || buf[0] == '2' || buf[0] == '3') &&
		    (buf[3] == ' ' || buf[3] == '\0'))
			return SM_OK;
		else if (buf[3] != '-')
			return SM_ERROR;
		else if (buf[0] == '5' && buf[1] == '0' &&
			 (buf[2] == '4' || buf[2] == '3' || buf[2] == '1'))
			return SM_ERROR;
	}

	return SM_UNRECOVERABLE;
}

static gint smtp_starttls(SMTPSession *session)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_STARTTLS);

	sock = SESSION(session)->sock;

	sock_printf(sock, "STARTTLS\r\n");
	if (verbose)
		log_print("ESMTP> STARTTLS\n");

	return smtp_ok(sock, NULL, 0);
}

static gint smtp_auth_cram_md5(SMTPSession *session, gchar *buf, gint len)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_AUTH);

	sock = SESSION(session)->sock;

	sock_printf(sock, "AUTH CRAM-MD5\r\n");
	if (verbose)
		log_print("ESMTP> AUTH CRAM-MD5\n");

	return smtp_ok(sock, buf, len);
}

static gint smtp_auth_login(SMTPSession *session, gchar *buf, gint len)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_AUTH);

	sock = SESSION(session)->sock;

	sock_printf(sock, "AUTH LOGIN\r\n");
	if (verbose)
		log_print("ESMTP> AUTH LOGIN\n");

	return smtp_ok(sock, buf, len);
}

gint smtp_helo(SMTPSession *session, const gchar *hostname)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_HELO);

	sock = SESSION(session)->sock;

	sock_printf(sock, "HELO %s\r\n", hostname);
	if (verbose)
		log_print("SMTP> HELO %s\n", hostname);

	return smtp_ok(sock, NULL, 0);
}

gint smtp_rcpt(SMTPSession *session, const gchar *to)
{
	SockInfo *sock;
	gchar buf[MSGBUFSIZE];

	UI_UPDATE(session, SMTP_RCPT);

	sock = SESSION(session)->sock;

	if (strchr(to, '<'))
		g_snprintf(buf, sizeof(buf), "RCPT TO: %s", to);
	else
		g_snprintf(buf, sizeof(buf), "RCPT TO: <%s>", to);

	sock_printf(sock, "%s\r\n", buf);
	if (verbose)
		log_print("SMTP> %s\n", buf);

	return smtp_ok(sock, NULL, 0);
}

gint smtp_data(SMTPSession *session)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_DATA);

	sock = SESSION(session)->sock;

	sock_printf(sock, "DATA\r\n");
	if (verbose)
		log_print("SMTP> DATA\n");

	return smtp_ok(sock, NULL, 0);
}

gint smtp_rset(SMTPSession *session)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_RSET);

	sock = SESSION(session)->sock;

	sock_printf(sock, "RSET\r\n");
	if (verbose)
		log_print("SMTP> RSET\n");

	return smtp_ok(sock, NULL, 0);
}

gint smtp_quit(SMTPSession *session)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_QUIT);

	sock = SESSION(session)->sock;

	sock_printf(sock, "QUIT\r\n");
	if (verbose)
		log_print("SMTP> QUIT\n");

	return smtp_ok(sock, NULL, 0);
}

gint smtp_eom(SMTPSession *session)
{
	SockInfo *sock;

	UI_UPDATE(session, SMTP_EOM);

	sock = SESSION(session)->sock;

	sock_printf(sock, ".\r\n");
	if (verbose)
		log_print("SMTP> . (EOM)\n");

	return smtp_ok(sock, NULL, 0);
}

static gint smtp_ok(SockInfo *sock, gchar *buf, gint len)
{
	gchar tmpbuf[MSGBUFSIZE];

	if (!buf) {
		buf = tmpbuf;
		len = sizeof(tmpbuf);
	}

	while ((sock_gets(sock, buf, len - 1)) != -1) {
		if (strlen(buf) < 4)
			return SM_ERROR;
		strretchomp(buf);

		if (verbose)
			log_print("SMTP< %s\n", buf);

		if ((buf[0] == '1' || buf[0] == '2' || buf[0] == '3') &&
		    (buf[3] == ' ' || buf[3] == '\0'))
			return SM_OK;
		else if (buf[3] != '-')
			return SM_ERROR;
		else if (buf[0] == '5' && buf[1] == '0' &&
			 (buf[2] == '4' || buf[2] == '3' || buf[2] == '1'))
			return SM_ERROR;
	}

	return SM_UNRECOVERABLE;
}
