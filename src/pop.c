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
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "intl.h"
#include "pop.h"
#include "socket.h"
#include "md5.h"
#include "prefs_account.h"
#include "utils.h"
#include "inc.h"
#include "recv.h"
#include "selective_download.h"
#include "log.h"
#if USE_OPENSSL
#  include "ssl.h"
#endif

#define LOOKUP_NEXT_MSG()							\
{										\
	Pop3MsgInfo *msg;							\
	PrefsAccount *ac = state->ac_prefs;					\
	gint size;								\
	gboolean size_limit_over;						\
										\
	for (;;) {								\
		msg = &state->msg[state->cur_msg];				\
		size = msg->size;						\
		size_limit_over =						\
		    (ac->enable_size_limit &&					\
		     ac->size_limit > 0 &&					\
		     size > ac->size_limit * 1024);				\
										\
		if (ac->rmmail &&						\
		    msg->recv_time != RECV_TIME_NONE &&				\
		    msg->recv_time != RECV_TIME_KEEP &&				\
		    state->current_time - msg->recv_time >=			\
		    ac->msg_leave_time * 24 * 60 * 60) {			\
			log_print(_("POP3: Deleting expired message %d\n"),	\
				  state->cur_msg);				\
			return POP3_DELETE_SEND;				\
		}								\
										\
		if (size_limit_over)						\
			log_print(_("POP3: Skipping message %d (%d bytes)\n"),	\
				  state->cur_msg, size);			\
										\
		if (size == 0 || msg->received || size_limit_over) {		\
			state->cur_total_bytes += size;				\
			if (state->cur_msg == state->count)			\
				return POP3_LOGOUT_SEND;			\
			else							\
				state->cur_msg++;				\
		} else								\
			break;							\
	}									\
}

static gint pop3_ok(SockInfo *sock, gchar *argbuf);
static void pop3_gen_send(SockInfo *sock, const gchar *format, ...);
static gint pop3_gen_recv(SockInfo *sock, gchar *buf, gint size);
static gboolean pop3_sd_get_next (Pop3State *state);
static void pop3_sd_new_header(Pop3State *state);
gboolean pop3_sd_state(Pop3State *state, gint cur_state, guint *next_state);

gint pop3_greeting_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];
	gint ok;

	if ((ok = pop3_ok(sock, buf)) == PS_SUCCESS) {
		state->greeting = g_strdup(buf);
#if USE_OPENSSL
		if (state->ac_prefs->ssl_pop == SSL_STARTTLS)
			return POP3_STLS_SEND;
#endif
		if (state->ac_prefs->protocol == A_APOP)
			return POP3_GETAUTH_APOP_SEND;
		else
			return POP3_GETAUTH_USER_SEND;
	} else {
		state->error_val = ok;
		return -1;
	}
}

#if USE_OPENSSL
gint pop3_stls_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "STLS");

	return POP3_STLS_RECV;
}

gint pop3_stls_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		if (!ssl_init_socket_with_method(sock, SSL_METHOD_TLSv1)) {
			state->error_val = PS_SOCKET;
			return -1;
		}
		if (state->ac_prefs->protocol == A_APOP)
			return POP3_GETAUTH_APOP_SEND;
		else
			return POP3_GETAUTH_USER_SEND;
	} else if (ok == PS_PROTOCOL) {
		log_warning(_("can't start TLS session\n"));
		state->error_val = ok;
		return POP3_LOGOUT_SEND;
	} else {
		state->error_val = ok;
		return -1;
	}
}
#endif /* USE_OPENSSL */

gint pop3_getauth_user_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	g_return_val_if_fail(state->user != NULL, -1);

	pop3_gen_send(sock, "USER %s", state->user);

	return POP3_GETAUTH_USER_RECV;
}

gint pop3_getauth_user_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	if (pop3_ok(sock, NULL) == PS_SUCCESS)
		return POP3_GETAUTH_PASS_SEND;
	else {
		log_warning(_("error occurred on authentication\n"));
		state->error_val = PS_AUTHFAIL;
		return -1;
	}
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
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS)
		return POP3_GETRANGE_STAT_SEND;
	else if (ok == PS_LOCKBUSY) {
		log_warning(_("mailbox is locked\n"));
		state->error_val = ok;
		return -1;
	} else {
		log_warning(_("error occurred on authentication\n"));
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

	if ((start = strchr(state->greeting, '<')) == NULL) {
		log_warning(_("Required APOP timestamp not found "
			      "in greeting\n"));
		state->error_val = PS_PROTOCOL;
		return -1;
	}

	if ((end = strchr(start, '>')) == NULL || end == start + 1) {
		log_warning(_("Timestamp syntax error in greeting\n"));
		state->error_val = PS_PROTOCOL;
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
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS)
		return POP3_GETRANGE_STAT_SEND;
	else if (ok == PS_LOCKBUSY) {
		log_warning(_("mailbox is locked\n"));
		state->error_val = ok;
		return -1;
	} else {
		log_warning(_("error occurred on authentication\n"));
		state->error_val = PS_AUTHFAIL;
		return -1;
	}
}

gint pop3_getrange_stat_send(SockInfo *sock, gpointer data)
{
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
			state->error_val = PS_PROTOCOL;
			return -1;
		} else {
			if (state->count == 0) {
				state->uidl_is_valid = TRUE;
				return POP3_LOGOUT_SEND;
			} else {
				state->msg = g_new0
					(Pop3MsgInfo, state->count + 1);
				state->cur_msg = 1;
				return POP3_GETRANGE_UIDL_SEND;
			}
		}
	} else if (ok == PS_PROTOCOL) {
		state->error_val = ok;
		return POP3_LOGOUT_SEND;
	} else {
		state->error_val = ok;
		return -1;
	}
}

gint pop3_getrange_last_send(SockInfo *sock, gpointer data)
{
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
			state->error_val = PS_PROTOCOL;
			return -1;
		} else {
			if (state->count == last)
				return POP3_LOGOUT_SEND;
			else {
				state->cur_msg = last + 1;
				return POP3_GETSIZE_LIST_SEND;
			}
		}
	} else
		return POP3_GETSIZE_LIST_SEND;
}

gint pop3_getrange_uidl_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "UIDL");

	return POP3_GETRANGE_UIDL_RECV;
}

gint pop3_getrange_uidl_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gboolean new = FALSE;
	gboolean get_all = FALSE;
	gchar buf[POPBUFSIZE];
	gchar id[IDLEN + 1];
	gint len;
	gint next_state;

	if (!state->uidl_table) new = TRUE;
#if 0
	if (state->ac_prefs->getall ||
	    (state->ac_prefs->rmmail && state->ac_prefs->msg_leave_time == 0))
#endif
	if (state->ac_prefs->getall)
		get_all = TRUE;

	if (pop3_ok(sock, NULL) != PS_SUCCESS) {
		/* UIDL is not supported */
		if (pop3_sd_state(state, POP3_GETRANGE_UIDL_RECV, &next_state))
			return next_state;

		if (!get_all)
			return POP3_GETRANGE_LAST_SEND;
		else
			return POP3_GETSIZE_LIST_SEND;
	}

	while ((len = sock_gets(sock, buf, sizeof(buf))) > 0) {
		gint num;
		time_t recv_time;

		if (buf[0] == '.') break;
		if (sscanf(buf, "%d %" Xstr(IDLEN) "s", &num, id) != 2)
			continue;
		if (num <= 0 || num > state->count) continue;

		state->msg[num].uidl = g_strdup(id);

		if (!state->uidl_table) continue;

		recv_time = (time_t)g_hash_table_lookup(state->uidl_table, id);
		state->msg[num].recv_time = recv_time;

		if (!get_all && recv_time != RECV_TIME_NONE)
			state->msg[num].received = TRUE;

		if (new == FALSE &&
		    (get_all || recv_time == RECV_TIME_NONE ||
		     state->ac_prefs->rmmail)) {
			state->cur_msg = num;
			new = TRUE;
		}
	}

	if (len < 0) {
		log_error(_("Socket error\n"));
		state->error_val = PS_SOCKET;
		return -1;
	}

	state->uidl_is_valid = TRUE;
	if (pop3_sd_state(state, POP3_GETRANGE_UIDL_RECV, &next_state))
		return next_state;

	if (new == TRUE)
		return POP3_GETSIZE_LIST_SEND;
	else
		return POP3_LOGOUT_SEND;
}

gint pop3_getsize_list_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "LIST");

	return POP3_GETSIZE_LIST_RECV;
}

gint pop3_getsize_list_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];
	gint len;
	gint next_state;

	if (pop3_ok(sock, NULL) != PS_SUCCESS) return POP3_LOGOUT_SEND;

	state->cur_total_bytes = 0;
	state->cur_total_recv_bytes = 0;

	while ((len = sock_gets(sock, buf, sizeof(buf))) > 0) {
		guint num, size;

		if (buf[0] == '.') break;
		if (sscanf(buf, "%u %u", &num, &size) != 2) {
			state->error_val = PS_PROTOCOL;
			return -1;
		}

		if (num > 0 && num <= state->count)
			state->msg[num].size = size;
		if (num > 0 && num < state->cur_msg)
			state->cur_total_bytes += size;
	}

	if (len < 0) {
		log_error(_("Socket error\n"));
		state->error_val = PS_SOCKET;
		return -1;
	}

	if (pop3_sd_state(state, POP3_GETSIZE_LIST_RECV, &next_state))
		return next_state;

	LOOKUP_NEXT_MSG();
	return POP3_RETR_SEND;
}
 
gint pop3_top_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_TOP_SEND); 

	pop3_gen_send(sock, "TOP %i 0", state->cur_msg );

	return POP3_TOP_RECV;
}

gint pop3_top_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar *filename, *path;
	gint next_state;
	gint write_val;
	if (pop3_ok(sock, NULL) != PS_SUCCESS) 
		return POP3_LOGOUT_SEND;

	path = g_strconcat(get_header_cache_dir(), G_DIR_SEPARATOR_S, NULL);

	if ( !is_dir_exist(path) )
		make_dir_hier(path);
	
	filename = g_strdup_printf("%s%i", path, state->cur_msg);
				   
	if ( (write_val = recv_write_to_file(sock, filename)) < 0) {
		state->error_val = (write_val == -1 ? PS_IOERR : PS_SOCKET);
		g_free(path);
		g_free(filename);
		return -1;
	}
	
	g_free(path);
	g_free(filename);
	
	pop3_sd_state(state, POP3_TOP_RECV, &next_state);
	
	if (state->cur_msg < state->count) {
		state->cur_msg++;
		return POP3_TOP_SEND;
	} else
		return POP3_LOGOUT_SEND;
}

gint pop3_retr_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	pop3_gen_send(sock, "RETR %d", state->cur_msg);

	return POP3_RETR_RECV;
}

gint pop3_retr_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar *file;
	gint ok, drop_ok;
	gint next_state;
	gint write_val;
	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		file = get_tmp_file();
		if ((write_val = recv_write_to_file(sock, file)) < 0) {
			g_free(file);
			if (!state->cancelled)
				state->error_val = 
					(write_val == -1 ? PS_IOERR : PS_SOCKET);
			return -1;
		}

		/* drop_ok: 0: success 1: don't receive -1: error */
		drop_ok = inc_drop_message(file, state);
		g_free(file);
		if (drop_ok < 0) {
			state->error_val = PS_ERROR;
			return -1;
		}

		if (pop3_sd_state(state, POP3_RETR_RECV, &next_state))
			return next_state;
	
		state->cur_total_bytes += state->msg[state->cur_msg].size;
		state->cur_total_recv_bytes += state->msg[state->cur_msg].size;
		state->cur_total_num++;

		state->msg[state->cur_msg].received = TRUE;
		state->msg[state->cur_msg].recv_time =
			drop_ok == 1 ? RECV_TIME_KEEP : state->current_time;

		if (drop_ok == 0 && state->ac_prefs->rmmail &&
		    state->ac_prefs->msg_leave_time == 0)
			return POP3_DELETE_SEND;

		if (state->cur_msg < state->count) {
			state->cur_msg++;
			LOOKUP_NEXT_MSG();
			return POP3_RETR_SEND;
		} else
			return POP3_LOGOUT_SEND;
	} else if (ok == PS_PROTOCOL) {
		state->error_val = ok;
		return POP3_LOGOUT_SEND;
	} else {
		state->error_val = ok;
		return -1;
	}
}

gint pop3_delete_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	pop3_gen_send(sock, "DELE %d", state->cur_msg);

	return POP3_DELETE_RECV;
}

gint pop3_delete_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gint next_state;
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		state->msg[state->cur_msg].deleted = TRUE;
		
		if (pop3_sd_state(state, POP3_DELETE_RECV, &next_state))
			return next_state;	

		if (state->cur_msg < state->count) {
			state->cur_msg++;
			LOOKUP_NEXT_MSG();
			return POP3_RETR_SEND;
		} else
			return POP3_LOGOUT_SEND;
	} else if (ok == PS_PROTOCOL) {
		state->error_val = ok;
		return POP3_LOGOUT_SEND;
	} else {
		state->error_val = ok;
		return -1;
	}
}

gint pop3_logout_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "QUIT");

	return POP3_LOGOUT_RECV;
}

gint pop3_logout_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) != PS_SUCCESS)
		state->error_val = ok;

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
	sock_write_all(sock, buf, strlen(buf));
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

static void pop3_sd_new_header(Pop3State *state)
{
	HeaderItems *new_msg;
	if (state->cur_msg <= state->count) {
		new_msg = g_new0(HeaderItems, 1); 
		
		new_msg->index              = state->cur_msg;
		new_msg->state              = SD_UNCHECKED;
		new_msg->size               = state->msg[state->cur_msg].size; 
		new_msg->received           = state->msg[state->cur_msg].received;
		new_msg->del_by_old_session = FALSE;
		
		state->ac_prefs->msg_list = g_slist_append(state->ac_prefs->msg_list, 
							   new_msg);
		debug_print("received ?: msg %i, received: %i\n",new_msg->index, new_msg->received); 
	}
}

gboolean pop3_sd_state(Pop3State *state, gint cur_state, guint *next_state) 
{
	gint session = state->ac_prefs->session;
	guint goto_state = -1;

	switch (cur_state) { 
	case POP3_GETRANGE_UIDL_RECV:
		switch (session) {
		case STYPE_POP_BEFORE_SMTP:
			goto_state = POP3_LOGOUT_SEND;
			break;
		case STYPE_DOWNLOAD:
		case STYPE_DELETE:
		case STYPE_PREVIEW_ALL:
			goto_state = POP3_GETSIZE_LIST_SEND;
		default:
			break;
		}
		break;
	case POP3_GETSIZE_LIST_RECV:
		switch (session) {
		case STYPE_PREVIEW_ALL:
			state->cur_msg = 1;
		case STYPE_PREVIEW_NEW:
			goto_state = POP3_TOP_SEND;
			break;
		case STYPE_DELETE:
			if (pop3_sd_get_next(state))
				goto_state = POP3_DELETE_SEND;		
			else
				goto_state = POP3_LOGOUT_SEND;
			break;
		case STYPE_DOWNLOAD:
			if (pop3_sd_get_next(state))
				goto_state = POP3_RETR_SEND;
			else
				goto_state = POP3_LOGOUT_SEND;
		default:
			break;
		}
		break;
	case POP3_TOP_RECV: 
		switch (session) { 
		case STYPE_PREVIEW_ALL:
		case STYPE_PREVIEW_NEW:
			pop3_sd_new_header(state);
		default:
			break;
		}
		break;
	case POP3_RETR_RECV:
		switch (session) {
		case STYPE_DOWNLOAD:
			if (state->ac_prefs->sd_rmmail_on_download) 
				goto_state = POP3_DELETE_SEND;
			else {
				if (pop3_sd_get_next(state)) 
					goto_state = POP3_RETR_SEND;
				else
					goto_state = POP3_LOGOUT_SEND;
			}
		default:	
			break;
		}
		break;
	case POP3_DELETE_RECV:
		switch (session) {
		case STYPE_DELETE:
			if (pop3_sd_get_next(state)) 
				goto_state = POP3_DELETE_SEND;
			else
				goto_state =  POP3_LOGOUT_SEND;
			break;
		case STYPE_DOWNLOAD:
			if (pop3_sd_get_next(state)) 
				goto_state = POP3_RETR_SEND;
			else
				goto_state = POP3_LOGOUT_SEND;
		default:
			break;
		}
	default:
		break;
		
	}		  

	*next_state = goto_state;
	if (goto_state != -1)
		return TRUE;
	else 
		return FALSE;
}

gboolean pop3_sd_get_next(Pop3State *state)
{
	GSList *cur;
	gint deleted_msgs = 0;
	
	switch (state->ac_prefs->session) {
	case STYPE_DOWNLOAD:
	case STYPE_DELETE: 	
		for (cur = state->ac_prefs->msg_list; cur != NULL; cur = cur->next) {
			HeaderItems *items = (HeaderItems*)cur->data;

			if (items->del_by_old_session)
				deleted_msgs++;

			switch (items->state) {
			case SD_REMOVE:
				items->state = SD_REMOVED;
				break;
			case SD_DOWNLOAD:
				items->state = SD_DOWNLOADED;
				break;
			case SD_CHECKED:
				state->cur_msg = items->index - deleted_msgs;
				if (state->ac_prefs->session == STYPE_DELETE)
					items->state = SD_REMOVE;
				else
					items->state = SD_DOWNLOAD;
				return TRUE;
			default:
				break;
			}
		}
		return FALSE;
	default:
		return FALSE;
	}
}

Pop3State *pop3_state_new(PrefsAccount *account)
{
	Pop3State *state;

	g_return_val_if_fail(account != NULL, NULL);

	state = g_new0(Pop3State, 1);

	state->ac_prefs = account;
	state->uidl_table = pop3_get_uidl_table(account);
	state->current_time = time(NULL);
	state->error_val = PS_SUCCESS;

	return state;
}

void pop3_state_destroy(Pop3State *state)
{
	gint n;

	g_return_if_fail(state != NULL);

	for (n = 1; n <= state->count; n++)
		g_free(state->msg[n].uidl);
	g_free(state->msg);

	if (state->uidl_table) {
		hash_free_strings(state->uidl_table);
		g_hash_table_destroy(state->uidl_table);
	}

	g_free(state->greeting);
	g_free(state->user);
	g_free(state->pass);
	g_free(state->prev_folder);

	g_free(state);
}

GHashTable *pop3_get_uidl_table(PrefsAccount *ac_prefs)
{
	GHashTable *table;
	gchar *path;
	FILE *fp;
	gchar buf[POPBUFSIZE];
	gchar uidl[POPBUFSIZE];
	time_t recv_time;
	time_t now;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S, ac_prefs->recv_server,
			   "-", ac_prefs->userid, NULL);
	if ((fp = fopen(path, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				   "uidl-", ac_prefs->recv_server,
				   "-", ac_prefs->userid, NULL);
		if ((fp = fopen(path, "rb")) == NULL) {
			if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
			g_free(path);
			return NULL;
		}
	}
	g_free(path);

	table = g_hash_table_new(g_str_hash, g_str_equal);

	now = time(NULL);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		recv_time = RECV_TIME_NONE;
		if (sscanf(buf, "%s\t%ld", uidl, &recv_time) != 2) {
			if (sscanf(buf, "%s", uidl) != 1)
				continue;
			else
				recv_time = now;
		}
		if (recv_time == RECV_TIME_NONE)
			recv_time = RECV_TIME_RECEIVED;
		g_hash_table_insert(table, g_strdup(uidl),
				    GINT_TO_POINTER(recv_time));
	}

	fclose(fp);
	return table;
}

gint pop3_write_uidl_list(Pop3State *state)
{
	gchar *path;
	FILE *fp;
	Pop3MsgInfo *msg;
	gint n;

	if (!state->uidl_is_valid) return 0;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S,
			   state->ac_prefs->recv_server,
			   "-", state->ac_prefs->userid, NULL);
	if ((fp = fopen(path, "wb")) == NULL) {
		FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return -1;
	}

	for (n = 1; n <= state->count; n++) {
		msg = &state->msg[n];
		if (msg->uidl && msg->received && !msg->deleted)
			fprintf(fp, "%s\t%ld\n", msg->uidl, msg->recv_time);
	}

	if (fclose(fp) == EOF) FILE_OP_ERROR(path, "fclose");
	g_free(path);

	return 0;
}
