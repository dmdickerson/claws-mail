/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & the Sylpheed-Claws team
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

#ifndef QUICKSEARCH_H
#define QUICKSEARCH_H 1

typedef enum
{
	QUICK_SEARCH_SUBJECT,
	QUICK_SEARCH_FROM,
	QUICK_SEARCH_TO,
	QUICK_SEARCH_EXTENDED,
} QuickSearchType;

typedef struct _QuickSearch QuickSearch;
typedef void (*QuickSearchExecuteCallback) (QuickSearch *quicksearch, gpointer data);

#include "procmsg.h"

QuickSearch *quicksearch_new();
GtkWidget *quicksearch_get_widget(QuickSearch *quicksearch);
void quicksearch_show(QuickSearch *quicksearch);
void quicksearch_hide(QuickSearch *quicksearch);
void quicksearch_set(QuickSearch *quicksearch, QuickSearchType type, const gchar *matchstring);
gboolean quicksearch_is_active(QuickSearch *quicksearch);
void quicksearch_set_execute_callback(QuickSearch *quicksearch,
				      QuickSearchExecuteCallback callback,
				      gpointer data);
gboolean quicksearch_match(QuickSearch *quicksearch, MsgInfo *msginfo);
gchar *expand_search_string(const gchar *str);
gboolean quicksearch_is_running(QuickSearch *quicksearch);

#endif /* QUICKSEARCH_H */
