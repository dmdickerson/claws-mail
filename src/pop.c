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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

#include "intl.h"
#include "pop.h"
#include "socket.h"
#include "md5.h"
#include "prefs_account.h"
#include "utils.h"
#include "inc.h"
#include "recv.h"

static gint pop3_ok(SockInfo *sock, gchar *argbuf);
static void pop3_gen_send(SockInfo *sock, const gchar *format, ...);
static gint pop3_gen_recv(SockInfo *sock, gchar *buf, gint size);

gint pop3_greeting_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];

	if (pop3_ok(sock, buf) == PS_SUCCESS) {
		if (state->ac_prefs->protocol == A_APOP) {
			state->greeting = g_strdup(buf);
			return POP3_GETAUTH_APOP_SEND;
		} else
			return POP3_GETAUTH_USER_SEND;
	} else
		return -1;
}

gint pop3_getauth_user_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	g_return_val_if_fail(state->user != NULL, -1);

	inc_progress_update(state, POP3_GETAUTH_USER_SEND);

	pop3_gen_send(sock, "USER %s", state->user);

	return POP3_GETAUTH_USER_RECV;
}

gint pop3_getauth_user_recv(SockInfo *sock, gpointer data)
{
	if (pop3_ok(sock, NULL) == PS_SUCCESS)
		return POP3_GETAUTH_PASS_SEND;
	else
		return -1;
}

gint pop3_getauth_pass_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	g_return_val_if_fail(state->pass != NULL, -1);

	pop3_gen_send(sock, "PASS %s", state->pass);

	return POP3_GETAUTH_PASS_RECV;
}

gint pop3_getauth_pass_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	if (pop3_ok(sock, NULL) == PS_SUCCESS)
		return POP3_GETRANGE_STAT_SEND;
	else {
		log_warning(_("error occurred on authorization\n"));
		state->error_val = PS_AUTHFAIL;
		return -1;
	}
}

gint pop3_getauth_apop_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar *start, *end;
	gchar *apop_str;
	gchar md5sum[33];

	g_return_val_if_fail(state->user != NULL, -1);
	g_return_val_if_fail(state->pass != NULL, -1);

	inc_progress_update(state, POP3_GETAUTH_APOP_SEND);

	if ((start = strchr(state->greeting, '<')) == NULL) {
		log_warning(_("Required APOP timestamp not found "
			      "in greeting\n"));
		return -1;
	}

	if ((end = strchr(start, '>')) == NULL || end == start + 1) {
		log_warning(_("Timestamp syntax error in greeting\n"));
		return -1;
	}

	*(end + 1) = '\0';

	apop_str = g_strconcat(start, state->pass, NULL);
	md5_hex_digest(md5sum, apop_str);
	g_free(apop_str);

	pop3_gen_send(sock, "APOP %s %s", state->user, md5sum);

	return POP3_GETAUTH_APOP_RECV;
}

gint pop3_getauth_apop_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	if (pop3_ok(sock, NULL) == PS_SUCCESS)
		return POP3_GETRANGE_STAT_SEND;
	else {
		log_warning(_("error occurred on authorization\n"));
		state->error_val = PS_AUTHFAIL;
		return -1;
	}
}

gint pop3_getrange_stat_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETRANGE_STAT_SEND);

	pop3_gen_send(sock, "STAT");

	return POP3_GETRANGE_STAT_RECV;
}

gint pop3_getrange_stat_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE + 1];
	gint ok;

	if ((ok = pop3_ok(sock, buf)) == PS_SUCCESS) {
		if (sscanf(buf, "%d %d", &state->count, &state->total_bytes)
		    != 2) {
			log_warning(_("POP3 protocol error\n"));
			return -1;
		} else {
			if (state->count == 0)
				return POP3_LOGOUT_SEND;
			else {
				state->cur_msg = 1;
				state->new = state->count;
				if (state->ac_prefs->rmmail ||
				    state->ac_prefs->getall)
					return POP3_GETSIZE_LIST_SEND;
				else
					return POP3_GETRANGE_UIDL_SEND;
			}
		}
	} else if (ok == PS_PROTOCOL)
		return POP3_LOGOUT_SEND;
	else
		return -1;
}

gint pop3_getrange_last_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETRANGE_LAST_SEND);

	pop3_gen_send(sock, "LAST");

	return POP3_GETRANGE_LAST_RECV;
}

gint pop3_getrange_last_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE + 1];

	if (pop3_ok(sock, buf) == PS_SUCCESS) {
		gint last;

		if (sscanf(buf, "%d", &last) == 0) {
			log_warning(_("POP3 protocol error\n"));
			return -1;
		} else {
			if (state->count == last)
				return POP3_LOGOUT_SEND;
			else {
				state->new = state->count - last;
				state->cur_msg = last + 1;
				return POP3_GETSIZE_LIST_SEND;
			}
		}
	} else
		return POP3_GETSIZE_LIST_SEND;
}

gint pop3_getrange_uidl_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETRANGE_UIDL_SEND);

	pop3_gen_send(sock, "UIDL");

	return POP3_GETRANGE_UIDL_RECV;
}

gint pop3_getrange_uidl_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gboolean new = FALSE;
	gchar buf[POPBUFSIZE];
	gchar id[IDLEN + 1];

	if (pop3_ok(sock, NULL) != PS_SUCCESS) return POP3_GETRANGE_LAST_SEND;

	if (!state->id_table) new = TRUE;

	while (sock_gets(sock, buf, sizeof(buf)) >= 0) {
		gint num;

		if (buf[0] == '.') break;
		if (sscanf(buf, "%d %" Xstr(IDLEN) "s", &num, id) != 2)
			continue;

		if (new == FALSE &&
		    g_hash_table_lookup(state->id_table, id) == NULL) {
			state->new = state->count - num + 1;
			state->cur_msg = num;
			new = TRUE;
		}

		if (new == TRUE)
			state->new_id_list = g_slist_append
				(state->new_id_list, g_strdup(id));
		else
			state->id_list = g_slist_append
				(state->id_list, g_strdup(id));
	}

	if (new == TRUE)
		return POP3_GETSIZE_LIST_SEND;
	else
		return POP3_LOGOUT_SEND;
}

gint pop3_getsize_list_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETSIZE_LIST_SEND);

	pop3_gen_send(sock, "LIST");

	return POP3_GETSIZE_LIST_RECV;
}

gint pop3_getsize_list_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];

	if (pop3_ok(sock, NULL) != PS_SUCCESS) return POP3_LOGOUT_SEND;

	state->sizes = g_new0(gint, state->count + 1);
	state->cur_total_bytes = 0;

	while (sock_gets(sock, buf, sizeof(buf)) >= 0) {
		gint num, size;

		if (buf[0] == '.') break;
		if (sscanf(buf, "%d %d", &num, &size) != 2)
			continue;

		if (num <= state->count)
			state->sizes[num] = size;
		if (num < state->cur_msg)
			state->cur_total_bytes += size;
	}

	return POP3_RETR_SEND;
}

gint pop3_retr_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_RETR_SEND);

	pop3_gen_send(sock, "RETR %d", state->cur_msg);

	return POP3_RETR_RECV;
}

gint pop3_retr_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	const gchar *file;
	gint ok, drop_ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		state->cur_msg_bytes = 0;

		if (recv_write_to_file(sock, (file = get_tmp_file())) < 0) {
			state->inc_state = INC_NOSPACE;
			return -1;
		}

		state->cur_total_bytes += state->sizes[state->cur_msg];

		if ((drop_ok = inc_drop_message(file, state)) < 0) {
			state->inc_state = INC_ERROR;
			return -1;
		}
		if (drop_ok == 0 && state->ac_prefs->rmmail)
			return POP3_DELETE_SEND;

		if (state->new_id_list) {
			state->id_list = g_slist_append
				(state->id_list, state->new_id_list->data);
			state->new_id_list =
				g_slist_remove(state->new_id_list,
					       state->new_id_list->data);
		}

		if (state->cur_msg < state->count) {
			state->cur_msg++;
			return POP3_RETR_SEND;
		} else
			return POP3_LOGOUT_SEND;
	} else if (ok == PS_PROTOCOL)
		return POP3_LOGOUT_SEND;
	else
		return -1;
}

gint pop3_delete_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	//inc_progress_update(state, POP3_DELETE_SEND);

	pop3_gen_send(sock, "DELE %d", state->cur_msg);

	return POP3_DELETE_RECV;
}

gint pop3_delete_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		if (state->cur_msg < state->count) {
			state->cur_msg++;
			return POP3_RETR_SEND;
		} else
			return POP3_LOGOUT_SEND;
	} else if (ok == PS_PROTOCOL)
		return POP3_LOGOUT_SEND;
	else
		return -1;
}

gint pop3_logout_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_LOGOUT_SEND);

	pop3_gen_send(sock, "QUIT");

	return POP3_LOGOUT_RECV;
}

gint pop3_logout_recv(SockInfo *sock, gpointer data)
{
	if (pop3_ok(sock, NULL) == PS_SUCCESS)
		return -1;
	else
		return -1;
}

static gint pop3_ok(SockInfo *sock, gchar *argbuf)
{
	gint ok;
	gchar buf[POPBUFSIZE + 1];
	gchar *bufp;

	if ((ok = pop3_gen_recv(sock, buf, sizeof(buf))) == PS_SUCCESS) {
		bufp = buf;
		if (*bufp == '+' || *bufp == '-')
			bufp++;
		else
			return PS_PROTOCOL;

		while (isalpha(*bufp))
			bufp++;

		if (*bufp)
			*(bufp++) = '\0';

		if (!strcmp(buf, "+OK"))
			ok = PS_SUCCESS;
		else if (!strncmp(buf, "-ERR", 4)) {
			if (strstr(bufp, "lock") ||
				 strstr(bufp, "Lock") ||
				 strstr(bufp, "LOCK") ||
				 strstr(bufp, "wait"))
				ok = PS_LOCKBUSY;
			else
				ok = PS_PROTOCOL;

			if (*bufp)
				fprintf(stderr, "POP3: %s\n", bufp);
		} else
			ok = PS_PROTOCOL;

		if (argbuf)
			strcpy(argbuf, bufp);
	}

	return ok;
}

static void pop3_gen_send(SockInfo *sock, const gchar *format, ...)
{
	gchar buf[POPBUFSIZE + 1];
	va_list args;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf) - 2, format, args);
	va_end(args);

	if (!strncasecmp(buf, "PASS ", 5))
		log_print("POP3> PASS ********\n");
	else
		log_print("POP3> %s\n", buf);

	strcat(buf, "\r\n");
	sock_write(sock, buf, strlen(buf));
}

static gint pop3_gen_recv(SockInfo *sock, gchar *buf, gint size)
{
	if (sock_gets(sock, buf, size) < 0) {
		return PS_SOCKET;
	} else {
		strretchomp(buf);
		log_print("POP3< %s\n", buf);

		return PS_SUCCESS;
	}
}
