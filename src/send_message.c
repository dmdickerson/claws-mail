/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkclist.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "intl.h"
#include "send_message.h"
#include "session.h"
#include "ssl.h"
#include "smtp.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "procheader.h"
#include "account.h"
#include "progressdialog.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"
#include "statusbar.h"
#include "inc.h"
#include "log.h"

typedef struct _SendProgressDialog	SendProgressDialog;

struct _SendProgressDialog
{
	ProgressDialog *dialog;
	Session *session;
	gboolean cancelled;
};
#if 0
static gint send_message_local		(const gchar		*command,
					 FILE			*fp);
static gint send_message_smtp		(PrefsAccount		*ac_prefs,
					 GSList			*to_list,
					 FILE			*fp);
#endif

static gint send_recv_message		(Session		*session,
					 const gchar		*msg,
					 gpointer		 data);
static gint send_send_data_progressive	(Session		*session,
					 guint			 cur_len,
					 guint			 total_len,
					 gpointer		 data);
static gint send_send_data_finished	(Session		*session,
					 guint			 len,
					 gpointer		 data);

static SendProgressDialog *send_progress_dialog_create	(void);
static void send_progress_dialog_destroy	(SendProgressDialog *dialog);

static void send_cancel_button_cb	(GtkWidget	*widget,
					 gpointer	 data);

gint send_message(const gchar *file, PrefsAccount *ac_prefs, GSList *to_list)
{
	FILE *fp;
	gint val;

	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(ac_prefs != NULL, -1);
	g_return_val_if_fail(to_list != NULL, -1);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	if (ac_prefs->use_mail_command && ac_prefs->mail_command &&
	    (*ac_prefs->mail_command)) {
		val = send_message_local(ac_prefs->mail_command, fp);
		fclose(fp);
		return val;
	}
	else if (prefs_common.use_extsend && prefs_common.extsend_cmd) {
		val = send_message_local(prefs_common.extsend_cmd, fp);
		fclose(fp);
		return val;
	}
	else {
		val = send_message_smtp(ac_prefs, to_list, fp);
		
		fclose(fp);
		return val;
	}
}

enum
{
	Q_SENDER     = 0,
	Q_SMTPSERVER = 1,
	Q_RECIPIENTS = 2,
	Q_ACCOUNT_ID = 3
};

#if 0
gint send_message_queue(const gchar *file)
{
	static HeaderEntry qentry[] = {{"S:",   NULL, FALSE},
				       {"SSV:", NULL, FALSE},
				       {"R:",   NULL, FALSE},
				       {"AID:", NULL, FALSE},
				       {NULL,   NULL, FALSE}};
	FILE *fp;
	gint val = 0;
	gchar *from = NULL;
	gchar *server = NULL;
	GSList *to_list = NULL;
	gchar buf[BUFFSIZE];
	gint hnum;
	glong fpos;
	PrefsAccount *ac = NULL, *mailac = NULL, *newsac = NULL;

	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, qentry))
	       != -1) {
		gchar *p;

		p = buf + strlen(qentry[hnum].name);

		switch (hnum) {
		case Q_SENDER:
			if (!from) from = g_strdup(p);
			break;
		case Q_SMTPSERVER:
			if (!server) server = g_strdup(p);
			break;
		case Q_RECIPIENTS:
			to_list = address_list_append(to_list, p);
			break;
		case Q_ACCOUNT_ID:
			ac = account_find_from_id(atoi(p));
			break;
		default:
			break;
		}
	}

	if (((!ac || (ac && ac->protocol != A_NNTP)) && !to_list) || !from) {
		g_warning("Queued message header is broken.\n");
		val = -1;
	} else if (prefs_common.use_extsend && prefs_common.extsend_cmd) {
		val = send_message_local(prefs_common.extsend_cmd, fp);
	} else {
		if (ac && ac->protocol == A_NNTP) {
			newsac = ac;

			/* search mail account */
			mailac = account_find_from_address(from);
			if (!mailac) {
				if (cur_account &&
				    cur_account->protocol != A_NNTP)
					mailac = cur_account;
				else {
					mailac = account_get_default();
					if (mailac->protocol == A_NNTP)
						mailac = NULL;
				}
			}
		} else if (ac) {
			mailac = ac;
		} else {
			ac = account_find_from_smtp_server(from, server);
			if (!ac) {
				g_warning("Account not found. "
					  "Using current account...\n");
				ac = cur_account;
				if (ac && ac->protocol != A_NNTP)
					mailac = ac;
			}
		}

		fpos = ftell(fp);
		if (to_list) {
			if (mailac)
				val = send_message_smtp(mailac, to_list, fp);
			else {
				PrefsAccount tmp_ac;

				g_warning("Account not found.\n");

				memset(&tmp_ac, 0, sizeof(PrefsAccount));
				tmp_ac.address = from;
				tmp_ac.smtp_server = server;
				tmp_ac.smtpport = SMTP_PORT;
				val = send_message_smtp(&tmp_ac, to_list, fp);
			}
		}

		if (val == 0 && newsac) {
			fseek(fp, fpos, SEEK_SET);
			val = news_post_stream(FOLDER(newsac->folder), fp);
		}
	}

	slist_free_strings(to_list);
	g_slist_free(to_list);
	g_free(from);
	g_free(server);
	fclose(fp);

	return val;
}
#endif

gint send_message_local(const gchar *command, FILE *fp)
{
	FILE *pipefp;
	gchar buf[BUFFSIZE];
	int r;
	sigset_t osig, mask;

	g_return_val_if_fail(command != NULL, -1);
	g_return_val_if_fail(fp != NULL, -1);

	pipefp = popen(command, "w");
	if (!pipefp) {
		g_warning("Can't execute external command: %s\n", command);
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		fputs(buf, pipefp);
		fputc('\n', pipefp);
	}

	/* we need to block SIGCHLD, otherwise pspell's handler will wait()
	 * the pipecommand away and pclose will return -1 because of its
	 * failed wait4().
	 */
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &osig);
	
	r = pclose(pipefp);

	sigprocmask(SIG_SETMASK, &osig, NULL);
	if (r != 0) {
		g_warning("external command `%s' failed with code `%i'\n", command, r);
		return -1;
	}

	return 0;
}

gint send_message_smtp(PrefsAccount *ac_prefs, GSList *to_list, FILE *fp)
{
	Session *session;
	SMTPSession *smtp_session;
	gushort port;
	SendProgressDialog *dialog;
	GtkCList *clist;
	const gchar *text[3];
	gchar buf[BUFFSIZE];
	gint ret = 0;

	g_return_val_if_fail(ac_prefs != NULL, -1);
	g_return_val_if_fail(ac_prefs->address != NULL, -1);
	g_return_val_if_fail(ac_prefs->smtp_server != NULL, -1);
	g_return_val_if_fail(to_list != NULL, -1);
	g_return_val_if_fail(fp != NULL, -1);

	session = smtp_session_new();
	smtp_session = SMTP_SESSION(session);

	smtp_session->hostname =
		ac_prefs->set_domain ? g_strdup(ac_prefs->domain) : NULL;

	if (ac_prefs->use_smtp_auth) {
		smtp_session->forced_auth_type = ac_prefs->smtp_auth_type;

		if (ac_prefs->smtp_userid) {
			smtp_session->user = g_strdup(ac_prefs->smtp_userid);
			if (ac_prefs->smtp_passwd)
				smtp_session->pass =
					g_strdup(ac_prefs->smtp_passwd);
			else if (ac_prefs->tmp_smtp_pass)
				smtp_session->pass =
					g_strdup(ac_prefs->tmp_smtp_pass);
			else {
				smtp_session->pass =
					input_dialog_query_password
						(ac_prefs->smtp_server,
						 smtp_session->user);
				if (!smtp_session->pass)
					smtp_session->pass = g_strdup("");
				ac_prefs->tmp_smtp_pass =
					g_strdup(smtp_session->pass);
			}
		} else {
			smtp_session->user = g_strdup(ac_prefs->userid);
			if (ac_prefs->passwd)
				smtp_session->pass = g_strdup(ac_prefs->passwd);
			else if (ac_prefs->tmp_pass)
				smtp_session->pass =
					g_strdup(ac_prefs->tmp_pass);
			else {
				smtp_session->pass =
					input_dialog_query_password
						(ac_prefs->smtp_server,
						 smtp_session->user);
				if (!smtp_session->pass)
					smtp_session->pass = g_strdup("");
				ac_prefs->tmp_pass =
					g_strdup(smtp_session->pass);
			}
		}
	} else {
		smtp_session->user = NULL;
		smtp_session->pass = NULL;
	}

	smtp_session->from = g_strdup(ac_prefs->address);
	smtp_session->to_list = to_list;
	smtp_session->cur_to = to_list;
	smtp_session->send_data = get_outgoing_rfc2822_str(fp);
	smtp_session->send_data_len = strlen(smtp_session->send_data);

#if USE_OPENSSL
	port = ac_prefs->set_smtpport ? ac_prefs->smtpport :
		ac_prefs->ssl_smtp == SSL_TUNNEL ? SSMTP_PORT : SMTP_PORT;
	session->ssl_type = ac_prefs->ssl_smtp;
#else
	port = ac_prefs->set_smtpport ? ac_prefs->smtpport : SMTP_PORT;
#endif

	dialog = send_progress_dialog_create();
	dialog->session = session;

	text[0] = NULL;
	text[1] = ac_prefs->smtp_server;
	text[2] = _("Connecting");
	clist = GTK_CLIST(dialog->dialog->clist);
	gtk_clist_append(clist, (gchar **)text);

	if (ac_prefs->pop_before_smtp
	    && (ac_prefs->protocol == A_APOP || ac_prefs->protocol == A_POP3)
	    && (time(NULL) - ac_prefs->last_pop_login_time) > (60 * ac_prefs->pop_before_smtp_timeout)) {
		g_snprintf(buf, sizeof(buf), _("Doing POP before SMTP..."));
		log_message(buf);
		progress_dialog_set_label(dialog->dialog, buf);
		gtk_clist_set_text(clist, 0, 2, _("POP before SMTP"));
		GTK_EVENTS_FLUSH();
		inc_pop_before_smtp(ac_prefs);
	}
	
	g_snprintf(buf, sizeof(buf), _("Connecting to SMTP server: %s ..."),
		   ac_prefs->smtp_server);
	progress_dialog_set_label(dialog->dialog, buf);
	log_message("%s\n", buf);

	session_set_recv_message_notify(session, send_recv_message, dialog);
	session_set_send_data_progressive_notify
		(session, send_send_data_progressive, dialog);
	session_set_send_data_notify(session, send_send_data_finished, dialog);

	if (session_connect(session, ac_prefs->smtp_server, port) < 0) {
		session_destroy(session);
		send_progress_dialog_destroy(dialog);
		return -1;
	}

	g_print("parent: begin event loop\n");

	while (session->state != SESSION_DISCONNECTED &&
	       session->state != SESSION_ERROR)
		gtk_main_iteration();

	if (SMTP_SESSION(session)->error_val == SM_AUTHFAIL) {
		if (ac_prefs->smtp_userid && ac_prefs->tmp_smtp_pass) {
			g_free(ac_prefs->tmp_smtp_pass);
			ac_prefs->tmp_smtp_pass = NULL;
		}
		ret = -1;
	} else if (session->state == SESSION_ERROR ||
		   SMTP_SESSION(session)->state == SMTP_ERROR ||
		   SMTP_SESSION(session)->error_val != SM_OK)
		ret = -1;
	else if (dialog->cancelled == TRUE)
		ret = -1;

	session_destroy(session);
	send_progress_dialog_destroy(dialog);

	statusbar_verbosity_set(FALSE);
	return ret;
}

static gint send_recv_message(Session *session, const gchar *msg, gpointer data)
{
	SMTPSession *smtp_session;
	SendProgressDialog *dialog; 
	gchar buf[BUFFSIZE];
	gchar *state_str;

	dialog = (SendProgressDialog *) data;
	state_str = NULL;
	smtp_session = SMTP_SESSION(session);

	switch (smtp_session->state) {
	case SMTP_READY:
	case SMTP_CONNECTED:
		return 0;
	case SMTP_HELO:
		g_snprintf(buf, sizeof(buf), _("Sending HELO..."));
		state_str = _("Authenticating");
		break;
	case SMTP_EHLO:
		g_snprintf(buf, sizeof(buf), _("Sending EHLO..."));
		state_str = _("Authenticating");
		break;
	case SMTP_AUTH:
		g_snprintf(buf, sizeof(buf), _("Authenticating..."));
		state_str = _("Authenticating");
		break;
	case SMTP_FROM:
		g_snprintf(buf, sizeof(buf), _("Sending MAIL FROM..."));
		state_str = _("Sending");
		break;
	case SMTP_RCPT:
		g_snprintf(buf, sizeof(buf), _("Sending RCPT TO..."));
		state_str = _("Sending");
		break;
	case SMTP_DATA:
	case SMTP_EOM:
		g_snprintf(buf, sizeof(buf), _("Sending DATA..."));
		state_str = _("Sending");
		break;
	case SMTP_QUIT:
		g_snprintf(buf, sizeof(buf), _("Quitting..."));
		state_str = _("Quitting");
		break;
	case SMTP_ERROR:
	case SMTP_AUTH_FAILED:
		g_warning("send: error: %s\n", msg);
		return 0;
	default:
		return 0;
	}

	progress_dialog_set_label(dialog->dialog, buf);
	gtk_clist_set_text(GTK_CLIST(dialog->dialog->clist), 0, 2, state_str);

	return 0;
}

static gint send_send_data_progressive(Session *session, guint cur_len,
				       guint total_len, gpointer data)
{
	SendProgressDialog *dialog = (SendProgressDialog *)data;
	gchar buf[BUFFSIZE];

	g_snprintf(buf, sizeof(buf), _("Sending message (%d / %d bytes)"),
		   cur_len, total_len);
	progress_dialog_set_label(dialog->dialog, buf);
	progress_dialog_set_percentage
		(dialog->dialog, (gfloat)cur_len / (gfloat)total_len);
	return 0;
}

static gint send_send_data_finished(Session *session, guint len, gpointer data)
{
	SendProgressDialog *dialog = (SendProgressDialog *)data;
	gchar buf[BUFFSIZE];
	
	g_snprintf(buf, sizeof(buf), _("Sending message (%d / %d bytes)"),
		   len, len);
	progress_dialog_set_label(dialog->dialog, buf);
	progress_dialog_set_percentage
		(dialog->dialog, (gfloat)len / (gfloat)len);
	return 0;
}

static SendProgressDialog *send_progress_dialog_create(void)
{
	SendProgressDialog *dialog;
	ProgressDialog *progress;

	dialog = g_new0(SendProgressDialog, 1);

	progress = progress_dialog_create();
	gtk_window_set_title(GTK_WINDOW(progress->window),
			     _("Sending message"));
	gtk_signal_connect(GTK_OBJECT(progress->cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(send_cancel_button_cb), dialog);
	gtk_signal_connect(GTK_OBJECT(progress->window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_true), NULL);
	gtk_window_set_modal(GTK_WINDOW(progress->window), TRUE);
	manage_window_set_transient(GTK_WINDOW(progress->window));

	progress_dialog_set_value(progress, 0.0);

	if (prefs_common.send_dialog_mode == SEND_DIALOG_ALWAYS) {
		gtk_widget_show_now(progress->window);
	}
	
	dialog->dialog = progress;

	return dialog;
}

static void send_progress_dialog_destroy(SendProgressDialog *dialog)
{
	g_return_if_fail(dialog != NULL);
	if (prefs_common.send_dialog_mode == SEND_DIALOG_ALWAYS) {
		progress_dialog_destroy(dialog->dialog);
	}
	g_free(dialog);
}

static void send_cancel_button_cb(GtkWidget *widget, gpointer data)
{
	SendProgressDialog *dialog = (SendProgressDialog *)data;
	Session *session = dialog->session;

	session->state = SESSION_DISCONNECTED;
	dialog->cancelled = TRUE;
}
