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

#ifndef __COMPOSE_H__
#define __COMPOSE_H__

typedef struct _Compose		Compose;
typedef struct _AttachInfo	AttachInfo;

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>

#include "procmsg.h"
#include "procmime.h"
#include "addressbook.h"
#include "prefs_account.h"
#include "undo.h"
#include "toolbar.h"

#ifdef USE_ASPELL
#include "gtkaspell.h"
#endif

typedef enum
{
	COMPOSE_TO,
	COMPOSE_CC,
	COMPOSE_BCC,
	COMPOSE_REPLYTO,
	COMPOSE_NEWSGROUPS,
	COMPOSE_FOLLOWUPTO
} ComposeEntryType;

typedef enum
{
	COMPOSE_REPLY,
	COMPOSE_REPLY_WITH_QUOTE,
	COMPOSE_REPLY_WITHOUT_QUOTE,
	COMPOSE_REPLY_TO_SENDER,
	COMPOSE_FOLLOWUP_AND_REPLY_TO,
	COMPOSE_REPLY_TO_SENDER_WITH_QUOTE,
	COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE,
	COMPOSE_REPLY_TO_ALL,
	COMPOSE_REPLY_TO_ALL_WITH_QUOTE,
	COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE,
	COMPOSE_REPLY_TO_LIST,
	COMPOSE_REPLY_TO_LIST_WITH_QUOTE,
	COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE,
	COMPOSE_FORWARD,
	COMPOSE_FORWARD_AS_ATTACH,
	COMPOSE_FORWARD_INLINE,
	COMPOSE_NEW,
	COMPOSE_REDIRECT,
	COMPOSE_REEDIT
} ComposeMode;

typedef struct {
	guint headernum;
	Compose *compose;
	GtkWidget *combo;
	GtkWidget *entry;
} ComposeHeaderEntry;

struct _Compose
{
	/* start with window widget don`t change order */
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;

	/* Toolbar handlebox */
	GtkWidget *handlebox;
	Toolbar *toolbar;
	
	GtkWidget *vbox2;

	/* Header */
	GtkWidget *table_vbox;
	GtkWidget *table;
/*
	GtkWidget *to_hbox;
	GtkWidget *to_entry;
	GtkWidget *newsgroups_hbox;
	GtkWidget *newsgroups_entry;
*/
	GtkWidget *subject_entry;
/*
	GtkWidget *cc_hbox;
	GtkWidget *cc_entry;
	GtkWidget *bcc_hbox;
	GtkWidget *bcc_entry;
	GtkWidget *reply_hbox;
	GtkWidget *reply_entry;
	GtkWidget *followup_hbox;
	GtkWidget *followup_entry;
*/
	GtkWidget *paned;

	/* Attachments */
	GtkWidget *attach_scrwin;
	GtkWidget *attach_clist;

	/* Others */
	GtkWidget *savemsg_checkbtn;
	GtkWidget *savemsg_entry;

	/* Textedit */
	GtkWidget *edit_vbox;
	GtkWidget *ruler_hbox;
	GtkWidget *ruler;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GtkWidget *focused_editable;

	GtkWidget *popupmenu;

	GtkItemFactory *popupfactory;

	GtkWidget *tmpl_menu;

	ComposeMode mode;

	MsgInfo *targetinfo;
	MsgInfo *replyinfo;
	MsgInfo *autosaved_draft;
	MsgInfo *fwdinfo;

	GtkWidget *header_table;
	GSList    *header_list;
	guint	   header_nextrow;
	ComposeHeaderEntry *header_last;

	gchar	*replyto;
	gchar	*cc;
	gchar	*bcc;
	gchar	*newsgroups;
	gchar	*followup_to;

	gchar	*ml_post;

	gchar	*inreplyto;
	gchar	*references;
	gchar	*msgid;
	gchar	*boundary;

	gboolean use_to;
	gboolean use_cc;
	gboolean use_bcc;
	gboolean use_replyto;
	gboolean use_newsgroups;
	gboolean use_followupto;
	gboolean use_attach;

	/* privacy settings */
	gboolean use_signing;
	gboolean use_encryption;
	
	gint gnupg_mode;

	gboolean modified;

	gboolean sending;
	
	gboolean return_receipt;

	GSList *to_list;
	GSList *newsgroup_list;

	PrefsAccount *account;

	UndoMain *undostruct;

	gchar *sig_str;

	/* external editor */
	gchar *exteditor_file;
	pid_t  exteditor_pid;
	gint   exteditor_readdes;
	gint   exteditor_tag;

#if USE_ASPELL
        /* GNU/aspell spell checker */
        GtkAspell *gtkaspell;
#endif

 	/* Priority */
 	gint priority;

	gchar *redirect_filename;
	
	gboolean remove_references;
};

struct _AttachInfo
{
	gchar *file;
	gchar *content_type;
	EncodingType encoding;
	gchar *name;
	off_t size;
};

Compose *compose_new			(PrefsAccount	*account,
				 	 const gchar	*mailto,
				 	 GPtrArray	*attach_files);

Compose *compose_new_with_folderitem	(PrefsAccount	*account,
					 FolderItem	*item);

void compose_reply_mode			(ComposeMode 	 mode, 
					 GSList 	*msginfo_list, 
					 gchar 		*body);
/* remove */
void compose_followup_and_reply_to	(MsgInfo	*msginfo,
					 gboolean	 quote,
					 gboolean	 to_all,
					 gboolean	 to_sender,
					 const gchar	*body);
void compose_reply			(MsgInfo	*msginfo,
					 gboolean	 quote,
					 gboolean	 to_all,
					 gboolean	 to_ml,
					 gboolean	 to_sender,
					 const gchar	*body);
Compose *compose_forward		(PrefsAccount *account,
					 MsgInfo	*msginfo,
					 gboolean	 as_attach,
					 const gchar	*body,
					 gboolean	 no_extedit);
Compose *compose_forward_multiple	(PrefsAccount	*account, 
					 GSList		*msginfo_list);
/* remove end */

Compose *compose_redirect		(PrefsAccount	*account,
					 MsgInfo	*msginfo);
void compose_reedit			(MsgInfo	*msginfo);

GList *compose_get_compose_list		(void);

void compose_entry_append		(Compose	  *compose,
					 const gchar	  *address,
					 ComposeEntryType  type);

void compose_entry_mark_default_to	(Compose	  *compose,
					 const gchar	  *address);

gint compose_send			(Compose	  *compose);

void compose_update_actions_menu	(Compose	*compose);

void compose_reflect_prefs_all			(void);
void compose_reflect_prefs_pixmap_theme	(void);

void compose_destroy_all                (void);
void compose_draft	                (gpointer data);
void compose_toolbar_cb			(gint 		action, 
					 gpointer 	data);

#endif /* __COMPOSE_H__ */
