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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtktext.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkselection.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkbutton.h>
#include <stdio.h>
#ifdef WIN32
# include <w32lib.h>
# include "w32_mailcap.h"
#endif

#ifndef HAVE_APACHE_FNMATCH
/* kludge: apache's fnmatch clashes with <regex.h>, don't include
 * fnmatch.h */
#include <fnmatch.h>
#endif

#include "intl.h"
#include "main.h"
#include "mimeview.h"
#include "textview.h"
#include "procmime.h"
#include "summaryview.h"
#include "menu.h"
#include "filesel.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "rfc2015.h"
#include "stock_pixmap.h"
#include "gtk/gtkvscrollbutton.h"


typedef enum
{
	COL_MIMETYPE = 0,
	COL_SIZE     = 1,
	COL_NAME     = 2
} MimeViewColumnPos;

#define N_MIMEVIEW_COLS	3

static void mimeview_set_multipart_tree		(MimeView	*mimeview,
						 MimeInfo	*mimeinfo,
						 GtkCTreeNode	*parent);
static GtkCTreeNode *mimeview_append_part	(MimeView	*mimeview,
						 MimeInfo	*partinfo,
						 GtkCTreeNode	*parent);
static void mimeview_show_message_part		(MimeView	*mimeview,
						 MimeInfo	*partinfo);
static void mimeview_change_view_type		(MimeView	*mimeview,
						 MimeViewType	 type);

static void mimeview_selected		(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 gint		 column,
					 MimeView	*mimeview);
static void mimeview_start_drag 	(GtkWidget	*widget,
					 gint		 button,
					 GdkEvent	*event,
					 MimeView	*mimeview);
static gint mimeview_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 MimeView	*mimeview);
static gint mimeview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MimeView	*mimeview);

static void mimeview_drag_data_get      (GtkWidget	  *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint		   info,
					 guint		   time,
					 MimeView	  *mimeview);

static void mimeview_display_as_text	(MimeView	*mimeview);
static void mimeview_save_as		(MimeView	*mimeview);
static void mimeview_save_all		(MimeView	*mimeview);
static void mimeview_launch		(MimeView	*mimeview);
static void mimeview_open_with		(MimeView	*mimeview);
static void mimeview_view_file		(const gchar	*filename,
					 MimeInfo	*partinfo,
					 const gchar	*cmdline);
static gboolean icon_clicked_cb		(GtkWidget 	*button, 
					 GdkEventButton	*event, 
					 MimeView 	*mimeview);
static void icon_selected               (MimeView       *mimeview, 
					 gint            num, 
					 MimeInfo       *partinfo);
static gint icon_key_pressed            (GtkWidget      *button, 
					 GdkEventKey    *event,
					 MimeView       *mimeview);
static void toggle_icon			(GtkToggleButton *button,
					 MimeView	*mimeview);
static void icon_list_append_icon 	(MimeView 	*mimeview, 
					 MimeInfo 	*mimeinfo);
static void icon_list_create		(MimeView 	*mimeview, 
					 MimeInfo 	*mimeinfo);
static void icon_list_clear		(MimeView	*mimeview);
static void icon_list_toggle_by_mime_info (MimeView	*mimeview,
					   MimeInfo	*mimeinfo);
static gboolean icon_list_select_by_number(MimeView	*mimeview,
					   gint		 number);
static void mime_toggle_button_cb 	(GtkWidget 	*button,
					 MimeView 	*mimeview);
static void part_button_pressed		(MimeView 	*mimeview, 
					 GdkEventButton *event, 
					 MimeInfo 	*partinfo);
static void icon_scroll_size_allocate_cb(GtkWidget 	*widget, 
					 GtkAllocation  *layout_size, 
					 MimeView 	*mimeview);

static GtkItemFactoryEntry mimeview_popup_entries[] =
{
	{N_("/_Open"),		  NULL, mimeview_launch,	  0, NULL},
	{N_("/Open _with..."),	  NULL, mimeview_open_with,	  0, NULL},
	{N_("/_Display as text"), NULL, mimeview_display_as_text, 0, NULL},
	{N_("/_Save as..."),	  NULL, mimeview_save_as,	  0, NULL},
	{N_("/Save _all..."),	  NULL, mimeview_save_all,	  0, NULL},
};

static GtkTargetEntry mimeview_mime_types[] =
{
	{"text/uri-list", 0, 0}
};

GSList *mimeviewer_factories;
GSList *mimeviews;

MimeView *mimeview_create(MainWindow *mainwin)
{
	MimeView *mimeview;

	GtkWidget *paned;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *mime_notebook;
	GtkWidget *popupmenu;
	GtkWidget *ctree_mainbox;
	GtkWidget *vbox;
	GtkWidget *mime_toggle;
	GtkWidget *icon_mainbox;
	GtkWidget *icon_scroll;
	GtkWidget *icon_vbox;
	GtkWidget *arrow;
	GtkWidget *scrollbutton;
	GtkWidget *hbox;
	GtkTooltips *tooltips;
	GtkItemFactory *popupfactory;
	NoticeView *siginfoview;
	gchar *titles[N_MIMEVIEW_COLS];
	gint n_entries;
	gint i;

	debug_print("Creating MIME view...\n");
	mimeview = g_new0(MimeView, 1);

	titles[COL_MIMETYPE] = _("MIME Type");
	titles[COL_SIZE]     = _("Size");
	titles[COL_NAME]     = _("Name");

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	ctree = gtk_sctree_new_with_titles(N_MIMEVIEW_COLS, 0, titles);
	gtk_widget_show(ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_NONE);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), COL_SIZE,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_MIMETYPE, 240);
	gtk_clist_set_column_width(GTK_CLIST(ctree), COL_SIZE, 90);
	for (i = 0; i < N_MIMEVIEW_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[i].button,
				       GTK_CAN_FOCUS);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);

	gtk_signal_connect(GTK_OBJECT(ctree), "tree_select_row",
			   GTK_SIGNAL_FUNC(mimeview_selected), mimeview);
	gtk_signal_connect(GTK_OBJECT(ctree), "button_press_event",
			   GTK_SIGNAL_FUNC(mimeview_button_pressed), mimeview);
	gtk_signal_connect(GTK_OBJECT(ctree), "key_press_event",
			   GTK_SIGNAL_FUNC(mimeview_key_pressed), mimeview);
	gtk_signal_connect(GTK_OBJECT (ctree),"start_drag",
			   GTK_SIGNAL_FUNC (mimeview_start_drag), mimeview);
	gtk_signal_connect(GTK_OBJECT(ctree), "drag_data_get",
			   GTK_SIGNAL_FUNC(mimeview_drag_data_get), mimeview);

	mime_notebook = gtk_notebook_new();
        gtk_widget_show(mime_notebook);
        GTK_WIDGET_UNSET_FLAGS(mime_notebook, GTK_CAN_FOCUS);
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(mime_notebook), FALSE);
        gtk_notebook_set_show_border(GTK_NOTEBOOK(mime_notebook), FALSE);
	
	icon_vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(icon_vbox);
	icon_scroll = gtk_layout_new(NULL, NULL);
	gtk_widget_show(icon_scroll);
	gtk_layout_put(GTK_LAYOUT(icon_scroll), icon_vbox, 0, 0);
	scrollbutton = gtk_vscrollbutton_new(gtk_layout_get_vadjustment(GTK_LAYOUT(icon_scroll)));
	gtk_widget_show(scrollbutton);

    	mime_toggle = gtk_toggle_button_new();
	gtk_widget_show(mime_toggle);
	arrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_NONE);
	gtk_widget_show(arrow);
	gtk_container_add(GTK_CONTAINER(mime_toggle), arrow);
	gtk_signal_connect(GTK_OBJECT(mime_toggle), "toggled", 
			   GTK_SIGNAL_FUNC(mime_toggle_button_cb), mimeview);

	icon_mainbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(icon_mainbox);
	gtk_box_pack_start(GTK_BOX(icon_mainbox), mime_toggle, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(icon_mainbox), icon_scroll, TRUE, TRUE, 3);
	gtk_box_pack_end(GTK_BOX(icon_mainbox), scrollbutton, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(icon_mainbox), "size_allocate", 
			GTK_SIGNAL_FUNC(icon_scroll_size_allocate_cb), mimeview);
	
	ctree_mainbox = gtk_hbox_new(FALSE, 0);	
	gtk_box_pack_start(GTK_BOX(ctree_mainbox), scrolledwin, TRUE, TRUE, 0);

 	n_entries = sizeof(mimeview_popup_entries) /
 		sizeof(mimeview_popup_entries[0]);
 	popupmenu = menu_create_items(mimeview_popup_entries, n_entries,
 				      "<MimeView>", &popupfactory, mimeview);
	tooltips = gtk_tooltips_new();
	gtk_tooltips_set_delay(tooltips, 0); 

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	siginfoview = noticeview_create(mainwin);
	noticeview_hide(siginfoview);
	gtk_box_pack_start(GTK_BOX(vbox), mime_notebook, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET_PTR(siginfoview), FALSE, FALSE, 0);

	paned = gtk_vpaned_new();
	gtk_widget_show(paned);
	gtk_paned_set_gutter_size(GTK_PANED(paned), 0);
	gtk_paned_pack1(GTK_PANED(paned), ctree_mainbox, FALSE, TRUE);
	gtk_paned_pack2(GTK_PANED(paned), vbox, TRUE, TRUE);
	
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), paned, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), icon_mainbox, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_widget_hide(ctree_mainbox);

	mimeview->hbox          = hbox;
	mimeview->paned         = paned;
	mimeview->scrolledwin   = scrolledwin;
	mimeview->ctree         = ctree;
	mimeview->mime_notebook = mime_notebook;
	mimeview->popupmenu     = popupmenu;
	mimeview->popupfactory  = popupfactory;
	mimeview->type          = -1;
	mimeview->ctree_mainbox = ctree_mainbox;
	mimeview->icon_scroll   = icon_scroll;
	mimeview->icon_vbox     = icon_vbox;
	mimeview->icon_mainbox  = icon_mainbox;
	mimeview->icon_count    = 0;
	mimeview->mainwin       = mainwin;
	mimeview->tooltips      = tooltips;
	mimeview->oldsize       = 60;
	mimeview->mime_toggle   = mime_toggle;
	mimeview->siginfoview	= siginfoview;

	mimeview->target_list	= gtk_target_list_new(mimeview_mime_types, 1); 
	
	mimeviews = g_slist_prepend(mimeviews, mimeview);

	return mimeview;
}

void mimeview_init(MimeView *mimeview)
{
	textview_init(mimeview->textview);

	gtk_container_add(GTK_CONTAINER(mimeview->mime_notebook),
		GTK_WIDGET_PTR(mimeview->textview));
}

void mimeview_show_message(MimeView *mimeview, MimeInfo *mimeinfo,
			   const gchar *file)
{
	GtkCTree *ctree = GTK_CTREE(mimeview->ctree);
	GtkCTreeNode *node;

	mimeview_clear(mimeview);

	g_return_if_fail(file != NULL);
	g_return_if_fail(mimeinfo != NULL);

	mimeview->mimeinfo = mimeinfo;

	mimeview->file = g_strdup(file);

	gtk_signal_handler_block_by_func(GTK_OBJECT(ctree), mimeview_selected,
					 mimeview);

	mimeview_set_multipart_tree(mimeview, mimeinfo, NULL);
	icon_list_create(mimeview, mimeinfo);

	gtk_signal_handler_unblock_by_func(GTK_OBJECT(ctree),
					   mimeview_selected, mimeview);

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	if (node) {
		gtk_ctree_select(ctree, node);
		icon_list_toggle_by_mime_info
			(mimeview, gtk_ctree_node_get_row_data(ctree, node));
		gtkut_ctree_set_focus_row(ctree, node);
	}
}

void mimeview_destroy(MimeView *mimeview)
{
	GSList *cur;
	
	for (cur = mimeview->viewers; cur != NULL; cur = g_slist_next(cur)) {
		MimeViewer *viewer = (MimeViewer *) cur->data;
		gtk_container_remove(GTK_CONTAINER(mimeview->mime_notebook),
			GTK_WIDGET(viewer->get_widget(viewer)));
		viewer->destroy_viewer(viewer);
	}
	g_slist_free(mimeview->viewers);
	gtk_target_list_unref(mimeview->target_list);

	procmime_mimeinfo_free_all(mimeview->mimeinfo);
	g_free(mimeview->file);
	g_free(mimeview);

	mimeviews = g_slist_remove(mimeviews, mimeview);
	
}

MimeInfo *mimeview_get_selected_part(MimeView *mimeview)
{
	return gtk_ctree_node_get_row_data
		(GTK_CTREE(mimeview->ctree), mimeview->opened);
}

static void mimeview_set_multipart_tree(MimeView *mimeview,
					MimeInfo *mimeinfo,
					GtkCTreeNode *parent)
{
	GtkCTreeNode *node;

	g_return_if_fail(mimeinfo != NULL);

	while (mimeinfo != NULL) {
		node = mimeview_append_part(mimeview, mimeinfo, parent);

		if (mimeinfo->node->children)
			mimeview_set_multipart_tree(mimeview, (MimeInfo *) mimeinfo->node->children->data, node);
		mimeinfo = mimeinfo->node->next != NULL ? (MimeInfo *) mimeinfo->node->next->data : NULL;
	}
}

static gchar *get_part_name(MimeInfo *partinfo)
{
	gchar *name;

	name = g_hash_table_lookup(partinfo->parameters, "name");
	if(name == NULL)
		name = "";

	return name;
}

static gchar *get_part_description(MimeInfo *partinfo)
{
	if (partinfo->description)
		return partinfo->description;
	else if (g_hash_table_lookup(partinfo->parameters, "name") != NULL)
		return g_hash_table_lookup(partinfo->parameters, "name");
	else
		return "";
}

static GtkCTreeNode *mimeview_append_part(MimeView *mimeview,
					  MimeInfo *partinfo,
					  GtkCTreeNode *parent)
{
	GtkCTree *ctree = GTK_CTREE(mimeview->ctree);
	GtkCTreeNode *node;
	static gchar content_type[64];
	gchar *str[N_MIMEVIEW_COLS];

	if (partinfo->type != MIMETYPE_UNKNOWN && partinfo->subtype) {
		g_snprintf(content_type, 64, "%s/%s", procmime_get_type_str(partinfo->type), partinfo->subtype);
	} else {
		g_snprintf(content_type, 64, "UNKNOWN");
	}

	str[COL_MIMETYPE] = content_type;
	str[COL_SIZE] = to_human_readable(partinfo->length);
#ifdef WIN32
	{
	 	gchar *g_name;
		if (prefs_common.attach_desc)
			g_name = g_strdup( get_part_description(partinfo) );
		else
			g_name = g_strdup( get_part_name(partinfo) );
		locale_to_utf8(&g_name);
		str[COL_NAME] = g_name;
	}
#else
	if (prefs_common.attach_desc)
		str[COL_NAME] = get_part_description(partinfo);
	else
		str[COL_NAME] = get_part_name(partinfo);
#endif

	node = gtk_ctree_insert_node(ctree, parent, NULL, str, 0,
				     NULL, NULL, NULL, NULL,
				     FALSE, TRUE);
	gtk_ctree_node_set_row_data(ctree, node, partinfo);

	return node;
}

static void mimeview_show_message_part(MimeView *mimeview, MimeInfo *partinfo)
{
	FILE *fp;
	const gchar *fname;

	if (!partinfo) return;

	fname = mimeview->file;
	if (!fname) return;

	if ((fp = fopen(fname, "rb")) == NULL) {
		FILE_OP_ERROR(fname, "fopen");
		return;
	}

	if (fseek(fp, partinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeview->file, "fseek");
		fclose(fp);
		return;
	}

	mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
	textview_show_part(mimeview->textview, partinfo, fp);

	fclose(fp);
}

static MimeViewer *get_viewer_for_content_type(MimeView *mimeview, const gchar *content_type)
{
	GSList *cur;
	MimeViewerFactory *factory = NULL;
	MimeViewer *viewer = NULL;

/*
 * FNM_CASEFOLD is a GNU extension
 * if its not defined copy the string to the stack and
 * convert the copy to lower case
 */
#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0
	Xstrdup_a(content_type, content_type, return NULL);
	g_strdown((gchar *)content_type);
#endif
	
	for (cur = mimeviewer_factories; cur != NULL; cur = g_slist_next(cur)) {
		MimeViewerFactory *curfactory = cur->data;
		gint i = 0;

		while (curfactory->content_types[i] != NULL) {
			if(!fnmatch(curfactory->content_types[i], content_type, FNM_CASEFOLD)) {
				debug_print("%s\n", curfactory->content_types[i]);
				factory = curfactory;
				break;
			}
			i++;
		}
		if (factory != NULL)
			break;
	}
	if (factory == NULL)
		return NULL;

	for (cur = mimeview->viewers; cur != NULL; cur = g_slist_next(cur)) {
		MimeViewer *curviewer = cur->data;
		
		if (curviewer->factory == factory)
			return curviewer;
	}
	viewer = factory->create_viewer();
	gtk_container_add(GTK_CONTAINER(mimeview->mime_notebook),
		GTK_WIDGET(viewer->get_widget(viewer)));
		
	mimeview->viewers = g_slist_append(mimeview->viewers, viewer);

	return viewer;
}

static MimeViewer *get_viewer_for_mimeinfo(MimeView *mimeview, MimeInfo *partinfo)
{
	gchar *content_type = NULL;
	MimeViewer *viewer = NULL;

	if ((partinfo->type == MIMETYPE_APPLICATION) &&
            (!g_strcasecmp(partinfo->subtype, "octet-stream"))) {
		const gchar *filename;

		filename = procmime_mimeinfo_get_parameter(partinfo, "name");
		if (filename != NULL)
			content_type = procmime_get_mime_type(filename);
	} else {
		content_type = g_strdup_printf("%s/%s", procmime_get_type_str(partinfo->type), partinfo->subtype);
	}

	if (content_type != NULL) {
		viewer = get_viewer_for_content_type(mimeview, content_type);
		g_free(content_type);
	}

	return viewer;
}

static gboolean mimeview_show_part(MimeView *mimeview, MimeInfo *partinfo)
{
	MimeViewer *viewer;
	
	viewer = get_viewer_for_mimeinfo(mimeview, partinfo);
	if (viewer == NULL) {
		if (mimeview->mimeviewer != NULL)
			mimeview->mimeviewer->clear_viewer(mimeview->mimeviewer);
		mimeview->mimeviewer = NULL;
		return FALSE;
	}

	if (mimeview->mimeviewer != viewer) {
		if (mimeview->mimeviewer != NULL)
			mimeview->mimeviewer->clear_viewer(mimeview->mimeviewer);
		mimeview->mimeviewer = viewer;
		mimeview_change_view_type(mimeview, MIMEVIEW_VIEWER);
	}
	viewer->show_mimepart(viewer, mimeview->file, partinfo);

	return TRUE;
}

static void mimeview_change_view_type(MimeView *mimeview, MimeViewType type)
{
	TextView  *textview  = mimeview->textview;

	if ((mimeview->type != MIMEVIEW_VIEWER) && 
	    (mimeview->type == type)) return;

	switch (type) {
	case MIMEVIEW_TEXT:
		gtk_notebook_set_page(GTK_NOTEBOOK(mimeview->mime_notebook),
			gtk_notebook_page_num(GTK_NOTEBOOK(mimeview->mime_notebook), 
			GTK_WIDGET_PTR(textview)));
		break;
	case MIMEVIEW_VIEWER:
		gtk_notebook_set_page(GTK_NOTEBOOK(mimeview->mime_notebook),
			gtk_notebook_page_num(GTK_NOTEBOOK(mimeview->mime_notebook), 
			GTK_WIDGET(mimeview->mimeviewer->get_widget(mimeview->mimeviewer))));
		break;
	default:
		return;
	}

	mimeview->type = type;
}

void mimeview_clear(MimeView *mimeview)
{
	GtkCList *clist = GTK_CLIST(mimeview->ctree);
	GtkAdjustment *adj;

	noticeview_hide(mimeview->siginfoview);

	gtk_clist_clear(clist);
	textview_clear(mimeview->textview);
	if (mimeview->mimeviewer != NULL)
		mimeview->mimeviewer->clear_viewer(mimeview->mimeviewer);

	if (mimeview->mimeinfo != NULL)
		procmime_mimeinfo_free_all(mimeview->mimeinfo);
	mimeview->mimeinfo = NULL;

	mimeview->opened = NULL;

	g_free(mimeview->file);
	mimeview->file = NULL;

	icon_list_clear(mimeview);
}

static void check_signature_cb(GtkWidget *widget, gpointer user_data);
static void display_full_info_cb(GtkWidget *widget, gpointer user_data);

static void update_signature_noticeview(MimeView *mimeview, MimeInfo *mimeinfo)
{
	gchar *text = NULL, *button_text = NULL;
	GtkSignalFunc func = NULL;
	StockPixmap icon = STOCK_PIXMAP_PRIVACY_SIGNED;

	g_return_if_fail(mimeview != NULL);
	g_return_if_fail(mimeinfo != NULL);
	
	switch (privacy_mimeinfo_get_sig_status(mimeinfo)) {
	case SIGNATURE_UNCHECKED:
		button_text = _("Check");
		func = check_signature_cb;
		icon = STOCK_PIXMAP_PRIVACY_SIGNED;
		break;
	case SIGNATURE_OK:
		button_text = _("Full info");
		func = display_full_info_cb;
		icon = STOCK_PIXMAP_PRIVACY_PASSED;
		break;
	case SIGNATURE_WARN:
		button_text = _("Full info");
		func = display_full_info_cb;
		icon = STOCK_PIXMAP_PRIVACY_WARN;
		break;
	case SIGNATURE_INVALID:
		button_text = _("Full info");
		func = display_full_info_cb;
		icon = STOCK_PIXMAP_PRIVACY_FAILED;
		break;
	case SIGNATURE_CHECK_FAILED:
		button_text = _("Check again");
		func = check_signature_cb;
		icon = STOCK_PIXMAP_PRIVACY_UNKNOWN;
	default:
		break;
	}
	text = privacy_mimeinfo_sig_info_short(mimeinfo);
	noticeview_set_text(mimeview->siginfoview, text);
	g_free(text);
	noticeview_set_button_text(mimeview->siginfoview, button_text);
	noticeview_set_button_press_callback(
		mimeview->siginfoview,
		func,
		(gpointer) mimeview);
	noticeview_set_icon(mimeview->siginfoview, icon);
}

static void check_signature_cb(GtkWidget *widget, gpointer user_data)
{
	MimeView *mimeview = (MimeView *) user_data;
	MimeInfo *mimeinfo = mimeview->siginfo;
	
	privacy_mimeinfo_check_signature(mimeinfo);
	update_signature_noticeview(mimeview, mimeview->siginfo);
}

static void display_full_info_cb(GtkWidget *widget, gpointer user_data)
{
	MimeView *mimeview = (MimeView *) user_data;
	gchar *siginfo;

	siginfo = privacy_mimeinfo_sig_info_full(mimeview->siginfo);
	textview_set_text(mimeview->textview, siginfo);
	g_free(siginfo);
	noticeview_set_button_text(mimeview->siginfoview, NULL);
}

static void update_signature_info(MimeView *mimeview, MimeInfo *selected)
{
	MimeInfo *siginfo;

	g_return_if_fail(mimeview != NULL);
	g_return_if_fail(selected != NULL);
	
	siginfo = selected;
	while (siginfo != NULL) {
		if (privacy_mimeinfo_is_signed(siginfo))
			break;
		siginfo = procmime_mimeinfo_parent(siginfo);
	}
	mimeview->siginfo = siginfo;
	
	if (siginfo == NULL) {
		noticeview_hide(mimeview->siginfoview);
		return;
	}
	
	update_signature_noticeview(mimeview, siginfo);
	noticeview_show(mimeview->siginfoview);
}

static void mimeview_selected(GtkCTree *ctree, GtkCTreeNode *node, gint column,
			      MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (mimeview->opened == node) return;
	mimeview->opened = node;
	gtk_ctree_node_moveto(ctree, node, -1, 0.5, 0);

	partinfo = gtk_ctree_node_get_row_data(ctree, node);
	if (!partinfo) return;

	/* ungrab the mouse event */
	if (GTK_WIDGET_HAS_GRAB(ctree)) {
		gtk_grab_remove(GTK_WIDGET(ctree));
		if (gdk_pointer_is_grabbed())
			gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}
	
	mimeview->textview->default_text = FALSE;

	update_signature_info(mimeview, partinfo);

	if (!mimeview_show_part(mimeview, partinfo)) {
		switch (partinfo->type) {
		case MIMETYPE_TEXT:
		case MIMETYPE_MESSAGE:
		case MIMETYPE_MULTIPART:
			mimeview_show_message_part(mimeview, partinfo);
		
			break;
		default:
			mimeview->textview->default_text = TRUE;	
			mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
			textview_show_mime_part(mimeview->textview, partinfo);
			break;
		}
	}
}

static void mimeview_start_drag(GtkWidget *widget, gint button,
				GdkEvent *event, MimeView *mimeview)
{
	GdkDragContext *context;
	MimeInfo *partinfo;

	g_return_if_fail(mimeview != NULL);

	partinfo = mimeview_get_selected_part(mimeview);
	if (partinfo->filename == NULL && partinfo->name == NULL) return;

	context = gtk_drag_begin(widget, mimeview->target_list,
				 GDK_ACTION_COPY, button, event);
	gtk_drag_set_icon_default(context);
}

static gint mimeview_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    MimeView *mimeview)
{
	GtkCList *clist = GTK_CLIST(widget);
	gint row, column;

	if (!event) return FALSE;

	if (event->button == 2 || event->button == 3) {
		if (!gtk_clist_get_selection_info(clist, event->x, event->y,
						  &row, &column))
			return FALSE;
		gtk_clist_unselect_all(clist);
		gtk_clist_select_row(clist, row, column);
		gtkut_clist_set_focus_row(clist, row);
	}
	part_button_pressed(mimeview, event, mimeview_get_selected_part(mimeview));

	return TRUE;
}

static void part_button_pressed(MimeView *mimeview, GdkEventButton *event, 
				MimeInfo *partinfo)
{
	if (event->button == 2 ||
	    (event->button == 1 && event->type == GDK_2BUTTON_PRESS)) {
		/* call external program for image, audio or html */
		mimeview_launch(mimeview);
	} else if (event->button == 3) {
		if (partinfo && (partinfo->type == MIMETYPE_TEXT ||
				 partinfo->type == MIMETYPE_MESSAGE ||
				 partinfo->type == MIMETYPE_IMAGE ||
				 partinfo->type == MIMETYPE_MULTIPART))
			menu_set_sensitive(mimeview->popupfactory,
					   "/Display as text", FALSE);
		else
			menu_set_sensitive(mimeview->popupfactory,
					   "/Display as text", TRUE);
		if (partinfo &&
		    partinfo->type == MIMETYPE_APPLICATION &&
		    !g_strcasecmp(partinfo->subtype, "octet-stream"))
			menu_set_sensitive(mimeview->popupfactory,
					   "/Open", FALSE);
		else
			menu_set_sensitive(mimeview->popupfactory,
					   "/Open", TRUE);

		gtk_object_set_data(GTK_OBJECT(mimeview->popupmenu),
				    "pop_partinfo", partinfo);
				    
		gtk_menu_popup(GTK_MENU(mimeview->popupmenu),
			       NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}
}


void mimeview_pass_key_press_event(MimeView *mimeview, GdkEventKey *event)
{
	mimeview_key_pressed(mimeview->ctree, event, mimeview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

#define KEY_PRESS_EVENT_STOP() \
	if (gtk_signal_n_emissions_by_name \
		(GTK_OBJECT(ctree), "key_press_event") > 0) { \
		gtk_signal_emit_stop_by_name(GTK_OBJECT(ctree), \
					     "key_press_event"); \
	}

static gint mimeview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				 MimeView *mimeview)
{
	SummaryView *summaryview;
	GtkCTree *ctree = GTK_CTREE(widget);
	GtkCTreeNode *node;

	if (!event) return FALSE;
	if (!mimeview->opened) return FALSE;

	switch (event->keyval) {
	case GDK_space:
		if (textview_scroll_page(mimeview->textview, FALSE))
			return TRUE;

		node = GTK_CTREE_NODE_NEXT(mimeview->opened);
		if (node) {
			gtk_sctree_unselect_all(GTK_SCTREE(ctree));
			gtk_sctree_select(GTK_SCTREE(ctree), node);
			return TRUE;
		}
		break;
	case GDK_BackSpace:
		textview_scroll_page(mimeview->textview, TRUE);
		return TRUE;
	case GDK_Return:
		textview_scroll_one_line(mimeview->textview,
					 (event->state & GDK_MOD1_MASK) != 0);
		return TRUE;
	case GDK_n:
	case GDK_N:
		BREAK_ON_MODIFIER_KEY();
		if (!GTK_CTREE_NODE_NEXT(mimeview->opened)) break;
		KEY_PRESS_EVENT_STOP();

		gtk_signal_emit_by_name(GTK_OBJECT(ctree), "scroll_vertical",
					GTK_SCROLL_STEP_FORWARD, 0.0);
		return TRUE;
	case GDK_p:
	case GDK_P:
		BREAK_ON_MODIFIER_KEY();
		if (!GTK_CTREE_NODE_PREV(mimeview->opened)) break;
		KEY_PRESS_EVENT_STOP();

		gtk_signal_emit_by_name(GTK_OBJECT(ctree), "scroll_vertical",
					GTK_SCROLL_STEP_BACKWARD, 0.0);
		return TRUE;
	case GDK_y:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_save_as(mimeview);
		return TRUE;
	case GDK_t:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_display_as_text(mimeview);
		return TRUE;	
	case GDK_l:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_launch(mimeview);
		return TRUE;
	case GDK_o:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_open_with(mimeview);
		return TRUE;
	default:
		break;
	}

	if (!mimeview->messageview->mainwin) return FALSE;
	summaryview = mimeview->messageview->mainwin->summaryview;
	summary_pass_key_press_event(summaryview, event);
	return TRUE;
}

static void mimeview_drag_data_get(GtkWidget	    *widget,
				   GdkDragContext   *drag_context,
				   GtkSelectionData *selection_data,
				   guint	     info,
				   guint	     time,
				   MimeView	    *mimeview)
{
	gchar *filename, *uriname;
	MimeInfo *partinfo;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) return;
	if (!partinfo->filename && !partinfo->name) return;

	filename = partinfo->filename ? partinfo->filename : partinfo->name;
	filename = g_basename(filename);
	if (*filename == '\0') return;

	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       filename, NULL);

	if (procmime_get_part(filename, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));

	uriname = g_strconcat("file:/", filename, NULL);
	gtk_selection_data_set(selection_data, selection_data->target, 8,
			       uriname, strlen(uriname));

	g_free(uriname);
	g_free(filename);
}

static void mimeview_save_all(MimeView *mimeview)
{
	gchar *dirname;
	gchar *defname = NULL;
	MimeInfo *attachment;
	gchar buf[1024];

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	attachment = mimeview->mimeinfo;
	g_return_if_fail(attachment != NULL);

	dirname = filesel_select_file(_("Save as"), defname);
	if (!dirname) return;

	if (!is_dir_exist (dirname)) {
		alertpanel_error(_("`%s' is not a directory."),
				 dirname);
		g_free (dirname);
		dirname = NULL;
		return;
	}
	
	{ /* add a / after the dirname, in case the user didn't */
		gchar *dirname_tmp = NULL;
		int dirname_last_char = strlen (dirname) - 1;

		if (dirname[dirname_last_char] != G_DIR_SEPARATOR) {
			dirname_tmp = g_strconcat (dirname, G_DIR_SEPARATOR_S, NULL);
			g_free (dirname);
			dirname = dirname_tmp;
		}
	}

	/* for each attachment, extract it in the selected dir. */
	while (attachment != NULL) {
		if (attachment->type != MIMETYPE_MESSAGE &&
		    attachment->type != MIMETYPE_MULTIPART &&
		    (procmime_mimeinfo_get_parameter(attachment, "name") ||
		     procmime_mimeinfo_get_parameter(attachment, "filename"))) {
			static guint subst_cnt = 1;
			gchar *attachdir;
			gchar *attachname = g_strdup(get_part_name(attachment));
			AlertValue aval = G_ALERTDEFAULT;
			gchar *res;

			if (!attachname || !strlen(attachname))
				attachname = g_strdup_printf("noname.%d",subst_cnt++);
			subst_chars(attachname, ":?*&|<>\t\r\n", '_');
			g_snprintf(buf, sizeof(buf), "%s%s",
				   dirname,
				   (attachname[0] == G_DIR_SEPARATOR)
				   ? &attachname[1]
				   : attachname);
			subst_chars(buf, "/\\", G_DIR_SEPARATOR);
			attachdir = g_dirname(buf);
			make_dir_hier(attachdir);
			g_free(attachdir);
			
			if (is_file_exist(buf)) {
				res = g_strdup_printf(_("Overwrite existing file '%s'?"),
						      attachname);
				aval = alertpanel(_("Overwrite"), res, _("OK"), 
						  _("Cancel"), NULL);
				g_free(res);					  
			}
			g_free(attachname);
			
			if ((G_ALERTDEFAULT != aval) || (procmime_get_part(buf, attachment) < 0))
				alertpanel_error(_("Can't save the part of multipart message."));
		}
		attachment = procmime_mimeinfo_next(attachment);
	}
}

static void mimeview_display_as_text(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo)  {
		partinfo = (MimeInfo *) gtk_object_get_data
			(GTK_OBJECT(mimeview->popupmenu),
			 "pop_partinfo");
		gtk_object_set_data(GTK_OBJECT(mimeview->popupmenu),
				    "pop_partinfo", NULL);
	
	}			 
	g_return_if_fail(partinfo != NULL);
	mimeview_show_message_part(mimeview, partinfo);
}

static void mimeview_save_as(MimeView *mimeview)
{
	gchar *filename;
	gchar *defname = NULL;
	MimeInfo *partinfo;
	gchar *res;
	const gchar *partname = NULL;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) { 
		partinfo = (MimeInfo *) gtk_object_get_data
			(GTK_OBJECT(mimeview->popupmenu),
			 "pop_partinfo");
		gtk_object_set_data(GTK_OBJECT(mimeview->popupmenu),
				    "pop_partinfo", NULL);
	}			 
	g_return_if_fail(partinfo != NULL);
	
	if ((partname = procmime_mimeinfo_get_parameter(partinfo, "name")) != NULL) {
		Xstrdup_a(defname, partname, return);
		subst_for_filename(defname);
	}

#ifdef WIN32
	{
		gchar *p_defname = g_strdup(defname);
		locale_to_utf8(&p_defname);
		filename = filesel_select_file(_("Save as"), p_defname);
		g_free(p_defname); 
	}
#else

	filename = filesel_select_file(_("Save as"), defname);
#endif
	if (!filename) return;
	if (is_file_exist(filename)) {
		AlertValue aval;
		res = g_strdup_printf(_("Overwrite existing file '%s'?"),
				      filename);
		aval = alertpanel(_("Overwrite"), res, _("OK"), 
				  _("Cancel"), NULL);
		g_free(res);					  
		if (G_ALERTDEFAULT != aval) return;
	}

	if (procmime_get_part(filename, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));
}

#ifdef WIN32
static gchar *w32_get_open_cmd(filename)
{
	gchar *open_cmd;
	if ( (open_cmd=g_strdup(w32_mailcap_lookup(filename)))==NULL) {
		open_cmd = g_malloc(MAX_PATH);
		FindExecutable(filename, NULL, open_cmd);
		if (!(open_cmd && *open_cmd)){
			g_free(open_cmd);
			open_cmd = NULL;
		} else {
			gchar *p = g_strdup(open_cmd);
			open_cmd = g_strconcat("\"", p, "\"", " \"%s\"", NULL);
			g_free(p);
		}
	}
	return open_cmd;
}
#endif

static void mimeview_launch(MimeView *mimeview)
{
	MimeInfo *partinfo;
	gchar *filename;
#ifdef WIN32
	gchar *open_cmd;
#endif

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) { 
		partinfo = (MimeInfo *) gtk_object_get_data
			(GTK_OBJECT(mimeview->popupmenu),
			 "pop_partinfo");
		gtk_object_set_data(GTK_OBJECT(mimeview->popupmenu),
				    "pop_partinfo", NULL);
	}			 
	g_return_if_fail(partinfo != NULL);

	filename = procmime_get_tmp_file_name(partinfo);

	if (procmime_get_part(filename, partinfo) < 0)
		alertpanel_error
			(_("Can't save the part of multipart message."));
	else
#ifdef WIN32
	{
		gchar *new_name = g_strdup(w32_move_to_exec_dir(filename));
		g_free(filename);
		filename = new_name;
	}
#else
		mimeview_view_file(filename, partinfo, NULL);
#endif

	g_free(filename);
}

static void mimeview_open_with(MimeView *mimeview)
{
	MimeInfo *partinfo;
	gchar *filename;
	gchar *cmd;
#ifdef WIN32
	gchar *open_cmd;
	gchar *newname;
#endif

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) { 
		partinfo = (MimeInfo *) gtk_object_get_data
			(GTK_OBJECT(mimeview->popupmenu),
			 "pop_partinfo");
		gtk_object_set_data(GTK_OBJECT(mimeview->popupmenu),
				    "pop_partinfo", NULL);
	}			 
	g_return_if_fail(partinfo != NULL);

	filename = procmime_get_tmp_file_name(partinfo);

	if (procmime_get_part(filename, partinfo) < 0) {
		alertpanel_error
			(_("Can't save the part of multipart message."));
		g_free(filename);
		return;
	}

	if (!prefs_common.mime_open_cmd_history)
		prefs_common.mime_open_cmd_history =
			add_history(NULL, prefs_common.mime_open_cmd);

#ifdef WIN32
	newname = g_strdup(w32_move_to_exec_dir(filename));
	g_free(filename);
	filename = newname;
	open_cmd = w32_get_open_cmd(filename);
#endif

	cmd = input_dialog_combo
		(_("Open with"),
		 _("Enter the command line to open file:\n"
		   "(`%s' will be replaced with file name)"),
#ifdef WIN32
		 open_cmd,
#else
		 prefs_common.mime_open_cmd,
#endif
		 prefs_common.mime_open_cmd_history,
		 TRUE);
	if (cmd) {
		mimeview_view_file(filename, partinfo, cmd);
		g_free(prefs_common.mime_open_cmd);
		prefs_common.mime_open_cmd = cmd;
		prefs_common.mime_open_cmd_history =
			add_history(prefs_common.mime_open_cmd_history, cmd);
	}

#ifdef WIN32
	g_free(open_cmd);
#endif
	g_free(filename);
}

static void mimeview_view_file(const gchar *filename, MimeInfo *partinfo,
			       const gchar *cmdline)
{
	static gchar *default_image_cmdline = "display '%s'";
	static gchar *default_audio_cmdline = "play '%s'";
	static gchar *default_html_cmdline = DEFAULT_BROWSER_CMD;
	static gchar *mime_cmdline = "metamail -d -b -x -c %s '%s'";
	gchar buf[1024];
	gchar m_buf[1024];
	const gchar *cmd;
	const gchar *def_cmd;
	const gchar *p;

	if (cmdline) {
		cmd = cmdline;
		def_cmd = NULL;
	} else if (MIMETYPE_APPLICATION == partinfo->type &&
		   !g_strcasecmp(partinfo->subtype, "octet-stream")) {
		return;
	} else if (MIMETYPE_IMAGE == partinfo->type) {
		cmd = prefs_common.mime_image_viewer;
		def_cmd = default_image_cmdline;
	} else if (MIMETYPE_AUDIO == partinfo->type) {
		cmd = prefs_common.mime_audio_player;
		def_cmd = default_audio_cmdline;
	} else if (MIMETYPE_TEXT == partinfo->type && !strcmp(partinfo->subtype, "html")) {
		cmd = prefs_common.uri_cmd;
		def_cmd = default_html_cmdline;
	} else {
		gchar *content_type;
		
		content_type = g_strdup_printf("%s/%s", procmime_get_type_str(partinfo->type), partinfo->subtype);
		g_snprintf(m_buf, sizeof(m_buf), mime_cmdline,
			   content_type, "%s");
		g_free(content_type);
		cmd = m_buf;
		def_cmd = NULL;
	}

	if (cmd && (p = strchr(cmd, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf), cmd, filename);
	else {
		if (cmd)
			g_warning("MIME viewer command line is invalid: `%s'", cmd);
		if (def_cmd)
			g_snprintf(buf, sizeof(buf), def_cmd, filename);
		else
			return;
	}

	execute_command_line(buf, TRUE);
}

void mimeview_register_viewer_factory(MimeViewerFactory *factory)
{
	mimeviewer_factories = g_slist_append(mimeviewer_factories, factory);
}

static gint cmp_viewer_by_factroy(gconstpointer a, gconstpointer b)
{
	return ((MimeViewer *) a)->factory == (MimeViewerFactory *) b ? 0 : -1;
}

void mimeview_unregister_viewer_factory(MimeViewerFactory *factory)
{
	GSList *mimeview_list, *viewer_list;

	for (mimeview_list = mimeviews; mimeview_list != NULL; mimeview_list = g_slist_next(mimeview_list)) {
		MimeView *mimeview = (MimeView *) mimeview_list->data;
		
		if (mimeview->mimeviewer && mimeview->mimeviewer->factory == factory) {
			mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
			mimeview->mimeviewer = NULL;
		}

		while ((viewer_list = g_slist_find_custom(mimeview->viewers, factory, cmp_viewer_by_factroy)) != NULL) {
			MimeViewer *mimeviewer = (MimeViewer *) viewer_list->data;

			mimeviewer->destroy_viewer(mimeviewer);
			mimeview->viewers = g_slist_remove(mimeview->viewers, mimeviewer);
		}
	}

	mimeviewer_factories = g_slist_remove(mimeviewer_factories, factory);
}

static gboolean icon_clicked_cb (GtkWidget *button, GdkEventButton *event, MimeView *mimeview)
{
	gint      num;
	MimeInfo *partinfo;

	num      = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(button), "icon_number"));
	partinfo = gtk_object_get_data(GTK_OBJECT(button), "partinfo");

	icon_selected(mimeview, num, partinfo);
	gtk_widget_grab_focus(button);
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
		toggle_icon(GTK_TOGGLE_BUTTON(button), mimeview);
		if (event->button == 2 || event->button == 3)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
						     TRUE);
	} else {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(button), "button_press_event");
	}

	part_button_pressed(mimeview, event, partinfo);

	return TRUE;
}

static void icon_selected (MimeView *mimeview, gint num, MimeInfo *partinfo)
{
	GtkCTreeNode *node;
	node = gtk_ctree_find_by_row_data(GTK_CTREE(mimeview->ctree), NULL, partinfo);
	if (node)
		gtk_ctree_select(GTK_CTREE(mimeview->ctree), node);
}		

#undef  KEY_PRESS_EVENT_STOP
#define KEY_PRESS_EVENT_STOP() \
	if (gtk_signal_n_emissions_by_name \
		(GTK_OBJECT(button), "key_press_event") > 0) { \
		gtk_signal_emit_stop_by_name(GTK_OBJECT(button), \
					     "key_press_event"); \
	}
static gint icon_key_pressed(GtkWidget *button, GdkEventKey *event,
			     MimeView *mimeview)
{
	gint          num;
	MimeInfo     *partinfo;
	SummaryView  *summaryview;
	TextView     *textview;

	num      = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(button), "icon_number"));
	partinfo = gtk_object_get_data(GTK_OBJECT(button), "partinfo");
	
	if (!event) return FALSE;

	textview = mimeview->textview;

	switch (event->keyval) {
	case GDK_space:
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
			/* stop the button being untoggled */
			KEY_PRESS_EVENT_STOP();
			if (textview_scroll_page(textview, FALSE))
				return TRUE;

			if (icon_list_select_by_number(mimeview, num + 1))
				return TRUE;
		} else {
			icon_selected(mimeview, num, partinfo);
			toggle_icon(GTK_TOGGLE_BUTTON(button), mimeview);
			return TRUE;
		}

		break;
	case GDK_BackSpace:
		textview_scroll_page(textview, TRUE);
		return TRUE;
	case GDK_Return:
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
			KEY_PRESS_EVENT_STOP();
			textview_scroll_one_line(textview,
						 (event->state & GDK_MOD1_MASK) != 0);
			return TRUE;
		} else {
			icon_selected(mimeview, num, partinfo);
			toggle_icon(GTK_TOGGLE_BUTTON(button), mimeview);
			return TRUE;
		}

	case GDK_n:
	case GDK_N:
		BREAK_ON_MODIFIER_KEY();
		if (icon_list_select_by_number(mimeview, num + 1)) {
			KEY_PRESS_EVENT_STOP();
			return TRUE;
		}
		break;
		
	case GDK_p:
	case GDK_P:
		BREAK_ON_MODIFIER_KEY();
		if (icon_list_select_by_number(mimeview, num - 1)) {
			KEY_PRESS_EVENT_STOP();
			return TRUE;
		}
		break;

	case GDK_y:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_save_as(mimeview);
		return TRUE;
	case GDK_t:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_display_as_text(mimeview);
		return TRUE;	
	case GDK_l:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_launch(mimeview);
		return TRUE;
	case GDK_o:
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		mimeview_open_with(mimeview);
		return TRUE;
	default:
		break;
	}

	if (!mimeview->messageview->mainwin) return FALSE;
	summaryview = mimeview->messageview->mainwin->summaryview;
	summary_pass_key_press_event(summaryview, event);
	return TRUE;
}

static void toggle_icon(GtkToggleButton *button, MimeView *mimeview)
{
	GList *child;
	
	child = gtk_container_children(GTK_CONTAINER(mimeview->icon_vbox));
	for (; child != NULL; child = g_list_next(child)) {
		if (GTK_IS_TOGGLE_BUTTON(child->data) && 
		    GTK_TOGGLE_BUTTON(child->data) != button &&
		    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(child->data)))
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(child->data),
				 FALSE);
	}
}

static void icon_list_append_icon (MimeView *mimeview, MimeInfo *mimeinfo) 
{
	GtkWidget *pixmap;
	GtkWidget *vbox;
	GtkWidget *button;
	gchar *tip;
	gchar *desc = NULL;
	StockPixmap stockp;
	
	vbox = mimeview->icon_vbox;
	mimeview->icon_count++;
	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_object_set_data(GTK_OBJECT(button), "icon_number", 
		GINT_TO_POINTER(mimeview->icon_count));
	gtk_object_set_data(GTK_OBJECT(button), "partinfo", 
		mimeinfo);
	
	switch (mimeinfo->type) {
		
	case MIMETYPE_TEXT:
		if (mimeinfo->subtype && !g_strcasecmp(mimeinfo->subtype, "html"))
			stockp = STOCK_PIXMAP_MIME_TEXT_HTML;
		else if  (mimeinfo->subtype && !g_strcasecmp(mimeinfo->subtype, "enriched"))
			stockp = STOCK_PIXMAP_MIME_TEXT_ENRICHED;
		else
			stockp = STOCK_PIXMAP_MIME_TEXT_PLAIN;
		break;
	case MIMETYPE_MESSAGE:
		stockp = STOCK_PIXMAP_MIME_MESSAGE;
		break;
	case MIMETYPE_APPLICATION:
		stockp = STOCK_PIXMAP_MIME_APPLICATION;
		break;
	case MIMETYPE_IMAGE:
		stockp = STOCK_PIXMAP_MIME_IMAGE;
		break;
	case MIMETYPE_AUDIO:
		stockp = STOCK_PIXMAP_MIME_AUDIO;
		break;
	default:
		stockp = STOCK_PIXMAP_MIME_UNKNOWN;
		break;
	}
	
	pixmap = stock_pixmap_widget(mimeview->mainwin->window, stockp);
	gtk_container_add(GTK_CONTAINER(button), pixmap);
	
	if (!desc) {
		if (prefs_common.attach_desc)
			desc = get_part_description(mimeinfo);
		else
			desc = get_part_name(mimeinfo);
	}

	if (desc && *desc)
		tip = g_strdup_printf("%s\n%s/%s\n%s", desc,
				procmime_get_type_str(mimeinfo->type),
				mimeinfo->subtype, 
				to_human_readable(mimeinfo->length));
	else 		
		tip = g_strdup_printf("%s/%s\n%s",
				procmime_get_type_str(mimeinfo->type),
				mimeinfo->subtype, 
				to_human_readable(mimeinfo->length));

#ifdef WIN32
	if (tip && *tip)
		locale_to_utf8(&tip);
#endif
	gtk_tooltips_set_tip(mimeview->tooltips, button, tip, NULL);
	g_free(tip);
	gtk_widget_show_all(button);
	gtk_signal_connect(GTK_OBJECT(button), "button_press_event", 
			   GTK_SIGNAL_FUNC(icon_clicked_cb), mimeview);
	gtk_signal_connect(GTK_OBJECT(button), "key_press_event", 
			   GTK_SIGNAL_FUNC(icon_key_pressed), mimeview);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

}

static void icon_list_clear (MimeView *mimeview)
{
	GList     *child;
	GtkAdjustment *adj;
	
	child = gtk_container_children(GTK_CONTAINER(mimeview->icon_vbox));
	for (; child != NULL; child = g_list_next(child)) {
		gtkut_container_remove(GTK_CONTAINER(mimeview->icon_vbox), 
				       GTK_WIDGET(child->data));
	}
	mimeview->icon_count = 0;
	adj  = gtk_layout_get_vadjustment(GTK_LAYOUT(mimeview->icon_scroll));
	adj->value = adj->lower;
	gtk_signal_emit_by_name(GTK_OBJECT (adj), "value_changed");
}

static void icon_list_toggle_by_mime_info(MimeView	*mimeview,
					  MimeInfo	*mimeinfo)
{
	GList *child;
	
	child = gtk_container_children(GTK_CONTAINER(mimeview->icon_vbox));
	for (; child != NULL; child = g_list_next(child)) {
		if (GTK_IS_TOGGLE_BUTTON(child->data) &&  
		    gtk_object_get_data(GTK_OBJECT(child->data),
					"partinfo") == (gpointer)mimeinfo) {
			toggle_icon(GTK_TOGGLE_BUTTON(child->data), mimeview);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(child->data), TRUE);
		}				 
	}
}

/*!
 *\brief        Used to 'click' the next or previous icon.
 *
 *\return       true if the icon 'number' exists and was selected.
 */
static gboolean icon_list_select_by_number(MimeView	*mimeview,
					   gint		 number)
{
	GList *child;

	if (number == 0) return FALSE;
	child = gtk_container_children(GTK_CONTAINER(mimeview->icon_vbox));
	for (; child != NULL; child = g_list_next(child)) {
		if (GTK_IS_TOGGLE_BUTTON(child->data) &&  
		    GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(child->data),
					"icon_number")) == number) {
			icon_selected(mimeview, number,
				      (MimeInfo*)gtk_object_get_data(GTK_OBJECT(child->data),
								     "partinfo"));
			toggle_icon(GTK_TOGGLE_BUTTON(child->data), mimeview);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(child->data), TRUE);
			gtk_widget_grab_focus(GTK_WIDGET(child->data));
		
			return TRUE;
		}				 
	}
	return FALSE;
}

static void icon_scroll_size_allocate_cb(GtkWidget *widget, 
					GtkAllocation *size, MimeView *mimeview)
{
	GtkAllocation *mainbox_size;
	GtkAllocation *vbox_size;
	GtkAllocation *layout_size;
	GtkAdjustment *adj;
	
	adj = gtk_layout_get_vadjustment(GTK_LAYOUT(mimeview->icon_scroll));

	mainbox_size = &mimeview->icon_mainbox->allocation;
	vbox_size = &mimeview->icon_vbox->allocation;
	layout_size = &mimeview->icon_scroll->allocation;
	
	/* centralise the vbox */
	gtk_layout_move(GTK_LAYOUT(mimeview->icon_scroll), mimeview->icon_vbox, 
			(mainbox_size->width - vbox_size->width)/2, 0);
	
	gtk_layout_set_size(GTK_LAYOUT(mimeview->icon_scroll), 
			    GTK_LAYOUT(mimeview->icon_scroll)->width, 
			    MAX(vbox_size->height, layout_size->height));
	adj->step_increment = 5;
}

static void icon_list_create(MimeView *mimeview, MimeInfo *mimeinfo)
{
	GtkRequisition size;
	gint           width;
	g_return_if_fail(mimeinfo != NULL);

	while (mimeinfo != NULL) {
		if (mimeinfo->type != MIMETYPE_MULTIPART)
			icon_list_append_icon(mimeview, mimeinfo);
		if (mimeinfo->node->children != NULL)
			icon_list_create(mimeview, (MimeInfo *) mimeinfo->node->children->data);
		mimeinfo = mimeinfo->node->next != NULL ? (MimeInfo *) mimeinfo->node->next->data : NULL;
	}
	gtk_widget_size_request(mimeview->icon_vbox, &size);
	width = size.width + 4;
	if (width > mimeview->icon_mainbox->requisition.width) {
		gtk_widget_set_usize(mimeview->icon_mainbox, 
				     width, -1);
	}

}

static void mime_toggle_button_cb (GtkWidget *button, MimeView *mimeview) 
{
	gtk_widget_ref(button); 

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
		gtk_arrow_set(GTK_ARROW(GTK_BIN(button)->child), GTK_ARROW_RIGHT, 
					GTK_SHADOW_NONE);
		gtk_widget_hide(mimeview->icon_mainbox);
		gtk_widget_show(mimeview->ctree_mainbox);
		gtk_paned_set_position(GTK_PANED(mimeview->paned), mimeview->oldsize);

		gtkut_container_remove(GTK_CONTAINER(mimeview->icon_mainbox), 
					button);
		gtk_box_pack_end(GTK_BOX(mimeview->ctree_mainbox), 
				   button, FALSE, FALSE, 0);
		gtk_paned_set_gutter_size(GTK_PANED(mimeview->paned), 6);
	} else {
		gtk_arrow_set(GTK_ARROW(GTK_BIN(button)->child), GTK_ARROW_LEFT, 
			      GTK_SHADOW_NONE);
		mimeview->oldsize = mimeview->ctree_mainbox->allocation.height;
		gtk_widget_hide(mimeview->ctree_mainbox);
		gtk_widget_show(mimeview->icon_mainbox);
		gtk_paned_set_position(GTK_PANED(mimeview->paned), 0);

		gtkut_container_remove(GTK_CONTAINER(mimeview->ctree_mainbox), 
					button);
		gtk_box_pack_start(GTK_BOX(mimeview->icon_mainbox), 
				   button, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(button->parent), button, 0);
		if (mimeview->opened)
			icon_list_toggle_by_mime_info
				(mimeview, gtk_ctree_node_get_row_data(GTK_CTREE(mimeview->ctree), 
								       mimeview->opened));

		gtk_paned_set_gutter_size(GTK_PANED(mimeview->paned), 0);
	}
	gtk_widget_grab_focus(button);
	gtk_widget_unref(button);

}

void mimeview_update (MimeView *mimeview) {
	if (mimeview && mimeview->mimeinfo) {
		icon_list_clear(mimeview);
		icon_list_create(mimeview, mimeview->mimeinfo);
	}
}
