/* 
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#ifndef NOTICEVIEW_H__
#define NOTICEVIEW_H__

typedef struct _NoticeView	NoticeView;

#include "stock_pixmap.h"

struct _NoticeView 
{
	GtkWidget	*vbox;
	GtkWidget	*hsep;
	GtkWidget	*hbox;
	GtkWidget	*icon;
	GtkWidget	*text;
	GtkWidget	*button;
	GtkWidget	*window;
	gboolean	 visible;
	gpointer	 user_data;
	void		(*press) (NoticeView *, gpointer user_data);
};

NoticeView	*noticeview_create	(MainWindow 	*mainwin);
void		 noticeview_destroy	(NoticeView	*noticeview);
void		 noticeview_init	(NoticeView	*noticeview);
void		 noticeview_set_icon	(NoticeView	*noticeview,
					 StockPixmap	 icon);
void		 noticeview_set_text	(NoticeView	*noticeview,
					 const gchar	*text);
void		 noticeview_set_button_text 
					(NoticeView	*noticeview,
					 const gchar    *text);
gboolean	 noticeview_is_visible  (NoticeView	*noticeview);
void		 noticeview_show	(NoticeView	*noticeview);
void		 noticeview_hide	(NoticeView	*noticeview);

void		 noticeview_set_button_press_callback
					(NoticeView	*noticeview,
					 GtkSignalFunc   callback,
					 gpointer	*user_data);
					
#endif /* NOTICEVIEW_H__ */
