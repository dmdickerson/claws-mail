/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "utils.h"
#include "gtk/menu.h"
#include "account.h"
#include "prefs_account.h"
#include "mainwindow.h"
#include "stock_pixmap.h"
#include "setup.h"
#include "folder.h"
#include "alertpanel.h"
#ifdef USE_OPENSSL			
#include "ssl.h"
#endif
#include "prefs_common.h"

typedef enum
{
	GO_BACK,
	GO_FORWARD,
	CANCEL,
	FINISHED
} PageNavigation;

int WELCOME_PAGE = -1;
int USER_PAGE = -1;
int SMTP_PAGE = -1;
int RECV_PAGE = -1;
int MAILBOX_PAGE = -1;
int SSL_PAGE = -1;
int DONE_PAGE = -1;

typedef struct
{
	GtkWidget *window;
	GSList    *pages;
	GtkWidget *notebook;

	MainWindow *mainwin;
	
	GtkWidget *email;
	GtkWidget *full_name;
	GtkWidget *organization;

	GtkWidget *mailbox_name;
	
	GtkWidget *smtp_server;

	GtkWidget *recv_type;
	GtkWidget *recv_label;
	GtkWidget *recv_server;
	GtkWidget *recv_username;
	GtkWidget *recv_password;
	GtkWidget *recv_username_label;
	GtkWidget *recv_password_label;
	GtkWidget *recv_imap_label;
	GtkWidget *recv_imap_subdir;

#ifdef USE_OPENSSL
	GtkWidget *smtp_use_ssl;
	GtkWidget *recv_use_ssl;
#endif
	
	gboolean create_mailbox;
	gboolean finished;
	gboolean result;

} WizardWindow;

static void initialize_fonts(WizardWindow *wizard)
{
	GtkWidget *widget = wizard->email;
	gint size = pango_font_description_get_size(
			widget->style->font_desc)
		      /PANGO_SCALE;
	gchar *tmp, *new;
	
	tmp = g_strdup(prefs_common.textfont);
	if (strrchr(tmp, ' ')) {
		*(strrchr(tmp, ' ')) = '\0';
		new = g_strdup_printf("%s %d", tmp, size);
		g_free(prefs_common.textfont);
		prefs_common.textfont = new;
	}
	g_free(tmp);
	
	tmp = g_strdup(prefs_common.smallfont);
	if (strrchr(tmp, ' ')) {
		*(strrchr(tmp, ' ')) = '\0';
		new = g_strdup_printf("%s %d", tmp, size);
		g_free(prefs_common.smallfont);
		prefs_common.smallfont = new;
	}
	g_free(tmp);
	
	tmp = g_strdup(prefs_common.normalfont);
	if (strrchr(tmp, ' ')) {
		*(strrchr(tmp, ' ')) = '\0';
		new = g_strdup_printf("%s %d", tmp, size);
		g_free(prefs_common.normalfont);
		prefs_common.normalfont = new;
	}
	g_free(tmp);
}

#define XFACE "+}Axz@~a,-Yx?0Ysa|q}CLRH=89Y]\"')DSX^<6p\"d)'81yx5%G#u^o*7JG&[aPU0h1Ux.vb2yIjH83{5`/bVo|~nn/i83vE^E)qk-4W)_E.4Y=D*qvf/,Ci_=P<iY<M6"

static void write_welcome_email(WizardWindow *wizard)
{
	gchar buf_date[64];
	gchar *head=NULL;
	gchar *body=NULL;
	gchar *msg=NULL;
	gchar *subj=NULL;
	const gchar *mailbox = gtk_entry_get_text(GTK_ENTRY(wizard->mailbox_name));
	Folder *folder = folder_find_from_path(mailbox);
	FolderItem *inbox = folder ? folder->inbox:NULL;
	gchar *file = get_tmp_file();
	
	get_rfc822_date(buf_date, sizeof(buf_date));

	subj = g_strdup_printf(_("Welcome to Sylpheed-Claws"));

	head = g_strdup_printf(
		"From: %s <sylpheed-claws-users@lists.sf.net>\n"
		"To: %s <%s>\n"
		"Date: %s\n"
		"Subject: %s\n"
		"X-Face: %s\n"
		"Content-Type: text/plain; charset=UTF-8\n",
		_("Sylpheed-Claws Team"),
		gtk_entry_get_text(GTK_ENTRY(wizard->full_name)),
		gtk_entry_get_text(GTK_ENTRY(wizard->email)),
		buf_date, subj, XFACE);
	body = g_strdup_printf(
		_("\n"
		"Welcome to Sylpheed-Claws\n"
		"-------------------------\n"
		"\n"
		"Now that you have set up your account you can fetch your\n"
		"mail by clicking the 'Get Mail' button at the left of the\n"
		"toolbar.\n"
		"\n"
		"You can change your Account Preferences by using the menu\n"
		"entry '/Configuration/Preferences for current account'\n"
		"and change the general Preferences by using\n"
		"'/Configuration/Preferences'.\n"
		"\n"
		"You can find further information in the Sylpheed-Claws manual,\n"
		"which can be accessed by using the menu entry '/Help/Manual'\n"
		"or online at the URL given below.\n"
		"\n"
		"Useful URLs\n"
		"-----------\n"
		"Homepage:      <%s>\n"
		"Manual:        <%s>\n"
		"FAQ:	       <%s>\n"
		"Themes:        <%s>\n"
		"Mailing Lists: <%s>\n"
		"\n"
		"LICENSE\n"
		"-------\n"
		"Sylpheed-Claws is free software, released under the terms\n"
		"of the GNU General Public License, version 2 or later, as\n"
		"published by the Free Software Foundation, 51 Franklin Street,\n"
		"Fifth Floor, Boston, MA 02110-1301, USA. The license can be\n"
		"found at <%s>.\n"
		"\n"
		"DONATIONS\n"
		"---------\n"
		"If you wish to donate to the Sylpheed-Claws project you can do\n"
		"so at <%s>.\n\n"),
		HOMEPAGE_URI, MANUAL_URI, FAQ_URI, THEMES_URI, MAILING_LIST_URI,
		GPL_URI, DONATE_URI);
	
	msg = g_strconcat(head, body, NULL);

	if (inbox && inbox->total_msgs == 0
	 && str_write_to_file(msg, file) >= 0) {
		MsgFlags flags = { MSG_UNREAD|MSG_NEW, 0};
		folder_item_add_msg(inbox, file, &flags, FALSE);
	}
	g_free(subj);
	g_free(head);
	g_free(body);
	g_free(msg);
	g_unlink(file);
}
#undef XFACE

static gboolean wizard_write_config(WizardWindow *wizard)
{
	static gboolean mailbox_ok = FALSE;
	PrefsAccount *prefs_account = prefs_account_new();
	GList *account_list = NULL;
	GtkWidget *menu, *menuitem;
	
	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(wizard->recv_type));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	prefs_account->protocol = GPOINTER_TO_INT
			(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	
	
	if (wizard->create_mailbox && prefs_account->protocol != A_IMAP4 && 
	    !strlen(gtk_entry_get_text(GTK_ENTRY(wizard->mailbox_name)))) {
		alertpanel_error(_("Please enter the mailbox name."));
		g_free(prefs_account);
		gtk_notebook_set_current_page (
			GTK_NOTEBOOK(wizard->notebook), 
			MAILBOX_PAGE);
		return FALSE;
	}

	if (!mailbox_ok) {
		if (wizard->create_mailbox && prefs_account->protocol != A_IMAP4) {
			mailbox_ok = setup_write_mailbox_path(wizard->mainwin, 
					gtk_entry_get_text(
						GTK_ENTRY(wizard->mailbox_name)));
		} else
			mailbox_ok = TRUE;
	}

	if (!mailbox_ok) {
		/* alertpanel done by setup_write_mailbox_path */
		g_free(prefs_account);
		gtk_notebook_set_current_page (
			GTK_NOTEBOOK(wizard->notebook), 
			MAILBOX_PAGE);
		return FALSE;
	}
	
	if (!strlen(gtk_entry_get_text(GTK_ENTRY(wizard->full_name)))
	||  !strlen(gtk_entry_get_text(GTK_ENTRY(wizard->email)))) {
		alertpanel_error(_("Please enter your name and email address."));
		g_free(prefs_account);
		gtk_notebook_set_current_page (
			GTK_NOTEBOOK(wizard->notebook), 
			USER_PAGE);
		return FALSE;
	}
	
	if (prefs_account->protocol != A_LOCAL) {
		if (!strlen(gtk_entry_get_text(GTK_ENTRY(wizard->recv_username)))
		||  !strlen(gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)))) {
			alertpanel_error(_("Please enter your receiving server "
					   "and username."));
			g_free(prefs_account);
			gtk_notebook_set_current_page (
				GTK_NOTEBOOK(wizard->notebook), 
				RECV_PAGE);
			return FALSE;
		}
	} else {
		if (!strlen(gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)))) {
			alertpanel_error(_("Please enter your username."));
			g_free(prefs_account);
			gtk_notebook_set_current_page (
				GTK_NOTEBOOK(wizard->notebook), 
				RECV_PAGE);
			return FALSE;
		}
	}

	if (!strlen(gtk_entry_get_text(GTK_ENTRY(wizard->smtp_server)))) {
		alertpanel_error(_("Please enter your SMTP server."));
		g_free(prefs_account);
		gtk_notebook_set_current_page (
			GTK_NOTEBOOK(wizard->notebook), 
			SMTP_PAGE);
		return FALSE;

	}

	if (prefs_account->protocol != A_LOCAL)
		prefs_account->account_name = g_strdup_printf("%s@%s",
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_username)),
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)));
	else
		prefs_account->account_name = g_strdup_printf("%s",
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)));

	prefs_account->name = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->full_name)));
	prefs_account->address = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->email)));
	prefs_account->organization = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->organization)));
	prefs_account->smtp_server = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->smtp_server)));

	if (prefs_account->protocol != A_LOCAL)
		prefs_account->recv_server = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)));
	else
		prefs_account->local_mbox = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)));

	prefs_account->userid = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_username)));
	prefs_account->passwd = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_password)));

#ifdef USE_OPENSSL			
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wizard->smtp_use_ssl)))
		prefs_account->ssl_smtp = SSL_TUNNEL;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wizard->recv_use_ssl))) {
		if (prefs_account->protocol == A_IMAP4)
			prefs_account->ssl_imap = SSL_TUNNEL;
		else
			prefs_account->ssl_pop = SSL_TUNNEL;
	}
#endif
	if (prefs_account->protocol == A_IMAP4) {
		gchar *directory = gtk_editable_get_chars(
			GTK_EDITABLE(wizard->recv_imap_subdir), 0, -1);
		if (directory && strlen(directory)) {
			prefs_account->imap_dir = g_strdup(directory);
		}
		g_free(directory);
	}

	account_list = g_list_append(account_list, prefs_account);
	prefs_account_write_config_all(account_list);
	prefs_account_free(prefs_account);
	account_read_config_all();

	initialize_fonts(wizard);
	if (wizard->create_mailbox && prefs_account->protocol != A_IMAP4)
		write_welcome_email(wizard);
	
	return TRUE;
}

static GtkWidget* create_page (WizardWindow *wizard, const char * title)
{
	GtkWidget *w;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	char *title_string;

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width  (GTK_CONTAINER(vbox), 10);

	/* create the titlebar */
	hbox = gtk_hbox_new (FALSE, 12);
	image = stock_pixmap_widget(wizard->window, 
			  	STOCK_PIXMAP_SYLPHEED_ICON);
	gtk_box_pack_start (GTK_BOX(hbox), image, FALSE, FALSE, 0);
     	title_string = g_strconcat ("<span size=\"xx-large\" weight=\"ultrabold\">", title ? title : "", "</span>", NULL);
	w = gtk_label_new (title_string);
	gtk_label_set_use_markup (GTK_LABEL(w), TRUE);
	g_free (title_string);
	gtk_box_pack_start (GTK_BOX(hbox), w, FALSE, FALSE, 0);

	/* pack the titlebar */
	gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* pack the separator */
	gtk_box_pack_start (GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

	/* pack space */
	w = gtk_alignment_new (0, 0, 0, 0);
	gtk_widget_set_size_request (w, 0, 6);
	gtk_box_pack_start (GTK_BOX(vbox), w, FALSE, FALSE, 0);

	return vbox;
}

#define GTK_TABLE_ADD_ROW_AT(table,text,entry,i) {			      \
	GtkWidget *label = gtk_label_new(text);				      \
	gtk_table_attach(GTK_TABLE(table), label, 			      \
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      \
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);		      \
	if (GTK_IS_MISC(label))						      \
		gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);	      \
	gtk_table_attach(GTK_TABLE(table), entry, 			      \
			 1,2,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      \
}

static gchar *get_default_email_addr(void)
{
	gchar *domain_name = g_strdup(get_domain_name());
	gchar *result;
	if (strchr(domain_name, '.') != strrchr(domain_name, '.')
	&& strlen(strchr(domain_name, '.')) > 6) {
		gchar *tmp = g_strdup(strchr(domain_name, '.')+1);
		g_free(domain_name);
		domain_name = tmp;
	}
	result = g_strdup_printf("%s@%s",
				g_get_user_name(),
				domain_name);
	g_free(domain_name);
	return result;
}

static gchar *get_default_server(WizardWindow * wizard, const gchar *type)
{
	gchar *domain_name = NULL;
	gchar *tmp = gtk_editable_get_chars(
			GTK_EDITABLE(wizard->email), 0, -1);
	gchar *result;
	
	if (strstr(tmp, "@")) {
		domain_name = g_strdup(strstr(tmp,"@")+1);
	} else {
		domain_name = g_strdup(get_domain_name());
	}
	
	g_free(tmp);

	result = g_strdup_printf("%s.%s",
				type, domain_name);
	g_free(domain_name);
	return result;
}

static gchar *get_default_account(WizardWindow * wizard)
{
	gchar *result = gtk_editable_get_chars(
			GTK_EDITABLE(wizard->email), 0, -1);
	
	if (strstr(result, "@")) {
		*(strstr(result,"@")) = '\0';
	} 

	return result;
}

static void wizard_email_changed(GtkWidget *widget, gpointer data)
{
	WizardWindow *wizard = (WizardWindow *)data;
	RecvProtocol protocol;
	gchar *text;
	protocol = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(wizard->recv_type), MENU_VAL_ID));
	
	text = get_default_server(wizard, "smtp");
	gtk_entry_set_text(GTK_ENTRY(wizard->smtp_server), text);
	g_free(text);

	text = get_default_account(wizard);
	gtk_entry_set_text(GTK_ENTRY(wizard->recv_username), text);
	g_free(text);

	if (protocol == A_POP3) {
		text = get_default_server(wizard, "pop");
		gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), text);
		g_free(text);
	} else if (protocol == A_IMAP4) {
		text = get_default_server(wizard, "imap");
		gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), text);
		g_free(text);
	} else if (protocol == A_LOCAL) {
		gchar *mbox = g_strdup_printf("/var/mail/%s", g_get_user_name());
		gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), mbox);
		g_free(mbox);
	}
	
}

static GtkWidget* user_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(3,2, FALSE);
	gchar *text;
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->full_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(wizard->full_name), g_get_real_name());
	GTK_TABLE_ADD_ROW_AT(table, _("<span weight=\"bold\">Your name:</span>"), 
			     wizard->full_name, i); i++;
	
	wizard->email = gtk_entry_new();
	text = get_default_email_addr();
	gtk_entry_set_text(GTK_ENTRY(wizard->email), text);
	g_free(text);
	GTK_TABLE_ADD_ROW_AT(table, _("<span weight=\"bold\">Your email address:</span>"), 
			     wizard->email, i); i++;
	
	wizard->organization = gtk_entry_new();
	GTK_TABLE_ADD_ROW_AT(table, _("Your organization:"),
			     wizard->organization, i); i++;
	
	g_signal_connect(G_OBJECT(wizard->email), "changed",
			 G_CALLBACK(wizard_email_changed),
			 wizard);
	return table;
}

static GtkWidget* mailbox_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(1,2, FALSE);
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->mailbox_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(wizard->mailbox_name), "Mail");
	GTK_TABLE_ADD_ROW_AT(table, _("<span weight=\"bold\">Mailbox name:</span>"), 
			     wizard->mailbox_name, i); i++;
	
	return table;
}

static GtkWidget* smtp_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(1,2, FALSE);
	gchar *text;
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->smtp_server = gtk_entry_new();
	text = get_default_server(wizard, "smtp");
	gtk_entry_set_text(GTK_ENTRY(wizard->smtp_server), text);
	g_free(text);
	GTK_TABLE_ADD_ROW_AT(table, _("<span weight=\"bold\">SMTP server address:</span>"), 
			     wizard->smtp_server, i); i++;
	return table;
}

static void wizard_protocol_changed(GtkMenuItem *menuitem, gpointer data)
{
	WizardWindow *wizard = (WizardWindow *)data;
	RecvProtocol protocol;
	gchar *text;
	protocol = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	
	if (protocol == A_POP3) {
		text = get_default_server(wizard, "pop");
		gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), text);
		gtk_widget_hide(wizard->recv_imap_label);
		gtk_widget_hide(wizard->recv_imap_subdir);
		gtk_widget_show(wizard->recv_username);
		gtk_widget_show(wizard->recv_password);
		gtk_widget_show(wizard->recv_username_label);
		gtk_widget_show(wizard->recv_password_label);
		gtk_label_set_text(GTK_LABEL(wizard->recv_label), _("<span weight=\"bold\">Server address:</span>"));
		gtk_label_set_use_markup(GTK_LABEL(wizard->recv_label), TRUE);
		g_free(text);
	} else if (protocol == A_IMAP4) {
		text = get_default_server(wizard, "imap");
		gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), text);
		gtk_widget_show(wizard->recv_imap_label);
		gtk_widget_show(wizard->recv_imap_subdir);
		gtk_widget_show(wizard->recv_username);
		gtk_widget_show(wizard->recv_password);
		gtk_widget_show(wizard->recv_username_label);
		gtk_widget_show(wizard->recv_password_label);
		gtk_label_set_text(GTK_LABEL(wizard->recv_label), _("<span weight=\"bold\">Server address:</span>"));
		gtk_label_set_use_markup(GTK_LABEL(wizard->recv_label), TRUE);
		g_free(text);
	} else if (protocol == A_LOCAL) {
		gchar *mbox = g_strdup_printf("/var/mail/%s", g_get_user_name());
		gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), mbox);
		g_free(mbox);
		gtk_label_set_text(GTK_LABEL(wizard->recv_label), _("<span weight=\"bold\">Local mailbox:</span>"));
		gtk_label_set_use_markup(GTK_LABEL(wizard->recv_label), TRUE);
		gtk_widget_hide(wizard->recv_imap_label);
		gtk_widget_hide(wizard->recv_imap_subdir);
		gtk_widget_hide(wizard->recv_username);
		gtk_widget_hide(wizard->recv_password);
		gtk_widget_hide(wizard->recv_username_label);
		gtk_widget_hide(wizard->recv_password_label);
	}
}

static GtkWidget* recv_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(5,2, FALSE);
	GtkWidget *menu = gtk_menu_new();
	GtkWidget *menuitem;
	gchar *text;
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->recv_type = gtk_option_menu_new();
	
	MENUITEM_ADD (menu, menuitem, _("POP3"), A_POP3);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(wizard_protocol_changed),
			 wizard);
#ifdef HAVE_LIBETPAN
	MENUITEM_ADD (menu, menuitem, _("IMAP"), A_IMAP4);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(wizard_protocol_changed),
			 wizard);
#endif
	MENUITEM_ADD (menu, menuitem, _("Local mbox file"), A_LOCAL);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(wizard_protocol_changed),
			 wizard);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (wizard->recv_type), menu);
	GTK_TABLE_ADD_ROW_AT(table, _("<span weight=\"bold\">Server type:</span>"), 
			     wizard->recv_type, i); i++;

	wizard->recv_server = gtk_entry_new();
	text = get_default_server(wizard, "pop");
	gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), text);
	g_free(text);
	
	wizard->recv_label = gtk_label_new(_("<span weight=\"bold\">Server address:</span>"));
	gtk_label_set_use_markup(GTK_LABEL(wizard->recv_label), TRUE);
	gtk_table_attach(GTK_TABLE(table), wizard->recv_label, 			      
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      
	if (GTK_IS_MISC(wizard->recv_label))						      
		gtk_misc_set_alignment(GTK_MISC(wizard->recv_label), 1, 0.5);	      
	gtk_table_attach(GTK_TABLE(table), wizard->recv_server,	      
			 1,2,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      
	i++;
	
	wizard->recv_username = gtk_entry_new();
	wizard->recv_username_label = gtk_label_new(_("<span weight=\"bold\">Username:</span>"));
	gtk_label_set_use_markup(GTK_LABEL(wizard->recv_username_label), TRUE);
	gtk_table_attach(GTK_TABLE(table), wizard->recv_username_label, 			      
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      
	if (GTK_IS_MISC(wizard->recv_username_label))						      
		gtk_misc_set_alignment(GTK_MISC(wizard->recv_username_label), 1, 0.5);	      
	gtk_table_attach(GTK_TABLE(table), wizard->recv_username,	      
			 1,2,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      
	i++;
	
	text = get_default_account(wizard);
	gtk_entry_set_text(GTK_ENTRY(wizard->recv_username), text);
	g_free(text);

	wizard->recv_password = gtk_entry_new();
	wizard->recv_password_label = gtk_label_new(_("Password:"));
	gtk_table_attach(GTK_TABLE(table), wizard->recv_password_label, 			      
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      
	if (GTK_IS_MISC(wizard->recv_password_label))						      
		gtk_misc_set_alignment(GTK_MISC(wizard->recv_password_label), 1, 0.5);	      
	gtk_table_attach(GTK_TABLE(table), wizard->recv_password,	      
			 1,2,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      
	gtk_entry_set_visibility(GTK_ENTRY(wizard->recv_password), FALSE);
	i++;
	
	wizard->recv_imap_subdir = gtk_entry_new();
	wizard->recv_imap_label = gtk_label_new(_("IMAP server directory:"));
	
	gtk_table_attach(GTK_TABLE(table), wizard->recv_imap_label, 			      
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      
	if (GTK_IS_MISC(wizard->recv_imap_label))						      
		gtk_misc_set_alignment(GTK_MISC(wizard->recv_imap_label), 1, 0.5);	      
	gtk_table_attach(GTK_TABLE(table), wizard->recv_imap_subdir,	      
			 1,2,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      

	i++;
	
	return table;
}

#ifdef USE_OPENSSL
static GtkWidget* ssl_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(2,2, FALSE);
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->smtp_use_ssl = gtk_check_button_new_with_label(
					_("Use SSL to connect to SMTP server"));
	gtk_table_attach(GTK_TABLE(table), wizard->smtp_use_ssl,      
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0); i++;
	
	wizard->recv_use_ssl = gtk_check_button_new_with_label(
					_("Use SSL to connect to receiving server"));
	gtk_table_attach(GTK_TABLE(table), wizard->recv_use_ssl,      
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	
	return table;
}
#endif

static void
wizard_response_cb (GtkDialog * dialog, int response, gpointer data)
{
	WizardWindow * wizard = (WizardWindow *)data;
	int current_page, num_pages;
	GtkWidget *menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(wizard->recv_type));
	GtkWidget *menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gint protocol = GPOINTER_TO_INT
			(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	gboolean skip_mailbox_page = FALSE;
	
	if (protocol == A_IMAP4) {
		skip_mailbox_page = TRUE;
	}
	
	num_pages = g_slist_length(wizard->pages);

 	current_page = gtk_notebook_get_current_page (
				GTK_NOTEBOOK(wizard->notebook));
	if (response == CANCEL)
	{
		wizard->result = FALSE;
		wizard->finished = TRUE;
		gtk_widget_destroy (GTK_WIDGET(dialog));
	}
	else if (response == FINISHED)
	{
		if (!wizard_write_config(wizard)) {
 			current_page = gtk_notebook_get_current_page (
					GTK_NOTEBOOK(wizard->notebook));
			goto set_sens;
		}
		wizard->result = TRUE;
		wizard->finished = TRUE;
		gtk_widget_destroy (GTK_WIDGET(dialog));
	}
	else
	{
		if (response == GO_BACK)
		{
			if (current_page > 0) {
				current_page--;
				if (current_page == MAILBOX_PAGE && skip_mailbox_page) {
					/* mailbox */
					current_page--;
				}
				gtk_notebook_set_current_page (
					GTK_NOTEBOOK(wizard->notebook), 
					current_page);
			}
		}
		else if (response == GO_FORWARD)
		{
			if (current_page < (num_pages-1)) {
				current_page++;
				if (current_page == MAILBOX_PAGE && skip_mailbox_page) {
					/* mailbox */
					current_page++;
				}
				gtk_notebook_set_current_page (
					GTK_NOTEBOOK(wizard->notebook), 
					current_page);
			}
		}
set_sens:
		gtk_dialog_set_response_sensitive (dialog, GO_BACK, 
				current_page > 0);
		gtk_dialog_set_response_sensitive (dialog, GO_FORWARD, 
				current_page < (num_pages - 1));
		gtk_dialog_set_response_sensitive (dialog, FINISHED, 
				current_page == (num_pages - 1));
	}
}

static gint wizard_close_cb(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	WizardWindow *wizard = (WizardWindow *)data;
	wizard->result = FALSE;
	wizard->finished = TRUE;
	
	return FALSE;
}

#define PACK_WARNING(text) {						\
	label = gtk_label_new(text);					\
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);			\
	gtk_box_pack_end(GTK_BOX(widget), label, FALSE, FALSE, 0);	\
}

gboolean run_wizard(MainWindow *mainwin, gboolean create_mailbox) {
	WizardWindow *wizard = g_new0(WizardWindow, 1);
	GtkWidget *page;
	GtkWidget *widget;
	GtkWidget *label;
	gchar     *text;
	GSList    *cur;
	gboolean   result;
	gint i = 0;
	wizard->mainwin = mainwin;
	wizard->create_mailbox = create_mailbox;
	
	gtk_widget_hide(mainwin->window);
	
	wizard->window = gtk_dialog_new_with_buttons (_("Sylpheed-Claws Setup Wizard"),
			NULL, 0, 
			GTK_STOCK_GO_BACK, GO_BACK,
			GTK_STOCK_GO_FORWARD, GO_FORWARD,
			GTK_STOCK_SAVE, FINISHED,
			GTK_STOCK_CANCEL, CANCEL,
			NULL);

	g_signal_connect(wizard->window, "response", 
			  G_CALLBACK(wizard_response_cb), wizard);
	gtk_widget_realize(wizard->window);
	gtk_dialog_set_default_response(GTK_DIALOG(wizard->window), 
			GO_FORWARD);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			GO_BACK, FALSE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			GO_FORWARD, TRUE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			FINISHED, FALSE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			CANCEL, TRUE);
	
	wizard->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(wizard->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(wizard->notebook), FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wizard->window)->vbox), 
			    wizard->notebook, TRUE, TRUE, 0);
	
	wizard->pages = NULL;
	
/*welcome page: 0 */
	WELCOME_PAGE = i;
	page = create_page(wizard, _("Welcome to Sylpheed-Claws"));
	
	wizard->pages = g_slist_append(wizard->pages, page);
	widget = stock_pixmap_widget(wizard->window, 
			  	STOCK_PIXMAP_SYLPHEED_LOGO);

	gtk_box_pack_start (GTK_BOX(page), widget, FALSE, FALSE, 0);
	
	text = g_strdup(_("Welcome to the Sylpheed-Claws setup wizard.\n\n"
			  "We will begin by defining some basic "
			  "information about you and your most common "
			  "mail options so that you can start to use "
			  "Sylpheed-Claws in less than five minutes."));
	widget = gtk_label_new(text);
	gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
	gtk_box_pack_start (GTK_BOX(page), widget, FALSE, FALSE, 0);
	g_free(text);

/* user page: 1 */
	i++;
	USER_PAGE = i;
	widget = create_page (wizard, _("About You"));
	gtk_box_pack_start (GTK_BOX(widget), user_page(wizard), FALSE, FALSE, 0);
	PACK_WARNING(_("Bold fields must be completed"));
	
	wizard->pages = g_slist_append(wizard->pages, widget);

/*smtp page: 2 */
	i++;
	SMTP_PAGE = i;
	widget = create_page (wizard, _("Sending mail"));
	gtk_box_pack_start (GTK_BOX(widget), smtp_page(wizard), FALSE, FALSE, 0);
	PACK_WARNING(_("Bold fields must be completed"));
	
	wizard->pages = g_slist_append(wizard->pages, widget);

/* recv+auth page: 3 */
	i++;
	RECV_PAGE = i;
	widget = create_page (wizard, _("Receiving mail"));
	gtk_box_pack_start (GTK_BOX(widget), recv_page(wizard), FALSE, FALSE, 0);
	PACK_WARNING(_("Bold fields must be completed"));
	
	wizard->pages = g_slist_append(wizard->pages, widget);

/* mailbox page: 4 */
	if (create_mailbox) {
		i++;
		MAILBOX_PAGE = i;
		widget = create_page (wizard, _("Saving mail on disk"));
		gtk_box_pack_start (GTK_BOX(widget), mailbox_page(wizard), FALSE, FALSE, 0);
		PACK_WARNING(_("Bold fields must be completed"));
	
		wizard->pages = g_slist_append(wizard->pages, widget);
	}
/* ssl page: 5 */
#ifdef USE_OPENSSL
	i++;
	SSL_PAGE = i;
	widget = create_page (wizard, _("Security"));
	gtk_box_pack_start (GTK_BOX(widget), ssl_page(wizard), FALSE, FALSE, 0);
	PACK_WARNING(_("Bold fields must be completed"));
	
	wizard->pages = g_slist_append(wizard->pages, widget);
#endif

/* done page: 6 */
	i++;
	DONE_PAGE = i;
	page = create_page(wizard, _("Configuration finished"));
	
	wizard->pages = g_slist_append(wizard->pages, page);
	widget = stock_pixmap_widget(wizard->window, 
			  	STOCK_PIXMAP_SYLPHEED_LOGO);

	gtk_box_pack_start (GTK_BOX(page), widget, FALSE, FALSE, 0);
	
	text = g_strdup(_("Sylpheed-Claws is now ready.\n\n"
			  "Click Save to start."));
	widget = gtk_label_new(text);
	gtk_box_pack_start (GTK_BOX(page), widget, FALSE, FALSE, 0);
	g_free(text);


	for (cur = wizard->pages; cur && cur->data; cur = cur->next) {
		gtk_notebook_append_page (GTK_NOTEBOOK(wizard->notebook), 
					  GTK_WIDGET(cur->data), NULL);
	}
	
	g_signal_connect(G_OBJECT(wizard->window), "delete_event",
			 G_CALLBACK(wizard_close_cb), wizard);
	gtk_widget_show_all (wizard->window);

	gtk_widget_hide(wizard->recv_imap_label);
	gtk_widget_hide(wizard->recv_imap_subdir);

	while (!wizard->finished)
		gtk_main_iteration();

	result = wizard->result;
	
	GTK_EVENTS_FLUSH();

	gtk_widget_show(mainwin->window);
	g_free(wizard);

	return result;
}