/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2003 Hiroyuki Yamamoto and the Sylpheed-Claws team
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

/*
 * General functions for accessing address book files.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#include <w32lib.h>
#else
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <math.h>
#include <setjmp.h>

#include "intl.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "compose.h"
#include "utils.h"
#include "xml.h"
#include "mgutils.h"
#include "prefs_gtk.h"
#include "codeconv.h"
#include "stock_pixmap.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "toolbar.h"
#include "menu.h"
#include "inc.h"
#include "action.h"
#include "prefs_actions.h"
#include "prefs_common.h"
#include "prefs_toolbar.h"
#include "alertpanel.h"

/* elements */
#define TOOLBAR_TAG_INDEX        "toolbar"
#define TOOLBAR_TAG_ITEM         "item"
#define TOOLBAR_TAG_SEPARATOR    "separator"

#define TOOLBAR_ICON_FILE   "file"    
#define TOOLBAR_ICON_TEXT   "text"     
#define TOOLBAR_ICON_ACTION "action"    

gboolean      toolbar_is_duplicate		(gint           action,
					      	 ToolbarType	source);
static void   toolbar_parse_item		(XMLFile        *file,
					      	 ToolbarType	source);

static gint   toolbar_ret_val_from_text		(const gchar	*text);
static gchar *toolbar_ret_text_from_val		(gint           val);

static void   toolbar_set_default_main		(void);
static void   toolbar_set_default_compose	(void);
static void   toolbar_set_default_msgview	(void);

static void	toolbar_style			(ToolbarType 	 type, 
						 guint 		 action, 
						 gpointer 	 data);

static GtkWidget *get_window_widget		(ToolbarType 	 type, 
						 gpointer 	 data);
static MainWindow *get_mainwin			(gpointer data);
static void activate_compose_button 		(Toolbar	*toolbar,
				     		 ToolbarStyle	 style,
				     		 ComposeButtonType type);

/* toolbar callbacks */
static void toolbar_reply			(gpointer 	 data, 
						 guint 		 action);
static void toolbar_delete_cb			(GtkWidget	*widget,
					 	 gpointer        data);

static void toolbar_compose_cb			(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_cb		   	(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_to_all_cb	   	(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_to_list_cb	   	(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_to_sender_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_forward_cb		   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_next_unread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_ignore_thread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_print_cb			(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_actions_execute_cb	   	(GtkWidget     	*widget,
				  	    	 gpointer      	 data);


static void toolbar_send_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_send_later_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_draft_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_insert_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_attach_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_sig_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_ext_editor_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_linewrap_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_addrbook_cb   		(GtkWidget   	*widget, 
					 	 gpointer     	 data);
#ifdef USE_ASPELL
static void toolbar_check_spelling_cb  		(GtkWidget   	*widget, 
					 	 gpointer     	 data);
#endif
static void toolbar_popup_cb			(gpointer 	 data, 
					 	 guint 		 action, 
					 	 GtkWidget 	*widget);
struct {
	gchar *index_str;
	const gchar *descr;
} toolbar_text [] = {
	{ "A_RECEIVE_ALL",   N_("Receive Mail on all Accounts")         },
	{ "A_RECEIVE_CUR",   N_("Receive Mail on current Account")      },
	{ "A_SEND_QUEUED",   N_("Send Queued Message(s)")               },
	{ "A_COMPOSE_EMAIL", N_("Compose Email")                        },
	{ "A_COMPOSE_NEWS",  N_("Compose News")                         },
	{ "A_REPLY_MESSAGE", N_("Reply to Message")                     },
	{ "A_REPLY_SENDER",  N_("Reply to Sender")                      },
	{ "A_REPLY_ALL",     N_("Reply to All")                         },
	{ "A_REPLY_ML",      N_("Reply to Mailing-list")                },
	{ "A_FORWARD",       N_("Forward Message")                      }, 
	{ "A_DELETE",        N_("Delete Message")                       },
	{ "A_EXECUTE",       N_("Execute")                              },
	{ "A_GOTO_NEXT",     N_("Goto Next Message")                    },
	{ "A_IGNORE_THREAD", N_("Ignore thread")			},
	{ "A_PRINT",	     N_("Print")				},

	{ "A_SEND",          N_("Send Message")                         },
	{ "A_SENDL",         N_("Put into queue folder and send later") },
	{ "A_DRAFT",         N_("Save to draft folder")                 },
	{ "A_INSERT",        N_("Insert file")                          },   
	{ "A_ATTACH",        N_("Attach file")                          },
	{ "A_SIG",           N_("Insert signature")                     },
	{ "A_EXTEDITOR",     N_("Edit with external editor")            },
	{ "A_LINEWRAP",      N_("Wrap all long lines")                  }, 
	{ "A_ADDRBOOK",      N_("Address book")                         },
#ifdef USE_ASPELL
	{ "A_CHECK_SPELLING",N_("Check spelling")                       },
#endif
	{ "A_SYL_ACTIONS",   N_("Sylpheed Actions Feature")             }, 
	{ "A_SEPARATOR",     "Separator"				}
};

/* struct holds configuration files and a list of
 * currently active toolbar items 
 * TOOLBAR_MAIN, TOOLBAR_COMPOSE and TOOLBAR_MSGVIEW
 * give us an index
 */
struct {
	const gchar  *conf_file;
	GSList       *item_list;
} toolbar_config[3] = {
	{ "toolbar_main.xml",    NULL},
	{ "toolbar_compose.xml", NULL}, 
  	{ "toolbar_msgview.xml", NULL}
};

static GtkItemFactoryEntry reply_popup_entries[] =
{
	{N_("/Reply with _quote"), NULL, toolbar_popup_cb, COMPOSE_REPLY_WITH_QUOTE, NULL},
	{N_("/_Reply without quote"), NULL, toolbar_popup_cb, COMPOSE_REPLY_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replyall_popup_entries[] =
{
	{N_("/Reply to all with _quote"), "<shift>A", toolbar_popup_cb, COMPOSE_REPLY_TO_ALL_WITH_QUOTE, NULL},
	{N_("/_Reply to all without quote"), "a", toolbar_popup_cb, COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replylist_popup_entries[] =
{
	{N_("/Reply to list with _quote"), NULL, toolbar_popup_cb, COMPOSE_REPLY_TO_LIST_WITH_QUOTE, NULL},
	{N_("/_Reply to list without quote"), NULL, toolbar_popup_cb, COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replysender_popup_entries[] =
{
	{N_("/Reply to sender with _quote"), NULL, toolbar_popup_cb, COMPOSE_REPLY_TO_SENDER_WITH_QUOTE, NULL},
	{N_("/_Reply to sender without quote"), NULL, toolbar_popup_cb, COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry fwd_popup_entries[] =
{
	{N_("/_Forward message (inline style)"), "f", toolbar_popup_cb, COMPOSE_FORWARD_INLINE, NULL},
	{N_("/Forward message as _attachment"), "<shift>F", toolbar_popup_cb, COMPOSE_FORWARD_AS_ATTACH, NULL}
};


gint toolbar_ret_val_from_descr(const gchar *descr)
{
	gint i;

	for (i = 0; i < N_ACTION_VAL; i++) {
		if (g_strcasecmp(gettext(toolbar_text[i].descr), descr) == 0)
				return i;
	}
	
	return -1;
}

gchar *toolbar_ret_descr_from_val(gint val)
{
	g_return_val_if_fail(val >=0 && val < N_ACTION_VAL, NULL);

	return gettext(toolbar_text[val].descr);
}

static gint toolbar_ret_val_from_text(const gchar *text)
{
	gint i;
	
	for (i = 0; i < N_ACTION_VAL; i++) {
		if (g_strcasecmp(toolbar_text[i].index_str, text) == 0)
				return i;
	}
	
	return -1;
}

static gchar *toolbar_ret_text_from_val(gint val)
{
	g_return_val_if_fail(val >=0 && val < N_ACTION_VAL, NULL);

	return toolbar_text[val].index_str;
}

gboolean toolbar_is_duplicate(gint action, ToolbarType source)
{
	GSList *cur;

	if ((action == A_SEPARATOR) || (action == A_SYL_ACTIONS)) 
		return FALSE;

	for (cur = toolbar_config[source].item_list; cur != NULL; cur = cur->next) {
		ToolbarItem *item = (ToolbarItem*) cur->data;
		
		if (item->index == action)
			return TRUE;
	}
	return FALSE;
}

/* depending on toolbar type this function 
   returns a list of available toolbar events being 
   displayed by prefs_toolbar
*/
GList *toolbar_get_action_items(ToolbarType source)
{
	GList *items = NULL;
	gint i = 0;
	
	if (source == TOOLBAR_MAIN) {
		gint main_items[]   = { A_RECEIVE_ALL,   A_RECEIVE_CUR,   A_SEND_QUEUED,
					A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER, 
					A_REPLY_ALL,     A_REPLY_ML,      A_FORWARD, 
					A_DELETE,        A_EXECUTE,       A_GOTO_NEXT, 
					A_IGNORE_THREAD, A_PRINT,  
					A_ADDRBOOK, 	 A_SYL_ACTIONS };

		for (i = 0; i < sizeof main_items / sizeof main_items[0]; i++)  {
			items = g_list_append(items, gettext(toolbar_text[main_items[i]].descr));
			if (main_items[i] == A_PRINT) {
				g_print("$$$ descr %s, trans %s\n",
					toolbar_text[main_items[i]].descr,
					gettext(toolbar_text[main_items[i]].descr));
			}
		}	
	}
	else if (source == TOOLBAR_COMPOSE) {
		gint comp_items[] =   {	A_SEND,          A_SENDL,        A_DRAFT,
					A_INSERT,        A_ATTACH,       A_SIG,
					A_EXTEDITOR,     A_LINEWRAP,     A_ADDRBOOK,
#ifdef USE_ASPELL
					A_CHECK_SPELLING, 
#endif
					A_SYL_ACTIONS };	

		for (i = 0; i < sizeof comp_items / sizeof comp_items[0]; i++) 
			items = g_list_append(items, gettext(toolbar_text[comp_items[i]].descr));
	}
	else if (source == TOOLBAR_MSGVIEW) {
		gint msgv_items[] =   { A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER,
				        A_REPLY_ALL,     A_REPLY_ML,      A_FORWARD,
				        A_DELETE,        A_GOTO_NEXT,	  A_ADDRBOOK,
					A_SYL_ACTIONS };	

		for (i = 0; i < sizeof msgv_items / sizeof msgv_items[0]; i++) 
			items = g_list_append(items, gettext(toolbar_text[msgv_items[i]].descr));
	}

	return items;
}

static void toolbar_parse_item(XMLFile *file, ToolbarType source)
{
	GList *attr;
	gchar *name, *value;
	ToolbarItem *item = NULL;

	attr = xml_get_current_tag_attr(file);
	item = g_new0(ToolbarItem, 1);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		
		if (g_strcasecmp(name, TOOLBAR_ICON_FILE) == 0) 
			item->file = g_strdup (value);
		else if (g_strcasecmp(name, TOOLBAR_ICON_TEXT) == 0)
#ifdef WIN32
			item->text = g_locale_to_utf8(value, -1, NULL, NULL, NULL);
#else
			item->text = g_strdup (value);
#endif
		else if (g_strcasecmp(name, TOOLBAR_ICON_ACTION) == 0)
			item->index = toolbar_ret_val_from_text(value);

		attr = g_list_next(attr);
	}
	if (item->index != -1) {
		
		if (!toolbar_is_duplicate(item->index, source)) 
			toolbar_config[source].item_list = g_slist_append(toolbar_config[source].item_list,
									 item);
	}
}

static void toolbar_set_default_main(void) 
{
	struct {
		gint action;
		gint icon;
		gchar *text;
	} default_toolbar[] = {
		{ A_RECEIVE_CUR,   STOCK_PIXMAP_MAIL_RECEIVE,         _("Get")     },
		{ A_RECEIVE_ALL,   STOCK_PIXMAP_MAIL_RECEIVE_ALL,     _("Get All") },
		{ A_SEPARATOR,     0,                                 ("")         }, 
		{ A_SEND_QUEUED,   STOCK_PIXMAP_MAIL_SEND_QUEUE,      _("Send")    },
		{ A_COMPOSE_EMAIL, STOCK_PIXMAP_MAIL_COMPOSE,         _("Email")   },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_REPLY_MESSAGE, STOCK_PIXMAP_MAIL_REPLY,           _("Reply")   }, 
		{ A_REPLY_ALL,     STOCK_PIXMAP_MAIL_REPLY_TO_ALL,    _("All")     },
		{ A_REPLY_SENDER,  STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR, _("Sender")  },
		{ A_FORWARD,       STOCK_PIXMAP_MAIL_FORWARD,         _("Forward") },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_DELETE,        STOCK_PIXMAP_CLOSE,                _("Delete")  },
		{ A_EXECUTE,       STOCK_PIXMAP_EXEC,                 _("Execute") },
		{ A_GOTO_NEXT,     STOCK_PIXMAP_DOWN_ARROW,           _("Next")    }
	};
	
	gint i;
	
	for (i = 0; i < sizeof(default_toolbar) / sizeof(default_toolbar[0]); i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			
			gchar *file = stock_pixmap_get_name((StockPixmap)default_toolbar[i].icon);
			
			toolbar_item->file  = g_strdup(file);
			toolbar_item->index = default_toolbar[i].action;
			toolbar_item->text  = g_strdup(default_toolbar[i].text);
		} else {

			toolbar_item->file  = g_strdup(TOOLBAR_TAG_SEPARATOR);
			toolbar_item->index = A_SEPARATOR;
		}
		
		if (toolbar_item->index != -1) {
			if ( !toolbar_is_duplicate(toolbar_item->index, TOOLBAR_MAIN)) 
				toolbar_config[TOOLBAR_MAIN].item_list = 
					g_slist_append(toolbar_config[TOOLBAR_MAIN].item_list, toolbar_item);
		}	
	}
}

static void toolbar_set_default_compose(void)
{
	struct {
		gint action;
		gint icon;
		gchar *text;
	} default_toolbar[] = {
		{ A_SEND,      STOCK_PIXMAP_MAIL_SEND,         _("Send")       },
		{ A_SENDL,     STOCK_PIXMAP_MAIL_SEND_QUEUE,   _("Send later") },
		{ A_DRAFT,     STOCK_PIXMAP_MAIL,              _("Draft")      },
		{ A_SEPARATOR, 0,                               ("")           }, 
		{ A_INSERT,    STOCK_PIXMAP_INSERT_FILE,       _("Insert")     },
		{ A_ATTACH,    STOCK_PIXMAP_MAIL_ATTACH,       _("Attach")     },
		{ A_SIG,       STOCK_PIXMAP_MAIL_SIGN,         _("Signature")  },
		{ A_SEPARATOR, 0,                               ("")           },
		{ A_EXTEDITOR, STOCK_PIXMAP_EDIT_EXTERN,       _("Editor")     },
		{ A_LINEWRAP,  STOCK_PIXMAP_LINEWRAP,          _("Linewrap")   },
		{ A_SEPARATOR, 0,                               ("")           },
		{ A_ADDRBOOK,  STOCK_PIXMAP_ADDRESS_BOOK,      _("Address")    }
	};
	
	gint i;

	for (i = 0; i < sizeof(default_toolbar) / sizeof(default_toolbar[0]); i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			
			gchar *file = stock_pixmap_get_name((StockPixmap)default_toolbar[i].icon);
			
			toolbar_item->file  = g_strdup(file);
			toolbar_item->index = default_toolbar[i].action;
			toolbar_item->text  = g_strdup(default_toolbar[i].text);
		} else {

			toolbar_item->file  = g_strdup(TOOLBAR_TAG_SEPARATOR);
			toolbar_item->index = A_SEPARATOR;
		}
		
		if (toolbar_item->index != -1) {
			if ( !toolbar_is_duplicate(toolbar_item->index, TOOLBAR_COMPOSE)) 
				toolbar_config[TOOLBAR_COMPOSE].item_list = 
					g_slist_append(toolbar_config[TOOLBAR_COMPOSE].item_list, toolbar_item);
		}	
	}
}

static void toolbar_set_default_msgview(void)
{
	struct {
		gint action;
		gint icon;
		gchar *text;
	} default_toolbar[] = {
		{ A_REPLY_MESSAGE, STOCK_PIXMAP_MAIL_REPLY,           _("Reply")   }, 
		{ A_REPLY_ALL,     STOCK_PIXMAP_MAIL_REPLY_TO_ALL,    _("All")     },
		{ A_REPLY_SENDER,  STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR, _("Sender")  },
		{ A_FORWARD,       STOCK_PIXMAP_MAIL_FORWARD,         _("Forward") },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_DELETE,        STOCK_PIXMAP_CLOSE,                _("Delete")  },
		{ A_GOTO_NEXT,     STOCK_PIXMAP_DOWN_ARROW,           _("Next")    }
	};
	
	gint i;

	for (i = 0; i < sizeof(default_toolbar) / sizeof(default_toolbar[0]); i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			
			gchar *file = stock_pixmap_get_name((StockPixmap)default_toolbar[i].icon);
			
			toolbar_item->file  = g_strdup(file);
			toolbar_item->index = default_toolbar[i].action;
			toolbar_item->text  = g_strdup(default_toolbar[i].text);
		} else {

			toolbar_item->file  = g_strdup(TOOLBAR_TAG_SEPARATOR);
			toolbar_item->index = A_SEPARATOR;
		}
		
		if (toolbar_item->index != -1) {
			if ( !toolbar_is_duplicate(toolbar_item->index, TOOLBAR_MSGVIEW)) 
				toolbar_config[TOOLBAR_MSGVIEW].item_list = 
					g_slist_append(toolbar_config[TOOLBAR_MSGVIEW].item_list, toolbar_item);
		}	
	}
}

void toolbar_set_default(ToolbarType source)
{
	if (source == TOOLBAR_MAIN)
		toolbar_set_default_main();
	else if  (source == TOOLBAR_COMPOSE)
		toolbar_set_default_compose();
	else if  (source == TOOLBAR_MSGVIEW)
		toolbar_set_default_msgview();
}

void toolbar_save_config_file(ToolbarType source)
{
	GSList *cur;
	FILE *fp;
	PrefFile *pfile;
	gchar *fileSpec = NULL;

	debug_print("save Toolbar Configuration to %s\n", toolbar_config[source].conf_file);

	fileSpec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, toolbar_config[source].conf_file, NULL );
	pfile = prefs_write_open(fileSpec);
	g_free( fileSpec );
	if( pfile ) {
		fp = pfile->fp;
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n",
			conv_get_current_charset_str());

		fprintf(fp, "<%s>\n", TOOLBAR_TAG_INDEX);

		for (cur = toolbar_config[source].item_list; cur != NULL; cur = cur->next) {
			ToolbarItem *toolbar_item = (ToolbarItem*) cur->data;
			
			if (toolbar_item->index != A_SEPARATOR) 
#ifdef WIN32
			{
			  	gchar *p_text=g_locale_from_utf8(toolbar_item->text,
					-1,NULL,NULL,NULL);
#endif
				fprintf(fp, "\t<%s %s=\"%s\" %s=\"%s\" %s=\"%s\"/>\n",
					TOOLBAR_TAG_ITEM, 
					TOOLBAR_ICON_FILE, toolbar_item->file,
#ifdef WIN32
					TOOLBAR_ICON_TEXT, p_text,
#else
					TOOLBAR_ICON_TEXT, toolbar_item->text,
#endif
					TOOLBAR_ICON_ACTION, 
					toolbar_ret_text_from_val(toolbar_item->index));
#ifdef WIN32
				g_free(p_text);
			}
#endif
			else 
				fprintf(fp, "\t<%s/>\n", TOOLBAR_TAG_SEPARATOR); 
		}

		fprintf(fp, "</%s>\n", TOOLBAR_TAG_INDEX);	
	
		if (prefs_file_close (pfile) < 0 ) 
			g_warning("failed to write toolbar configuration to file\n");
	} else
		g_warning("failed to open toolbar configuration file for writing\n");
}

void toolbar_read_config_file(ToolbarType source)
{
	XMLFile *file   = NULL;
	gchar *fileSpec = NULL;
	GList *attr;
	gboolean retVal;
	jmp_buf    jumper;

	debug_print("read Toolbar Configuration from %s\n", toolbar_config[source].conf_file);

	fileSpec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, toolbar_config[source].conf_file, NULL );
	file = xml_open_file(fileSpec);
	g_free(fileSpec);

	toolbar_clear_list(source);

	if (file) {
		if ((setjmp(jumper))
		|| (xml_get_dtd(file))
		|| (xml_parse_next_tag(file))
		|| (!xml_compare_tag(file, TOOLBAR_TAG_INDEX))) {
			xml_close_file(file);
			return;
		}

		attr = xml_get_current_tag_attr(file);
		
		retVal = TRUE;
		for (;;) {
			if (!file->level) 
				break;
			/* Get item tag */
			if (xml_parse_next_tag(file)) 
				longjmp(jumper, 1);

			/* Get next tag (icon, icon_text or icon_action) */
			if (xml_compare_tag(file, TOOLBAR_TAG_ITEM)) {
				toolbar_parse_item(file, source);
			} else if (xml_compare_tag(file, TOOLBAR_TAG_SEPARATOR)) {
				ToolbarItem *item = g_new0(ToolbarItem, 1);
			
				item->file   = g_strdup(toolbar_ret_descr_from_val(A_SEPARATOR));
				item->index  = A_SEPARATOR;
				toolbar_config[source].item_list = 
					g_slist_append(toolbar_config[source].item_list, item);
			}
		}
		xml_close_file(file);
	}

	if ((!file) || (g_slist_length(toolbar_config[source].item_list) == 0)) {

		if (source == TOOLBAR_MAIN) 
			toolbar_set_default(TOOLBAR_MAIN);
		else if (source == TOOLBAR_COMPOSE) 
			toolbar_set_default(TOOLBAR_COMPOSE);
		else if (source == TOOLBAR_MSGVIEW) 
			toolbar_set_default(TOOLBAR_MSGVIEW);
		else {		
			g_warning("failed to write Toolbar Configuration to %s\n", toolbar_config[source].conf_file);
			return;
		}

		toolbar_save_config_file(source);
	}
}

/*
 * clears list of toolbar items read from configuration files
 */
void toolbar_clear_list(ToolbarType source)
{
	while (toolbar_config[source].item_list != NULL) {
		ToolbarItem *item = (ToolbarItem*) toolbar_config[source].item_list->data;
		
		toolbar_config[source].item_list = 
			g_slist_remove(toolbar_config[source].item_list, item);

		if (item->file)
			g_free(item->file);
		if (item->text)
			g_free(item->text);
		g_free(item);	
	}
	g_slist_free(toolbar_config[source].item_list);
}


/* 
 * return list of Toolbar items
 */
GSList *toolbar_get_list(ToolbarType source)
{
	GSList *list = NULL;

	if ((source == TOOLBAR_MAIN) || (source == TOOLBAR_COMPOSE) || (source == TOOLBAR_MSGVIEW))
		list = toolbar_config[source].item_list;

	return list;
}

void toolbar_set_list_item(ToolbarItem *t_item, ToolbarType source)
{
	ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);

	toolbar_item->file  = g_strdup(t_item->file);
	toolbar_item->text  = g_strdup(t_item->text);
	toolbar_item->index = t_item->index;
	
	toolbar_config[source].item_list = 
		g_slist_append(toolbar_config[source].item_list,
			       toolbar_item);
}

void toolbar_action_execute(GtkWidget    *widget,
			    GSList       *action_list, 
			    gpointer     data,
			    gint         source) 
{
	GSList *cur, *lop;
	gchar *action, *action_p;
	gboolean found = FALSE;
	gint i = 0;

	for (cur = action_list; cur != NULL;  cur = cur->next) {
		ToolbarSylpheedActions *act = (ToolbarSylpheedActions*)cur->data;

		if (widget == act->widget) {
			
			for (lop = prefs_common.actions_list; lop != NULL; lop = lop->next) {
				action = g_strdup((gchar*)lop->data);

				action_p = strstr(action, ": ");
				action_p[0] = 0x00;
				if (g_strcasecmp(act->name, action) == 0) {
					found = TRUE;
					g_free(action);
					break;
				} else 
					i++;
				g_free(action);
			}
			if (found) 
				break;
		}
	}

	if (found) 
		actions_execute(data, i, widget, source);
	else
		g_warning ("Error: did not find Sylpheed Action to execute");
}

static void activate_compose_button (Toolbar           *toolbar,
				     ToolbarStyle      style,
				     ComposeButtonType type)
{
	if ((!toolbar->compose_mail_btn) || (!toolbar->compose_news_btn))
		return;

	gtk_widget_hide(type == COMPOSEBUTTON_NEWS ? toolbar->compose_mail_btn 
			: toolbar->compose_news_btn);
	gtk_widget_show(type == COMPOSEBUTTON_NEWS ? toolbar->compose_news_btn
			: toolbar->compose_mail_btn);
	toolbar->compose_btn_type = type;	
}

void toolbar_set_compose_button(Toolbar            *toolbar, 
				ComposeButtonType  compose_btn_type)
{
	if (toolbar->compose_btn_type != compose_btn_type)
		activate_compose_button(toolbar, 
					prefs_common.toolbar_style,
					compose_btn_type);
}

void toolbar_toggle(guint action, gpointer data)
{
	MainWindow *mainwin = (MainWindow*)data;
	GList *list;
	GList *cur;

	g_return_if_fail(mainwin != NULL);

	toolbar_style(TOOLBAR_MAIN, action, mainwin);

	list = compose_get_compose_list();
	for (cur = list; cur != NULL; cur = cur->next) {
		toolbar_style(TOOLBAR_COMPOSE, action, cur->data);
	}
	list = messageview_get_msgview_list();
	for (cur = list; cur != NULL; cur = cur->next) {
		toolbar_style(TOOLBAR_MSGVIEW, action, cur->data);
	}
	
}

void toolbar_set_style(GtkWidget *toolbar_wid, GtkWidget *handlebox_wid, guint action)
{
	switch ((ToolbarStyle)action) {
	case TOOLBAR_NONE:
		gtk_widget_hide(handlebox_wid);
		break;
	case TOOLBAR_ICON:
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar_wid),
				      GTK_TOOLBAR_ICONS);
		break;
	case TOOLBAR_TEXT:
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar_wid),
				      GTK_TOOLBAR_TEXT);
		break;
	case TOOLBAR_BOTH:
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar_wid),
				      GTK_TOOLBAR_BOTH);
		break;
	default:
		return;
	}

	prefs_common.toolbar_style = (ToolbarStyle)action;
	gtk_widget_set_usize(handlebox_wid, 1, -1);
	
	if (prefs_common.toolbar_style != TOOLBAR_NONE) {
		gtk_widget_show(handlebox_wid);
		gtk_widget_queue_resize(handlebox_wid);
	}
}
/*
 * Change the style of toolbar
 */
static void toolbar_style(ToolbarType type, guint action, gpointer data)
{
	GtkWidget  *handlebox_wid;
	GtkWidget  *toolbar_wid;
	MainWindow *mainwin = (MainWindow*)data;
	Compose    *compose = (Compose*)data;
	MessageView *msgview = (MessageView*)data;
	
	g_return_if_fail(data != NULL);
	
	switch (type) {
	case TOOLBAR_MAIN:
		handlebox_wid = mainwin->handlebox;
		toolbar_wid = mainwin->toolbar->toolbar;
		break;
	case TOOLBAR_COMPOSE:
		handlebox_wid = compose->handlebox;
		toolbar_wid = compose->toolbar->toolbar;
		break;
	case TOOLBAR_MSGVIEW: 
		handlebox_wid = msgview->handlebox;
		toolbar_wid = msgview->toolbar->toolbar;
		break;
	default:

		return;
	}
	toolbar_set_style(toolbar_wid, handlebox_wid, action);
}

/* Toolbar handling */
static void toolbar_inc_cb(GtkWidget	*widget,
			   gpointer	 data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;	
		inc_mail_cb(mainwin, 0, NULL);
		break;
	default:
		break;
	}
}

static void toolbar_inc_all_cb(GtkWidget	*widget,
			       gpointer	 	 data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		inc_all_account_mail_cb(mainwin, 0, NULL);
		break;
	default:
		break;
	}
}

static void toolbar_send_queued_cb(GtkWidget *widget,gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		send_queue_cb(mainwin, 0, NULL);
		break;
	default:
		break;
	}
}

static void toolbar_exec_cb(GtkWidget	*widget,
			    gpointer	 data)
{
	MainWindow *mainwin = get_mainwin(data);

	g_return_if_fail(mainwin != NULL);
	summary_execute(mainwin->summaryview);
}



/* popup callback functions */
static void toolbar_reply_popup_cb(GtkWidget       *widget, 
				   GdkEventButton  *event, 
				   gpointer         data)
{
	Toolbar *toolbar_data = (Toolbar*)data;
	
	if (!event) return;
	
	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(toolbar_data->reply_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	GtkWidget *window;
	GtkWidget *reply_btn;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin   = (MainWindow*)toolbar_item->parent;
		reply_btn = mainwin->toolbar->reply_btn;
		window    = mainwin->window;
		break;
	case TOOLBAR_MSGVIEW:
		msgview   = (MessageView*)toolbar_item->parent;
		reply_btn = msgview->toolbar->reply_btn;
		window    = msgview->window;
		break;
	default:
		return;
	}

	gtk_button_set_relief(GTK_BUTTON(reply_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(window, NULL, NULL);
}

static void toolbar_reply_to_all_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Toolbar *toolbar_data = (Toolbar*)data;
	
	if (!event) return;
	
	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(toolbar_data->replyall_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_to_all_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	GtkWidget *window;
	GtkWidget *replyall_btn;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin      = (MainWindow*)toolbar_item->parent; 
		replyall_btn = mainwin->toolbar->replyall_btn;
		window       = mainwin->window;
		break;
	case TOOLBAR_MSGVIEW:
		msgview      = (MessageView*)toolbar_item->parent;
		replyall_btn = msgview->toolbar->replyall_btn;
		window       = msgview->window;
		break;
	default:
		return;
	}

	gtk_button_set_relief(GTK_BUTTON(replyall_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(window, NULL, NULL);
}

static void toolbar_reply_to_list_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Toolbar *toolbar_data = (Toolbar*)data;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(toolbar_data->replylist_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_to_list_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	gpointer parent = toolbar_item->parent;
	GtkWidget *window;
	GtkWidget *replylist_btn;


	g_return_if_fail(toolbar_item != NULL);

	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		replylist_btn = ((MainWindow*)parent)->toolbar->replylist_btn;
		window        = ((MainWindow*)parent)->window;
		break;
	case TOOLBAR_MSGVIEW:
		replylist_btn = ((MessageView*)parent)->toolbar->replylist_btn;
		window        = ((MessageView*)parent)->window;
		break;
	default:
		return;
	}

	gtk_button_set_relief(GTK_BUTTON(replylist_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(window, NULL, NULL);
}

static void toolbar_reply_to_sender_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Toolbar *toolbar_data = (Toolbar*)data;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(toolbar_data->replysender_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_to_sender_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	gpointer parent = toolbar_item->parent;
	GtkWidget *window;
	GtkWidget *replysender_btn;

	g_return_if_fail(toolbar_item != NULL);

	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		replysender_btn = ((MainWindow*)parent)->toolbar->replysender_btn;
		window          = ((MainWindow*)parent)->window;
		break;
	case TOOLBAR_MSGVIEW:
		replysender_btn = ((MessageView*)parent)->toolbar->replysender_btn;
		window          = ((MessageView*)parent)->window;
		break;
	default:
		return;
	}

	gtk_button_set_relief(GTK_BUTTON(replysender_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(window, NULL, NULL);
}

static void toolbar_forward_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Toolbar *toolbar_data = (Toolbar*)data;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(toolbar_data->fwd_popup), NULL, NULL,
			       menu_button_position, widget,
			       event->button, event->time);
	}
}

static void toolbar_forward_popup_closed_cb (GtkMenuShell *menu_shell, 
					     gpointer     data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	gpointer parent = toolbar_item->parent;
	GtkWidget *window;
	GtkWidget *fwd_btn;

	g_return_if_fail(toolbar_item != NULL);

	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		fwd_btn = ((MainWindow*)parent)->toolbar->fwd_btn;
		window  = ((MainWindow*)parent)->window;
		break;
	case TOOLBAR_MSGVIEW:
		fwd_btn = ((MessageView*)parent)->toolbar->fwd_btn;
		window  = ((MessageView*)parent)->window;
		break;
	default:
		return;
	}

	gtk_button_set_relief(GTK_BUTTON(fwd_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(window, NULL, NULL);
}

/*
 * Delete current/selected(s) message(s)
 */
static void toolbar_delete_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);
	
	switch (toolbar_item->type) {
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		messageview_delete(msgview);
        	break;
        case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
        	summary_delete(mainwin->summaryview);
        	break;
        default: 
        	debug_print("toolbar event not supported\n");
        	break;
	}
}


/*
 * Compose new message
 */
static void toolbar_compose_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		if (mainwin->toolbar->compose_btn_type == COMPOSEBUTTON_NEWS) 
			compose_news_cb(mainwin, 0, NULL);
		else
			compose_mail_cb(mainwin, 0, NULL);
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		compose_new_with_folderitem(NULL, 
					    msgview->msginfo->folder);
		break;	
	default:
		debug_print("toolbar event not supported\n");
	}
}

static void toolbar_popup_cb(gpointer data, guint action, GtkWidget *widget)
{
	toolbar_reply(data, action);
}


/*
 * Reply Message
 */
static void toolbar_reply_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, prefs_common.reply_with_quote ? 
		      COMPOSE_REPLY_WITH_QUOTE : COMPOSE_REPLY_WITHOUT_QUOTE);
}


/*
 * Reply message to Sender and All recipients
 */
static void toolbar_reply_to_all_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data,
		      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_ALL_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE);
}


/*
 * Reply to Mailing List
 */
static void toolbar_reply_to_list_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, 
		      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_LIST_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE);
}


/*
 * Reply to sender of message
 */ 
static void toolbar_reply_to_sender_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, 
		      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_SENDER_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE);
}

/*
 * Open addressbook
 */ 
static void toolbar_addrbook_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	Compose *compose;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
	case TOOLBAR_MSGVIEW:
		compose = NULL;
		break;
	case TOOLBAR_COMPOSE:
		compose = (Compose *)toolbar_item->parent;
		break;
	default:
		return;
	}
	addressbook_open(compose);
}


/*
 * Forward current/selected(s) message(s)
 */
static void toolbar_forward_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, COMPOSE_FORWARD);
}


/*
 * Goto Next Unread Message
 */
static void toolbar_next_unread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		summary_select_next_unread(mainwin->summaryview);
		break;
		
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		summary_select_next_unread(msgview->mainwin->summaryview);
		
		/* Now we need to update the messageview window */
		if (msgview->mainwin->summaryview->selected) {
			GtkCTree *ctree = GTK_CTREE(msgview->mainwin->summaryview->ctree);
			
			MsgInfo * msginfo = gtk_ctree_node_get_row_data(ctree, 
									msgview->mainwin->summaryview->selected);
		       
			messageview_show(msgview, msginfo, 
					 msgview->all_headers);
		} else {
			gtk_widget_destroy(msgview->window);
		}
		break;
	default:
		debug_print("toolbar event not supported\n");
	}
}

static void toolbar_ignore_thread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_toggle_ignore_thread(mainwin->summaryview);
		break;
	case TOOLBAR_MSGVIEW:
		/* TODO: see toolbar_next_unread_cb() if you need
		 * this in the message view */
		break;
	default:
		debug_print("toolbar event not supported\n");
		break;
	}
}

static void toolbar_print_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_print(mainwin->summaryview);
		break;
	case TOOLBAR_MSGVIEW:
		/* TODO: see toolbar_next_unread_cb() if you need
		 * this in the message view */
		break;
	default:
		debug_print("toolbar event not supported\n");
		break;
	}
}

static void toolbar_send_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_SEND, data);
}

static void toolbar_send_later_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_SENDL, data);
}

static void toolbar_draft_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_DRAFT, data);
}

static void toolbar_insert_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_INSERT, data);
}

static void toolbar_attach_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_ATTACH, data);
}

static void toolbar_sig_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_SIG, data);
}

static void toolbar_ext_editor_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_EXTEDITOR, data);
}

static void toolbar_linewrap_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_LINEWRAP, data);
}

#ifdef USE_ASPELL
static void toolbar_check_spelling_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_CHECK_SPELLING, data);
}
#endif
/*
 * Execute actions from toolbar
 */
static void toolbar_actions_execute_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	GSList *action_list;
	MainWindow *mainwin;
	Compose *compose;
	MessageView *msgview;
	gpointer parent = toolbar_item->parent;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)parent;
		action_list = mainwin->toolbar->action_list;
		break;
	case TOOLBAR_COMPOSE:
		compose = (Compose*)parent;
		action_list = compose->toolbar->action_list;
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent;
		action_list = msgview->toolbar->action_list;
		break;
	default:
		debug_print("toolbar event not supported\n");
		return;
	}
	toolbar_action_execute(widget, action_list, parent, toolbar_item->type);	
}

static MainWindow *get_mainwin(gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin = NULL;
	MessageView *msgview;

	g_return_val_if_fail(toolbar_item != NULL, NULL);

	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		mainwin = (MainWindow*)msgview->mainwin;
		break;
	default:
		break;
	}

	return mainwin;
}

static GtkWidget *get_window_widget(ToolbarType type, gpointer data)
{
	MainWindow *mainwin;
	MessageView *msgview;

	switch (type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)data;
		return mainwin->window;
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)data;
		return msgview->vbox;
		break;
	default:
		break;
	}
	return NULL;
}

static void toolbar_buttons_cb(GtkWidget   *widget, 
			       ToolbarItem *item)
{
	gint num_items;
	gint i;
	struct {
		gint   index;
		void (*func)(GtkWidget *widget, gpointer data);
	} callbacks[] = {
		{ A_RECEIVE_ALL,	toolbar_inc_all_cb		},
		{ A_RECEIVE_CUR,	toolbar_inc_cb			},
		{ A_SEND_QUEUED,	toolbar_send_queued_cb		},
		{ A_COMPOSE_EMAIL,	toolbar_compose_cb		},
		{ A_COMPOSE_NEWS,	toolbar_compose_cb		},
		{ A_REPLY_MESSAGE,	toolbar_reply_cb		},
		{ A_REPLY_SENDER,	toolbar_reply_to_sender_cb	},
		{ A_REPLY_ALL,		toolbar_reply_to_all_cb		},
		{ A_REPLY_ML,		toolbar_reply_to_list_cb	},
		{ A_FORWARD,		toolbar_forward_cb		},
		{ A_DELETE,         	toolbar_delete_cb		},
		{ A_EXECUTE,        	toolbar_exec_cb			},
		{ A_GOTO_NEXT,      	toolbar_next_unread_cb		},
		{ A_IGNORE_THREAD,	toolbar_ignore_thread_cb	},
		{ A_PRINT,		toolbar_print_cb		},

		{ A_SEND,		toolbar_send_cb       		},
		{ A_SENDL,		toolbar_send_later_cb 		},
		{ A_DRAFT,		toolbar_draft_cb      		},
		{ A_INSERT,		toolbar_insert_cb     		},
		{ A_ATTACH,		toolbar_attach_cb     		},
		{ A_SIG,		toolbar_sig_cb	      		},
		{ A_EXTEDITOR,		toolbar_ext_editor_cb 		},
		{ A_LINEWRAP,		toolbar_linewrap_cb   		},
		{ A_ADDRBOOK,		toolbar_addrbook_cb		},
#ifdef USE_ASPELL
		{ A_CHECK_SPELLING,     toolbar_check_spelling_cb       },
#endif
		{ A_SYL_ACTIONS,	toolbar_actions_execute_cb	}
	};

	num_items = sizeof(callbacks)/sizeof(callbacks[0]);

	for (i = 0; i < num_items; i++) {
		if (callbacks[i].index == item->index) {
			callbacks[i].func(widget, item);
			return;
		}
	}
}

/**
 * Create a new toolbar with specified type
 * if a callback list is passed it will be used before the 
 * common callback list
 **/
Toolbar *toolbar_create(ToolbarType 	 type, 
	  		GtkWidget 	*container,
			gpointer 	 data)
{
	ToolbarItem *toolbar_item;

	GtkWidget *toolbar;
	GtkWidget *icon_wid = NULL;
	GtkWidget *icon_news;
	GtkWidget *item;
	GtkWidget *item_news;
	GtkWidget *window_wid;

	guint n_menu_entries;
	GtkWidget *reply_popup;
	GtkWidget *replyall_popup;
	GtkWidget *replylist_popup;
	GtkWidget *replysender_popup;
	GtkWidget *fwd_popup;

	GtkTooltips *toolbar_tips;
	ToolbarSylpheedActions *action_item;
	GSList *cur;
	GSList *toolbar_list;
	Toolbar *toolbar_data;

	
 	toolbar_tips = gtk_tooltips_new();
	
	toolbar_read_config_file(type);
	toolbar_list = toolbar_get_list(type);

	toolbar_data = g_new0(Toolbar, 1); 

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				  GTK_TOOLBAR_BOTH);
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_container_set_border_width(GTK_CONTAINER(container), 2);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
	gtk_toolbar_set_space_style(GTK_TOOLBAR(toolbar),
				    GTK_TOOLBAR_SPACE_LINE);
	
	for (cur = toolbar_list; cur != NULL; cur = cur->next) {

		if (g_strcasecmp(((ToolbarItem*)cur->data)->file, TOOLBAR_TAG_SEPARATOR) == 0) {
			gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
			continue;
		}
		
		toolbar_item = g_new0(ToolbarItem, 1); 
		toolbar_item->index = ((ToolbarItem*)cur->data)->index;
		toolbar_item->file = g_strdup(((ToolbarItem*)cur->data)->file);
		toolbar_item->text = g_strdup(((ToolbarItem*)cur->data)->text);
		toolbar_item->parent = data;
		toolbar_item->type = type;

		/* collect toolbar items in list to keep track */
		toolbar_data->item_list = 
			g_slist_append(toolbar_data->item_list, 
				       toolbar_item);

		icon_wid = stock_pixmap_widget(container, stock_pixmap_get_icon(toolbar_item->file));
		item  = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
						toolbar_item->text,
						(""),
						(""),
						icon_wid, toolbar_buttons_cb, 
						toolbar_item);
		
		switch (toolbar_item->index) {

		case A_RECEIVE_ALL:
			toolbar_data->getall_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->getall_btn, 
					   _("Receive Mail on all Accounts"), NULL);
			break;
		case A_RECEIVE_CUR:
			toolbar_data->get_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->get_btn,
					   _("Receive Mail on current Account"), NULL);
			break;
		case A_SEND_QUEUED:
			toolbar_data->send_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->send_btn,
					   _("Send Queued Message(s)"), NULL);
			break;
		case A_COMPOSE_EMAIL:
			icon_news = stock_pixmap_widget(container, STOCK_PIXMAP_NEWS_COMPOSE);
			item_news = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
							    _("News"),
							    (""),
							    (""),
							    icon_news, toolbar_buttons_cb, 
							    toolbar_item);
			toolbar_data->compose_mail_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->compose_mail_btn,
					   _("Compose Email"), NULL);
			toolbar_data->compose_news_btn = item_news;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->compose_news_btn,
					   _("Compose News"), NULL);
			break;
		case A_REPLY_MESSAGE:
			toolbar_data->reply_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->reply_btn,
					   _("Reply to Message"), NULL);
			gtk_signal_connect(GTK_OBJECT(toolbar_data->reply_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_reply_popup_cb),
					   toolbar_data);
			n_menu_entries = sizeof(reply_popup_entries) /
				sizeof(reply_popup_entries[0]);

			window_wid = get_window_widget(type, data);
			reply_popup = popupmenu_create(window_wid,
						       reply_popup_entries, n_menu_entries,
						       "<ReplyPopup>", (gpointer)toolbar_item);

			gtk_signal_connect(GTK_OBJECT(reply_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_reply_popup_closed_cb), toolbar_item);
			toolbar_data->reply_popup = reply_popup;
			break;
		case A_REPLY_SENDER:
			toolbar_data->replysender_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->replysender_btn,
					   _("Reply to Sender"), NULL);
			gtk_signal_connect(GTK_OBJECT(toolbar_data->replysender_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_sender_popup_cb),
					   toolbar_data);
			n_menu_entries = sizeof(replysender_popup_entries) /
				sizeof(replysender_popup_entries[0]);

			window_wid = get_window_widget(type, data);
			replysender_popup = popupmenu_create(window_wid, 
							     replysender_popup_entries, n_menu_entries,
							     "<ReplySenderPopup>", (gpointer)toolbar_item);

			gtk_signal_connect(GTK_OBJECT(replysender_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_sender_popup_closed_cb), toolbar_item);
			toolbar_data->replysender_popup = replysender_popup;
			break;
		case A_REPLY_ALL:
			toolbar_data->replyall_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->replyall_btn,
					   _("Reply to All"), NULL);
			gtk_signal_connect(GTK_OBJECT(toolbar_data->replyall_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_all_popup_cb),
					   toolbar_data);
			n_menu_entries = sizeof(replyall_popup_entries) /
				sizeof(replyall_popup_entries[0]);

			window_wid = get_window_widget(type, data);	
			replyall_popup = popupmenu_create(window_wid, 
							  replyall_popup_entries, n_menu_entries,
							  "<ReplyAllPopup>", (gpointer)toolbar_item);
	
			gtk_signal_connect(GTK_OBJECT(replyall_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_all_popup_closed_cb), toolbar_item);
			toolbar_data->replyall_popup = replyall_popup;
			break;
		case A_REPLY_ML:
			toolbar_data->replylist_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->replylist_btn,
					   _("Reply to Mailing-list"), NULL);
			gtk_signal_connect(GTK_OBJECT(toolbar_data->replylist_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_list_popup_cb),
					   toolbar_data);
			n_menu_entries = sizeof(replylist_popup_entries) /
				sizeof(replylist_popup_entries[0]);

			window_wid = get_window_widget(type, data);
			replylist_popup = popupmenu_create(window_wid, 
							   replylist_popup_entries, n_menu_entries,
							   "<ReplyMlPopup>", (gpointer)toolbar_item);
		
			gtk_signal_connect(GTK_OBJECT(replylist_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_list_popup_closed_cb), toolbar_item);
			toolbar_data->replylist_popup = replylist_popup;
			break;
		case A_FORWARD:
			toolbar_data->fwd_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->fwd_btn,
					     _("Forward Message"), NULL);
			gtk_signal_connect(GTK_OBJECT(toolbar_data->fwd_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_forward_popup_cb),
					   toolbar_data);
			n_menu_entries = sizeof(fwd_popup_entries) /
				sizeof(fwd_popup_entries[0]);

			window_wid = get_window_widget(type, data);
			fwd_popup = popupmenu_create(window_wid, 
						     fwd_popup_entries, n_menu_entries,
						     "<ForwardPopup>", (gpointer)toolbar_item);

			gtk_signal_connect(GTK_OBJECT(fwd_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_forward_popup_closed_cb), toolbar_item);
			toolbar_data->fwd_popup = fwd_popup;
			break;
		case A_DELETE:
			toolbar_data->delete_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->delete_btn,
					     _("Delete Message"), NULL);
			break;
		case A_EXECUTE:
			toolbar_data->exec_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->exec_btn,
					   _("Execute"), NULL);
			break;
		case A_GOTO_NEXT:
			toolbar_data->next_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->next_btn,
					     _("Goto Next Message"), NULL);
			break;
		
		/* Compose Toolbar */
		case A_SEND:
			toolbar_data->send_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->send_btn, 
					     _("Send Message"), NULL);
			break;
		case A_SENDL:
			toolbar_data->sendl_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->sendl_btn,
					     _("Put into queue folder and send later"), NULL);
			break;
		case A_DRAFT:
			toolbar_data->draft_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->draft_btn,
					     _("Save to draft folder"), NULL);
			break;
		case A_INSERT:
			toolbar_data->insert_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->insert_btn,
					     _("Insert file"), NULL);
			break;
		case A_ATTACH:
			toolbar_data->attach_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->attach_btn,
					     _("Attach file"), NULL);
			break;
		case A_SIG:
			toolbar_data->sig_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->sig_btn,
					     _("Insert signature"), NULL);
			break;
		case A_EXTEDITOR:
			toolbar_data->exteditor_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->exteditor_btn,
					     _("Edit with external editor"), NULL);
			break;
		case A_LINEWRAP:
			toolbar_data->linewrap_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->linewrap_btn,
					     _("Wrap all long lines"), NULL);
			break;
		case A_ADDRBOOK:
			toolbar_data->addrbook_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->addrbook_btn,
					     _("Address book"), NULL);
			break;
#ifdef USE_ASPELL
		case A_CHECK_SPELLING:
			toolbar_data->spellcheck_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->spellcheck_btn,
					     _("Check spelling"), NULL);
			break;
#endif

		case A_SYL_ACTIONS:
			action_item = g_new0(ToolbarSylpheedActions, 1);
			action_item->widget = item;
			action_item->name   = g_strdup(toolbar_item->text);

			toolbar_data->action_list = 
				g_slist_append(toolbar_data->action_list,
					       action_item);

			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     item,
					     action_item->name, NULL);

			gtk_widget_show(item);
			break;
		default:
			/* find and set the tool tip text */
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips),
					     item,
					     toolbar_ret_descr_from_val
						(toolbar_item->index),
					     NULL);
			break;
		}

	}
	toolbar_data->toolbar = toolbar;
	if (type == TOOLBAR_MAIN)
		activate_compose_button(toolbar_data, 
					prefs_common.toolbar_style, 
					toolbar_data->compose_btn_type);
	
	gtk_widget_show_all(toolbar);
	
	return toolbar_data; 
}

/**
 * Free toolbar structures
 */ 
void toolbar_destroy(Toolbar * toolbar) {

	TOOLBAR_DESTROY_ITEMS(toolbar->item_list);	
	TOOLBAR_DESTROY_ACTIONS(toolbar->action_list);
}

void toolbar_update(ToolbarType type, gpointer data)
{
	Toolbar *toolbar_data;
	GtkWidget *handlebox;
	MainWindow *mainwin = (MainWindow*)data;
	Compose    *compose = (Compose*)data;
	MessageView *msgview = (MessageView*)data;

	switch(type) {
	case TOOLBAR_MAIN:
		toolbar_data = mainwin->toolbar;
		handlebox    = mainwin->handlebox;
		break;
	case TOOLBAR_COMPOSE:
		toolbar_data = compose->toolbar;
		handlebox    = compose->handlebox;
		break;
	case TOOLBAR_MSGVIEW:
		toolbar_data = msgview->toolbar;
		handlebox    = msgview->handlebox;
		break;
	default:
		return;
	}

	gtk_container_remove(GTK_CONTAINER(handlebox), 
			     GTK_WIDGET(toolbar_data->toolbar));

	toolbar_init(toolbar_data);
 	toolbar_data = toolbar_create(type, handlebox, data);
	switch(type) {
	case TOOLBAR_MAIN:
		mainwin->toolbar = toolbar_data;
		break;
	case TOOLBAR_COMPOSE:
		compose->toolbar = toolbar_data;
		break;
	case TOOLBAR_MSGVIEW:
		msgview->toolbar = toolbar_data;
		break;
	}

	toolbar_style(type, prefs_common.toolbar_style, data);

	if (type == TOOLBAR_MAIN)
		toolbar_main_set_sensitive((MainWindow*)data);
}

void toolbar_main_set_sensitive(gpointer data)
{
	SensitiveCond state;
	gboolean sensitive;
	MainWindow *mainwin = (MainWindow*)data;
	Toolbar *toolbar = mainwin->toolbar;
	GSList *cur;
	GSList *entry_list = NULL;
	
	typedef struct _Entry Entry;
	struct _Entry {
		GtkWidget *widget;
		SensitiveCond cond;
		gboolean empty;
	};

#define SET_WIDGET_COND(w, c)     \
{ \
	Entry *e = g_new0(Entry, 1); \
	e->widget = w; \
	e->cond   = c; \
	entry_list = g_slist_append(entry_list, e); \
}

	SET_WIDGET_COND(toolbar->get_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(toolbar->getall_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(toolbar->compose_news_btn, M_HAVE_ACCOUNT);
	SET_WIDGET_COND(toolbar->reply_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(toolbar->replyall_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(toolbar->replylist_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(toolbar->replysender_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(toolbar->fwd_btn, M_HAVE_ACCOUNT|M_TARGET_EXIST);

	SET_WIDGET_COND(toolbar->next_btn, M_MSG_EXIST);
	SET_WIDGET_COND(toolbar->delete_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE|M_UNLOCKED);
	SET_WIDGET_COND(toolbar->exec_btn, M_DELAY_EXEC);

	for (cur = toolbar->action_list; cur != NULL;  cur = cur->next) {
		ToolbarSylpheedActions *act = (ToolbarSylpheedActions*)cur->data;
		
		SET_WIDGET_COND(act->widget, M_TARGET_EXIST|M_UNLOCKED);
	}

#undef SET_WIDGET_COND

	state = main_window_get_current_state(mainwin);

	for (cur = entry_list; cur != NULL; cur = cur->next) {
		Entry *e = (Entry*) cur->data;

		if (e->widget != NULL) {
			sensitive = ((e->cond & state) == e->cond);
			gtk_widget_set_sensitive(e->widget, sensitive);	
		}
	}
	
	while (entry_list != NULL) {
		Entry *e = (Entry*) entry_list->data;

		if (e)
			g_free(e);
		entry_list = g_slist_remove(entry_list, e);
	}

	g_slist_free(entry_list);

	activate_compose_button(toolbar, 
				prefs_common.toolbar_style,
				toolbar->compose_btn_type);
}

void toolbar_comp_set_sensitive(gpointer data, gboolean sensitive)
{
	Compose *compose = (Compose*)data;
	GSList *items = compose->toolbar->action_list;

	if (compose->toolbar->send_btn)
		gtk_widget_set_sensitive(compose->toolbar->send_btn, sensitive);
	if (compose->toolbar->sendl_btn)
		gtk_widget_set_sensitive(compose->toolbar->sendl_btn, sensitive);
	if (compose->toolbar->draft_btn )
		gtk_widget_set_sensitive(compose->toolbar->draft_btn , sensitive);
	if (compose->toolbar->insert_btn )
		gtk_widget_set_sensitive(compose->toolbar->insert_btn , sensitive);
	if (compose->toolbar->attach_btn)
		gtk_widget_set_sensitive(compose->toolbar->attach_btn, sensitive);
	if (compose->toolbar->sig_btn)
		gtk_widget_set_sensitive(compose->toolbar->sig_btn, sensitive);
	if (compose->toolbar->exteditor_btn)
		gtk_widget_set_sensitive(compose->toolbar->exteditor_btn, sensitive);
	if (compose->toolbar->linewrap_btn)
		gtk_widget_set_sensitive(compose->toolbar->linewrap_btn, sensitive);
	if (compose->toolbar->addrbook_btn)
		gtk_widget_set_sensitive(compose->toolbar->addrbook_btn, sensitive);
#ifdef USE_ASPELL
	if (compose->toolbar->spellcheck_btn)
		gtk_widget_set_sensitive(compose->toolbar->spellcheck_btn, sensitive);
#endif
	for (; items != NULL; items = g_slist_next(items)) {
		ToolbarSylpheedActions *item = (ToolbarSylpheedActions *)items->data;
		gtk_widget_set_sensitive(item->widget, sensitive);
	}
}

/**
 * Initialize toolbar structure
 **/
void toolbar_init(Toolbar * toolbar) {

	toolbar->toolbar          = NULL;
	toolbar->get_btn          = NULL;
	toolbar->getall_btn       = NULL;
	toolbar->send_btn         = NULL;
	toolbar->compose_mail_btn = NULL;
	toolbar->compose_news_btn = NULL;
	toolbar->reply_btn        = NULL;
	toolbar->replysender_btn  = NULL;
	toolbar->replyall_btn     = NULL;
	toolbar->replylist_btn    = NULL;
	toolbar->fwd_btn          = NULL;
	toolbar->delete_btn       = NULL;
	toolbar->next_btn         = NULL;
	toolbar->exec_btn         = NULL;

	/* compose buttons */ 
	toolbar->sendl_btn        = NULL;
	toolbar->draft_btn        = NULL;
	toolbar->insert_btn       = NULL;
	toolbar->attach_btn       = NULL;
	toolbar->sig_btn          = NULL;	
	toolbar->exteditor_btn    = NULL;	
	toolbar->linewrap_btn     = NULL;	
	toolbar->addrbook_btn     = NULL;	
#ifdef USE_ASPELL
	toolbar->spellcheck_btn   = NULL;
#endif

	toolbar_destroy(toolbar);
}

/*
 */
static void toolbar_reply(gpointer data, guint action)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;
	GSList *msginfo_list = NULL;
	gchar *body;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		msginfo_list = summary_get_selection(mainwin->summaryview);
		msgview = (MessageView*)mainwin->messageview;
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		msginfo_list = g_slist_append(msginfo_list, msgview->msginfo);
		break;
	default:
		return;
	}

	g_return_if_fail(msgview != NULL);
	body = messageview_get_selection(msgview);

	g_return_if_fail(msginfo_list != NULL);
	compose_reply_mode((ComposeMode)action, msginfo_list, body);

	g_free(body);
	g_slist_free(msginfo_list);

	/* TODO: update reply state ion summaryview */
}


/* exported functions */

void inc_mail_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;

	inc_mail(mainwin, prefs_common.newmail_notify_manu);
}

void inc_all_account_mail_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;

	inc_all_account_mail(mainwin, FALSE, prefs_common.newmail_notify_manu);
}

void send_queue_cb(gpointer data, guint action, GtkWidget *widget)
{
	GList *list;

	if (prefs_common.work_offline)
		if (alertpanel(_("Offline warning"), 
			       _("You're working offline. Override?"),
			       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
		return;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			procmsg_send_queue(folder->queue, prefs_common.savemsg);
			folder_item_scan(folder->queue);
		}
	}
}

void compose_mail_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;
	PrefsAccount *ac = NULL;
	FolderItem *item = mainwin->summaryview->folder_item;	
        GList * list;
        GList * cur;

	if (item) {
		ac = account_find_from_item(item);
		if (ac && ac->protocol != A_NNTP) {
			compose_new_with_folderitem(ac, item);		/* CLAWS */
			return;
		}
	}

	/*
	 * CLAWS - use current account
	 */
	if (cur_account && (cur_account->protocol != A_NNTP)) {
		compose_new_with_folderitem(cur_account, item);
		return;
	}

	/*
	 * CLAWS - just get the first one
	 */
	list = account_get_list();
	for (cur = list ; cur != NULL ; cur = g_list_next(cur)) {
		ac = (PrefsAccount *) cur->data;
		if (ac->protocol != A_NNTP) {
			compose_new_with_folderitem(ac, item);
			return;
		}
	}
}

void compose_news_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;
	PrefsAccount * ac = NULL;
	GList * list;
	GList * cur;

	if (mainwin->summaryview->folder_item) {
		ac = mainwin->summaryview->folder_item->folder->account;
		if (ac && ac->protocol == A_NNTP) {
			compose_new(ac,
				    mainwin->summaryview->folder_item->path,
				    NULL);
			return;
		}
	}

	list = account_get_list();
	for(cur = list ; cur != NULL ; cur = g_list_next(cur)) {
		ac = (PrefsAccount *) cur->data;
		if (ac->protocol == A_NNTP) {
			compose_new(ac, NULL, NULL);
			return;
		}
	}
}
