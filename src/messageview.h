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

#ifndef __MESSAGEVIEW_H__
#define __MESSAGEVIEW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

typedef struct _MessageView		MessageView;

#include "mainwindow.h"
#include "headerview.h"
#include "textview.h"
#include "mimeview.h"
#include "noticeview.h"
#include "procmsg.h"
#include "toolbar.h"

typedef enum
{
	MVIEW_TEXT,
	MVIEW_MIME
} MessageType;

struct _MessageView
{
	GtkWidget *vbox;

	MessageType type;
	gboolean new_window;
	GtkWidget *window;

	/* Toolbar handlebox */
	GtkWidget *handlebox;
	Toolbar *toolbar;
	GtkWidget *menubar;

	HeaderView *headerview;
	TextView *textview;
	MimeView *mimeview;
	NoticeView *noticeview;

	MainWindow *mainwin;

	MsgInfo *msginfo;

	gchar *forced_charset;

	gboolean visible;

	/* From messageview_show */
	gboolean all_headers;
};

MessageView *messageview_create			(MainWindow	*mainwin);
MessageView *messageview_create_with_new_window	(MainWindow	*mainwin);

void messageview_init				(MessageView	*messageview);
void messageview_show				(MessageView	*messageview,
						 MsgInfo	*msginfo,
						 gboolean	 all_headers);
void messageview_clear				(MessageView	*messageview);
void messageview_destroy			(MessageView	*messageview);
void messageview_quote_color_set		(void);
void messageview_set_font			(MessageView	*messageview);

TextView *messageview_get_current_textview	(MessageView	*messageview);

void messageview_copy_clipboard			(MessageView	*messageview);
void messageview_select_all			(MessageView	*messageview);
void messageview_set_position			(MessageView	*messageview,
						 gint		 pos);
void messageview_toggle_view_real		(MessageView 	*messageview);
GList *messageview_get_msgview_list		(void);
void messageview_delete				(MessageView 	*messageview);
gboolean messageview_search_string		(MessageView	*messageview,
						 const gchar	*str,
						 gboolean	 case_sens);
gboolean messageview_search_string_backward	(MessageView	*messageview,
						 const gchar	*str,
						 gboolean	 case_sens);

gboolean messageview_is_visible			(MessageView	*messageview);

void messageview_add_toolbar			(MessageView 	*msgview,
						 GtkWidget 	*vbox);
void messageview_reflect_prefs_pixmap_theme	(void);
gchar *messageview_get_selection		(MessageView 	*msgview);
#endif /* __MESSAGEVIEW_H__ */
