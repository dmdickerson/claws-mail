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

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkthemes.h>
#include <gtk/gtkbindings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#include "intl.h"
#include "gtkutils.h"
#include "utils.h"
#include "gtksctree.h"
#include "codeconv.h"
#include "stock_pixmap.h"
#include "menu.h"
#include "prefs_account.h"

gint gtkut_get_font_width(GdkFont *font)
{
	gchar *str;
	gint width;

	if (conv_get_current_charset() == C_UTF_8)
		str = "Abcdef";
	else
		str = _("Abcdef");

	width = gdk_string_width(font, str);
	width /= strlen(str);

	return width;
}

gint gtkut_get_font_height(GdkFont *font)
{
	gchar *str;
	gint height;

	if (conv_get_current_charset() == C_UTF_8)
		str = "Abcdef";
	else
		str = _("Abcdef");

	height = gdk_string_height(font, str);

	return height;
}

GdkFont *gtkut_font_load(const gchar *fontset_name)
{
	GdkFont *font;

	g_return_val_if_fail(fontset_name != NULL, NULL);

	if (conv_get_current_charset() == C_US_ASCII)
		font = gtkut_font_load_from_fontset(fontset_name);
	else
		font = gdk_fontset_load(fontset_name);

	return font;
}

GdkFont *gtkut_font_load_from_fontset(const gchar *fontset_name)
{
	GdkFont *font = NULL;

	if (strchr(fontset_name, ',') == NULL) {
		font = gdk_font_load(fontset_name);
	} else {
		gchar **fonts;
		gint i;

		fonts = g_strsplit(fontset_name, ",", -1);
		for (i = 0; fonts[i] != NULL && fonts[i][0] != '\0';
		     i++) {
			font = gdk_font_load(fonts[i]);
			if (font) break;
		}
		g_strfreev(fonts);
	}

	return font;
}

void gtkut_convert_int_to_gdk_color(gint rgbvalue, GdkColor *color)
{
	g_return_if_fail(color != NULL);

	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0) * 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0) * 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0) * 65535.0);
}

void gtkut_button_set_create(GtkWidget **bbox,
			     GtkWidget **button1, const gchar *label1,
			     GtkWidget **button2, const gchar *label2,
			     GtkWidget **button3, const gchar *label3)
{
	g_return_if_fail(bbox != NULL);
	g_return_if_fail(button1 != NULL);

	*bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(*bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(*bbox), 5);

	*button1 = gtk_button_new_with_label(label1);
	GTK_WIDGET_SET_FLAGS(*button1, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(*bbox), *button1, TRUE, TRUE, 0);
	gtk_widget_show(*button1);

	if (button2) {
		*button2 = gtk_button_new_with_label(label2);
		GTK_WIDGET_SET_FLAGS(*button2, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button2, TRUE, TRUE, 0);
		gtk_widget_show(*button2);
	}

	if (button3) {
		*button3 = gtk_button_new_with_label(label3);
		GTK_WIDGET_SET_FLAGS(*button3, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button3, TRUE, TRUE, 0);
		gtk_widget_show(*button3);
	}
}

#define CELL_SPACING 1
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)
#define ROW_FROM_YPIXEL(clist, y) (((y) - (clist)->voffset) / \
				   ((clist)->row_height + CELL_SPACING))

void gtkut_ctree_node_move_if_on_the_edge(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint row;
	GtkVisibility row_visibility, prev_row_visibility, next_row_visibility;

	g_return_if_fail(ctree != NULL);
	g_return_if_fail(node != NULL);

	row = g_list_position(clist->row_list, (GList *)node);
	if (row < 0 || row >= clist->rows || clist->row_height == 0) return;
	row_visibility = gtk_clist_row_is_visible(clist, row);
	prev_row_visibility = gtk_clist_row_is_visible(clist, row - 1);
	next_row_visibility = gtk_clist_row_is_visible(clist, row + 1);

	if (row_visibility == GTK_VISIBILITY_NONE) {
		gtk_clist_moveto(clist, row, -1, 0.5, 0);
		return;
	}
	if (row_visibility == GTK_VISIBILITY_FULL &&
	    prev_row_visibility == GTK_VISIBILITY_FULL &&
	    next_row_visibility == GTK_VISIBILITY_FULL)
		return;
	if (prev_row_visibility != GTK_VISIBILITY_FULL &&
	    next_row_visibility != GTK_VISIBILITY_FULL)
		return;

	if (prev_row_visibility != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, row, -1, 0.2, 0);
		return;
	}
	if (next_row_visibility != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, row, -1, 0.8, 0);
		return;
	}
}

#undef CELL_SPACING
#undef ROW_TOP_YPIXEL
#undef ROW_FROM_YPIXEL

gint gtkut_ctree_get_nth_from_node(GtkCTree *ctree, GtkCTreeNode *node)
{
	g_return_val_if_fail(ctree != NULL, -1);
	g_return_val_if_fail(node != NULL, -1);

	return g_list_position(GTK_CLIST(ctree)->row_list, (GList *)node);
}

/* get the next node, including the invisible one */
GtkCTreeNode *gtkut_ctree_node_next(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCTreeNode *parent;

	if (!node) return NULL;

	if (GTK_CTREE_ROW(node)->children)
		return GTK_CTREE_ROW(node)->children;

	if (GTK_CTREE_ROW(node)->sibling)
		return GTK_CTREE_ROW(node)->sibling;

	for (parent = GTK_CTREE_ROW(node)->parent; parent != NULL;
	     parent = GTK_CTREE_ROW(parent)->parent) {
		if (GTK_CTREE_ROW(parent)->sibling)
			return GTK_CTREE_ROW(parent)->sibling;
	}

	return NULL;
}

/* get the previous node, including the invisible one */
GtkCTreeNode *gtkut_ctree_node_prev(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCTreeNode *prev;
	GtkCTreeNode *child;

	if (!node) return NULL;

	prev = GTK_CTREE_NODE_PREV(node);
	if (prev == GTK_CTREE_ROW(node)->parent)
		return prev;

	child = prev;
	while (GTK_CTREE_ROW(child)->children != NULL) {
		child = GTK_CTREE_ROW(child)->children;
		while (GTK_CTREE_ROW(child)->sibling != NULL)
			child = GTK_CTREE_ROW(child)->sibling;
	}

	return child;
}

gboolean gtkut_ctree_node_is_selected(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCList *clist = GTK_CLIST(ctree);
	GList *cur;

	for (cur = clist->selection; cur != NULL; cur = cur->next) {
		if (node == GTK_CTREE_NODE(cur->data))
			return TRUE;
	}

	return FALSE;
}

GtkCTreeNode *gtkut_ctree_find_collapsed_parent(GtkCTree *ctree,
						GtkCTreeNode *node)
{
	if (!node) return NULL;

	while ((node = GTK_CTREE_ROW(node)->parent) != NULL) {
		if (!GTK_CTREE_ROW(node)->expanded)
			return node;
	}

	return NULL;
}

void gtkut_ctree_expand_parent_all(GtkCTree *ctree, GtkCTreeNode *node)
{
	while ((node = gtkut_ctree_find_collapsed_parent(ctree, node)) != NULL)
		gtk_ctree_expand(ctree, node);
}

gboolean gtkut_ctree_node_is_parent(GtkCTreeNode *parent, GtkCTreeNode *node)
{
	GtkCTreeNode *tmp;
	g_return_val_if_fail(node != NULL, FALSE);
	g_return_val_if_fail(parent != NULL, FALSE);
	tmp = node;
	
	while (tmp) {
		if(GTK_CTREE_ROW(tmp)->parent && GTK_CTREE_ROW(tmp)->parent == parent)
			return TRUE;
		tmp = GTK_CTREE_ROW(tmp)->parent;
	}
	
	return FALSE;
}

void gtkut_ctree_set_focus_row(GtkCTree *ctree, GtkCTreeNode *node)
{
	gtkut_clist_set_focus_row(GTK_CLIST(ctree),
				  gtkut_ctree_get_nth_from_node(ctree, node));
}

void gtkut_clist_set_focus_row(GtkCList *clist, gint row)
{
	clist->focus_row = row;
	GTKUT_CTREE_REFRESH(clist);
}

void gtkut_combo_set_items(GtkCombo *combo, const gchar *str1, ...)
{
	va_list args;
	gchar *s;
	GList *combo_items = NULL;

	g_return_if_fail(str1 != NULL);

	combo_items = g_list_append(combo_items, (gpointer)str1);
	va_start(args, str1);
	s = va_arg(args, gchar*);
	while (s) {
		combo_items = g_list_append(combo_items, (gpointer)s);
		s = va_arg(args, gchar*);
	}
	va_end(args);

	gtk_combo_set_popdown_strings(combo, combo_items);

	g_list_free(combo_items);
}

gchar *gtkut_editable_get_selection(GtkEditable *editable)
{
	guint start_pos, end_pos;

	g_return_val_if_fail(editable != NULL, NULL);

	if (!editable->has_selection) return NULL;

	if (editable->selection_start_pos == editable->selection_end_pos)
		return NULL;

	if (editable->selection_start_pos < editable->selection_end_pos) {
		start_pos = editable->selection_start_pos;
		end_pos = editable->selection_end_pos;
	} else {
		start_pos = editable->selection_end_pos;
		end_pos = editable->selection_start_pos;
	}

	return gtk_editable_get_chars(editable, start_pos, end_pos);
}

/*
 * Walk through the widget tree and disclaim the selection from all currently
 * realized GtkEditable widgets.
 */
static void gtkut_check_before_remove(GtkWidget *widget, gpointer unused)
{
	g_return_if_fail(widget != NULL);

	if (!GTK_WIDGET_REALIZED(widget))
		return; /* all nested widgets must be unrealized too */
	if (GTK_IS_CONTAINER(widget))
		gtk_container_forall(GTK_CONTAINER(widget),
				     gtkut_check_before_remove, NULL);
	if (GTK_IS_EDITABLE(widget))
		gtk_editable_claim_selection(GTK_EDITABLE(widget),
					     FALSE, GDK_CURRENT_TIME);
}

/*
 * Wrapper around gtk_container_remove to work around a bug in GtkText and
 * GtkEntry (in all GTK+ versions up to and including at least 1.2.10).
 *
 * The problem is that unrealizing a GtkText or GtkEntry widget which has the
 * active selection completely messes up selection handling, leading to
 * non-working selections and crashes.  Removing a widget from its container
 * implies unrealizing it and all its child widgets; this triggers the bug if
 * the removed widget or any of its children is GtkText or GtkEntry.  As a
 * workaround, this function walks through the widget subtree before removing
 * and disclaims the selection from all GtkEditable widgets found.
 *
 * A similar workaround may be needed for gtk_widget_reparent(); currently it
 * is not necessary because Sylpheed does not use gtk_widget_reparent() for
 * GtkEditable widgets or containers holding such widgets.
 */
void gtkut_container_remove(GtkContainer *container, GtkWidget *widget)
{
	gtkut_check_before_remove(widget, NULL);
	gtk_container_remove(container, widget);
}

void gtkut_window_popup(GtkWidget *window)
{
	gint x, y, sx, sy, new_x, new_y;

	g_return_if_fail(window != NULL);
	g_return_if_fail(window->window != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	gdk_window_get_origin(window->window, &x, &y);
	new_x = x % sx; if (new_x < 0) new_x = 0;
	new_y = y % sy; if (new_y < 0) new_y = 0;
	if (new_x != x || new_y != y)
		gdk_window_move(window->window, new_x, new_y);

	gdk_window_raise(window->window);
	gdk_window_show(window->window);
}

void gtkut_widget_get_uposition(GtkWidget *widget, gint *px, gint *py)
{
	gint x, y;
	gint sx, sy;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(widget->window != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	/* gdk_window_get_root_origin ever return *rootwindow*'s position */
	gdk_window_get_root_origin(widget->window, &x, &y);

	x %= sx; if (x < 0) x = 0;
	y %= sy; if (y < 0) y = 0;
	*px = x;
	*py = y;
}

void gtkut_widget_disable_theme_engine(GtkWidget *widget)
{
	GtkStyle *style, *new_style;

	style = gtk_widget_get_style(widget);

	if (style->engine) {
		GtkThemeEngine *engine;

		engine = style->engine;
		style->engine = NULL;
		new_style = gtk_style_copy(style);
		style->engine = engine;
		gtk_widget_set_style(widget, new_style);
	}
}

static void gtkut_widget_draw_cb(GtkWidget *widget, GdkRectangle *area,
				 gboolean *flag)
{
	*flag = TRUE;
	gtk_signal_disconnect_by_data(GTK_OBJECT(widget), flag);
}

void gtkut_widget_wait_for_draw(GtkWidget *widget)
{
	gboolean flag = FALSE;

	if (!GTK_WIDGET_VISIBLE(widget) || !GTK_WIDGET_MAPPED(widget)) return;

	gtk_signal_connect(GTK_OBJECT(widget), "draw",
			   GTK_SIGNAL_FUNC(gtkut_widget_draw_cb), &flag);
	while (!flag)
		gtk_main_iteration();
}

static void gtkut_clist_bindings_add(GtkWidget *clist)
{
	GtkBindingSet *binding_set;

	binding_set = gtk_binding_set_by_class
		(GTK_CLIST_CLASS(GTK_OBJECT(clist)->klass));

	gtk_binding_entry_add_signal(binding_set, GDK_n, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				     GTK_TYPE_FLOAT, 0.0);
	gtk_binding_entry_add_signal(binding_set, GDK_p, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				     GTK_TYPE_FLOAT, 0.0);
}

void gtkut_widget_init(void)
{
	GtkWidget *clist;

	clist = gtk_clist_new(1);
	gtkut_clist_bindings_add(clist);
	gtk_widget_unref(clist);

	clist = gtk_ctree_new(1, 0);
	gtkut_clist_bindings_add(clist);
	gtk_widget_unref(clist);

	clist = gtk_sctree_new_with_titles(1, 0, NULL);
	gtkut_clist_bindings_add(clist);
	gtk_widget_unref(clist);
}

void gtkut_widget_set_app_icon(GtkWidget *widget)
{
#include "pixmaps/sylpheed.xpm"	
	static GdkPixmap *sylpheedxpm;
	static GdkBitmap *sylpheedxpmmask;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(widget->window != NULL);
	if (!sylpheedxpm) {
		PIXMAP_CREATE(widget, sylpheedxpm, sylpheedxpmmask, sylpheed_xpm);
	}		
	gdk_window_set_icon(widget->window, NULL, sylpheedxpm, sylpheedxpmmask);
}

void gtkut_widget_set_composer_icon(GtkWidget *widget)
{
	static GdkPixmap *xpm;
	static GdkBitmap *bmp;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(widget->window != NULL);
	if (!xpm) {
		stock_pixmap_gdk(widget, STOCK_PIXMAP_MAIL_COMPOSE, &xpm, &bmp);
	}
	gdk_window_set_icon(widget->window, NULL, xpm, bmp);	
}

GtkWidget *gtkut_account_menu_new(GList			*ac_list,
				  GtkSignalFunc		 callback,
				  gpointer		 data)
{
	GList *cur_ac;
	GtkWidget *menu;
	
	g_return_val_if_fail(ac_list != NULL, NULL);

	menu = gtk_menu_new();

	for (cur_ac = ac_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		gchar *name;
		GtkWidget *menuitem;
		PrefsAccount *account;
		
		account = (PrefsAccount *) cur_ac->data;
		if (account->name)
			name = g_strdup_printf("%s: %s <%s>",
					       account->account_name,
					       account->name,
					       account->address);
		else
			name = g_strdup_printf("%s: %s",
					       account->account_name,
					       account->address);
		MENUITEM_ADD(menu, menuitem, name, account->account_id);
		g_free(name);
		if (callback != NULL)
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   callback,
					   data);
	}
	return menu;
}
