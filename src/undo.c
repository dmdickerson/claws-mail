/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

/* code ported from gedit */
/* This is for my patient girlfirend Regina */

#include <config.h>
#include <string.h> /* For strlen */

#include "undo.h"
#include "utils.h"
#include "gtkstext.h"

typedef struct _UndoInfo UndoInfo;

struct _UndoInfo 
{
        UndoAction action;
        gchar *text;
        gint start_pos;
        gint end_pos;
        gfloat window_position;
        gint mergeable;
};

static void undo_free_list	(GList **list_pointer);
static void undo_check_size	(UndoMain *undostruct);
static gint undo_merge		(GList *list, guint start_pos,
		                 guint end_pos, gint action, 
				 const guchar *text);
static void undo_add		(const gchar *text, gint start_pos, gint end_pos,
                                 UndoAction action, UndoMain *undostruct);
static gint undo_get_selection	(GtkEditable *text, guint *start, guint *end);
static void undo_insert_text_cb (GtkEditable *editable, gchar *new_text,
				 gint new_text_length, gint *position, 
				 UndoMain *undostruct);
static void undo_delete_text_cb (GtkEditable *editable, gint start_pos,
				 gint end_pos, UndoMain *undostruct);
static void undo_paste_clipboard_cb (GtkEditable *editable, UndoMain *undostruct);
void undo_undo			(UndoMain *undostruct);
void undo_redo			(UndoMain *undostruct);


UndoMain *undo_init (GtkWidget *text) 
{
	UndoMain *undostruct;
	
	g_return_if_fail(text != NULL);

	undostruct = g_new(UndoMain, 1);
	undostruct->text = text;
	undostruct->undo = NULL;
	undostruct->redo = NULL;
	undostruct->paste = 0;
	undostruct->undo_state = FALSE;
	undostruct->redo_state = FALSE;
	debug_print("undostruct: %x\n", &undostruct);

	gtk_signal_connect(GTK_OBJECT(text), "insert-text",
			   GTK_SIGNAL_FUNC(undo_insert_text_cb), undostruct);
	gtk_signal_connect(GTK_OBJECT(text), "delete-text",
			   GTK_SIGNAL_FUNC(undo_delete_text_cb), undostruct);
	gtk_signal_connect(GTK_OBJECT(text), "paste-clipboard",
			   GTK_SIGNAL_FUNC(undo_paste_clipboard_cb), undostruct);

	return undostruct;
}

void undo_destroy (UndoMain *undostruct) 
{
	undo_free_list(&undostruct->undo);
	undo_free_list(&undostruct->redo);
	g_free(undostruct);
}

static UndoInfo *undo_object_new(gchar *text, gint start_pos, gint end_pos, 
				 UndoAction action, gfloat window_position) 
{
	UndoInfo *undoinfo;
	undoinfo = g_new (UndoInfo, 1);
        undoinfo->text      = text;
        undoinfo->start_pos = start_pos;
        undoinfo->end_pos   = end_pos;
        undoinfo->action    = action;
	undoinfo->window_position = window_position;
	return undoinfo;
}

static void undo_object_free(UndoInfo *undo) 
{
	g_free (undo->text);
	g_free (undo);
}

/**
 * undo_free_list:
 * @list_pointer: list to be freed
 *
 * frees and undo structure list
 **/
static void undo_free_list(GList **list_pointer) 
{
	gint n;
	UndoInfo *nth_redo;
        GList *cur, *list = *list_pointer;

	if (list == NULL)
		return;

	debug_print("length of list: %d\n", g_list_length(list));

	for (cur = list; cur != NULL; cur = cur->next) {
		nth_redo = cur->data;
		undo_object_free(nth_redo);
	}

        g_list_free(list);
	*list_pointer = NULL;
}

void undo_set_undo_change_funct(UndoMain *undostruct, UndoChangeState func, 
				GtkWidget *changewidget) 
{
        g_return_if_fail(undostruct != NULL);
        undostruct->change_func = func;
	undostruct->changewidget = changewidget;
}

/**
 * undo_check_size:
 * @compose: document to check
 *
 * Checks that the size of compose->undo does not excede settings->undo_levels and
 * frees any undo level above sett->undo_level.
 *
 **/
static void undo_check_size(UndoMain *undostruct) 
{
        gint n;
        UndoInfo *nth_undo;
	gint undo_levels = 50;

        if (undo_levels < 1)
                return;

        /* No need to check for the redo list size since the undo
           list gets freed on any call to compose_undo_add */
        if (g_list_length(undostruct->undo) >= undo_levels && undo_levels > 0) {
		nth_undo = g_list_nth_data(undostruct->undo, g_list_length(undostruct->undo) - 1);
		undostruct->undo = g_list_remove(undostruct->undo, nth_undo);
		g_free (nth_undo->text);
		g_free (nth_undo);
        }
	debug_print("g_list_length (undostruct->undo): %d\n", g_list_length(undostruct->undo));
}

/**
 * undo_merge:
 * @last_undo:
 * @start_pos:
 * @end_pos:
 * @action:
 *
 * This function tries to merge the undo object at the top of
 * the stack with a new set of data. So when we undo for example
 * typing, we can undo the whole word and not each letter by itself
 *
 * Return Value: TRUE is merge was sucessful, FALSE otherwise
 **/
static gint undo_merge (GList *list, guint start_pos, guint end_pos, gint action, const guchar* text) 
{
        guchar *temp_string;
        UndoInfo *last_undo;
	gboolean checkit = TRUE;

        /* This are the cases in which we will NOT merge :
           1. if (last_undo->mergeable == FALSE)
           [mergeable = FALSE when the size of the undo data was not 1.
           or if the data was size = 1 but = '\n' or if the undo object
           has been "undone" already ]
           2. The size of text is not 1
           3. If the new merging data is a '\n'
           4. If the last char of the undo_last data is a space/tab
           and the new char is not a space/tab ( so that we undo
           words and not chars )
           5. If the type (action) of undo is different from the last one
        Chema */

        if (list == NULL)
                return FALSE;

        last_undo = list->data;

        if (!last_undo->mergeable)
                return FALSE;

        if (end_pos-start_pos != 1) {
                last_undo->mergeable = FALSE;
                return FALSE;
        }

        if (text[0] == '\n') 
		checkit = FALSE;

        if (action != last_undo->action) 
		checkit = FALSE;

        if (action == UNDO_ACTION_REPLACE_INSERT || action == UNDO_ACTION_REPLACE_DELETE)
                 checkit = FALSE;

	if (action == UNDO_ACTION_DELETE && checkit) {
                if (last_undo->start_pos!=end_pos && last_undo->start_pos != start_pos && checkit)
                         checkit = FALSE;

                if (last_undo->start_pos == start_pos && checkit) {
                        /* Deleted with the delete key */
                        if ( text[0] != ' ' && text[0] != '\t' && checkit &&
                             (last_undo->text[last_undo->end_pos-last_undo->start_pos - 1] == ' '
                              || last_undo->text[last_undo->end_pos-last_undo->start_pos - 1] == '\t'))
                                 checkit = FALSE;

                        temp_string = g_strdup_printf("%s%s", last_undo->text, text);
                        g_free(last_undo->text);
                        last_undo->end_pos += 1;
                        last_undo->text = temp_string;
                }
                else if (checkit) {
                        /* Deleted with the backspace key */
                        if ( text[0] != ' ' && text[0] != '\t' && checkit &&
                             (last_undo->text[0] == ' '
                              || last_undo->text[0] == '\t'))
                                 checkit = FALSE;

                        temp_string = g_strdup_printf("%s%s", text, last_undo->text);
                        g_free(last_undo->text);
                        last_undo->start_pos = start_pos;
                        last_undo->text = temp_string;
                }
        }
        else if (action == UNDO_ACTION_INSERT && checkit) {
                if (last_undo->end_pos != start_pos && checkit)
                         checkit = FALSE;

/*                if ( text[0]!=' ' && text[0]!='\t' &&
                     (last_undo->text [last_undo->end_pos-last_undo->start_pos - 1] ==' '
                      || last_undo->text [last_undo->end_pos-last_undo->start_pos - 1] == '\t'))
                        goto compose_undo_do_not_merge;
*/
                if (checkit) {
			temp_string = g_strdup_printf("%s%s", last_undo->text, text);
                	g_free(last_undo->text);
                	last_undo->end_pos = end_pos;
                	last_undo->text = temp_string;
		}
        }
        else if (checkit)
                debug_print("Unknown action [%i] inside undo merge encountered", action);

	if (checkit) {
		debug_print("Merged: %s\n", text);
        	return TRUE;
	}
	else {
		last_undo->mergeable = FALSE;
		return FALSE;
	}
}

/**
 * compose_undo_add:
 * @text:
 * @start_pos:
 * @end_pos:
 * @action: either UNDO_ACTION_INSERT or UNDO_ACTION_DELETE
 * @compose:
 * @view: The view so that we save the scroll bar position.
 *
 * Adds text to the undo stack. It also performs test to limit the number
 * of undo levels and deltes the redo list
 **/

static void undo_add(const gchar *text, 
		     gint start_pos, gint end_pos,
		     UndoAction action, UndoMain *undostruct) 
{
        UndoInfo *undoinfo;

        debug_print("undo_add(%i)*%s*\n", strlen (text), text);

	g_return_if_fail(text != NULL);
        g_return_if_fail(end_pos >= start_pos);

        undo_free_list(&undostruct->redo);

        /* Set the redo sensitivity */
        undostruct->change_func(undostruct, UNDO_STATE_UNCHANGED, UNDO_STATE_FALSE, 
				undostruct->changewidget);

	if (undostruct->paste != 0) {
		if (action == UNDO_ACTION_INSERT) 
			action = UNDO_ACTION_REPLACE_INSERT;
		else 
			action = UNDO_ACTION_REPLACE_DELETE;
		undostruct->paste = undostruct->paste + 1;
		if (undostruct->paste == 3) 
			undostruct->paste = 0;
	}

        if (undo_merge(undostruct->undo, start_pos, end_pos, action, text))
                return;

	undo_check_size(undostruct);

	debug_print("New: %s Action: %d Paste: %d\n", text, action, undostruct->paste);

	undoinfo = undo_object_new(g_strdup(text), start_pos, end_pos, action,
				   GTK_ADJUSTMENT(GTK_STEXT(undostruct->text)->vadj)->value);

	if (end_pos-start_pos != 1 || text[0] == '\n')
                undoinfo->mergeable = FALSE;
        else
                undoinfo->mergeable = TRUE;

	undostruct->undo = g_list_prepend(undostruct->undo, undoinfo);

        undostruct->change_func(undostruct, UNDO_STATE_TRUE, UNDO_STATE_UNCHANGED, undostruct->changewidget);
}

/**
 * undo_undo:
 * @w: not used
 * @data: not used
 *
 * Executes an undo request on the current document
 **/
void undo_undo(UndoMain *undostruct) 
{
	UndoInfo *undoinfo;
        guint start_pos, end_pos;

	if (undostruct->undo == NULL)
		return;

	g_return_if_fail(undostruct != NULL);


	/* The undo data we need is always at the top op the
	   stack. So, therefore, the first one */
	undoinfo = g_list_nth_data(undostruct->undo, 0);
	g_return_if_fail(undoinfo != NULL);
	undoinfo->mergeable = FALSE;
	undostruct->redo = g_list_prepend(undostruct->redo, undoinfo);
	undostruct->undo = g_list_remove(undostruct->undo, undoinfo);

	/* Check if there is a selection active */
        start_pos = GTK_EDITABLE(undostruct->text)->selection_start_pos;
        end_pos   = GTK_EDITABLE(undostruct->text)->selection_end_pos;
	if ((start_pos > 0 || end_pos > 0) && (start_pos != end_pos))
		gtk_editable_select_region(GTK_EDITABLE(undostruct->text), 0, 0);

	/* Move the view (scrollbars) to the correct position */
	gtk_adjustment_set_value(GTK_ADJUSTMENT(GTK_STEXT(undostruct->text)->vadj), undoinfo->window_position);

	switch (undoinfo->action) {
	case UNDO_ACTION_DELETE:
		gtk_stext_set_point(GTK_STEXT(undostruct->text), undoinfo->start_pos);
		gtk_stext_insert(GTK_STEXT(undostruct->text), NULL, NULL, NULL, undoinfo->text, -1);
		debug_print("UNDO_ACTION_DELETE %s\n", undoinfo->text);
		break;
	case UNDO_ACTION_INSERT:
		gtk_stext_set_point(GTK_STEXT(undostruct->text), undoinfo->end_pos);
		gtk_stext_backward_delete(GTK_STEXT(undostruct->text), undoinfo->end_pos-undoinfo->start_pos);
		debug_print("UNDO_ACTION_INSERT %d\n", undoinfo->end_pos-undoinfo->start_pos);
		break;
        case UNDO_ACTION_REPLACE_INSERT:
		gtk_stext_set_point(GTK_STEXT(undostruct->text), undoinfo->end_pos);
		gtk_stext_backward_delete(GTK_STEXT(undostruct->text), undoinfo->end_pos-undoinfo->start_pos);
		debug_print("UNDO_ACTION_REPLACE %s\n", undoinfo->text);
                /* "pull" another data structure from the list */
                undoinfo = g_list_nth_data(undostruct->undo, 0);
                g_return_if_fail(undoinfo != NULL);
                undostruct->redo = g_list_prepend(undostruct->redo, undoinfo);
                undostruct->undo = g_list_remove(undostruct->undo, undoinfo);
                g_return_if_fail(undoinfo->action == UNDO_ACTION_REPLACE_DELETE);
		gtk_stext_set_point(GTK_STEXT(undostruct->text), undoinfo->start_pos);
		gtk_stext_insert(GTK_STEXT(undostruct->text), NULL, NULL, NULL, undoinfo->text, -1);
		debug_print("UNDO_ACTION_REPLACE %s\n", undoinfo->text);
                break;
        case UNDO_ACTION_REPLACE_DELETE:
                g_warning("This should not happen. UNDO_REPLACE_DELETE");
                break;
	default:
		g_assert_not_reached();
		break;
	}

        undostruct->change_func(undostruct, UNDO_STATE_UNCHANGED, 
				UNDO_STATE_TRUE, undostruct->changewidget);
				
	if (g_list_length (undostruct->undo) == 0)
	        undostruct->change_func(undostruct, UNDO_STATE_FALSE, 
			                UNDO_STATE_UNCHANGED, 
					undostruct->changewidget);
}

/**
 * undo_redo:
 * @w: not used
 * @data: not used
 *
 * executes a redo request on the current document
 **/
void undo_redo(UndoMain *undostruct) 
{
	UndoInfo *redoinfo;
        guint start_pos, end_pos;

	if (undostruct->redo == NULL)
		return;

	if (undostruct==NULL)
		return;

	redoinfo = g_list_nth_data(undostruct->redo, 0);
	g_return_if_fail (redoinfo!=NULL);
	undostruct->undo = g_list_prepend(undostruct->undo, redoinfo);
	undostruct->redo = g_list_remove(undostruct->redo, redoinfo);

	/* Check if there is a selection active */
        start_pos = GTK_EDITABLE(undostruct->text)->selection_start_pos;
        end_pos   = GTK_EDITABLE(undostruct->text)->selection_end_pos;
	if ((start_pos > 0 || end_pos > 0) && (start_pos != end_pos))
		gtk_editable_select_region(GTK_EDITABLE(undostruct->text), 0, 0);

	/* Move the view to the right position. */
	gtk_adjustment_set_value(GTK_ADJUSTMENT(GTK_STEXT(undostruct->text)->vadj), 
				 redoinfo->window_position);

	switch (redoinfo->action) {
	case UNDO_ACTION_INSERT:
		gtk_stext_set_point(GTK_STEXT(undostruct->text), redoinfo->start_pos);
		gtk_stext_insert(GTK_STEXT(undostruct->text), NULL, NULL, 
				 NULL, redoinfo->text, -1);
		debug_print("UNDO_ACTION_DELETE %s\n",redoinfo->text);
		break;
	case UNDO_ACTION_DELETE:
		gtk_stext_set_point(GTK_STEXT(undostruct->text), redoinfo->end_pos);
		gtk_stext_backward_delete(GTK_STEXT(undostruct->text), 
					  redoinfo->end_pos-redoinfo->start_pos);
		debug_print("UNDO_ACTION_INSERT %d\n", 
			    redoinfo->end_pos-redoinfo->start_pos);
		break;
        case UNDO_ACTION_REPLACE_DELETE:
		gtk_stext_set_point(GTK_STEXT(undostruct->text), redoinfo->end_pos);
		gtk_stext_backward_delete(GTK_STEXT(undostruct->text), 
					  redoinfo->end_pos-redoinfo->start_pos);
                /* "pull" another data structure from the list */
                redoinfo = g_list_nth_data(undostruct->redo, 0);
                g_return_if_fail(redoinfo != NULL);
                undostruct->undo = g_list_prepend(undostruct->undo, redoinfo);
                undostruct->redo = g_list_remove(undostruct->redo, redoinfo);
                g_return_if_fail(redoinfo->action==UNDO_ACTION_REPLACE_INSERT);
  		gtk_stext_set_point(GTK_STEXT(undostruct->text), redoinfo->start_pos);
		gtk_stext_insert(GTK_STEXT(undostruct->text), NULL, NULL, 
				 NULL, redoinfo->text, -1);
                break;
        case UNDO_ACTION_REPLACE_INSERT:
                g_warning("This should not happen. Redo: UNDO_REPLACE_INSERT");
                break;
	default:
		g_assert_not_reached();
		break;
	}

        undostruct->change_func(undostruct, UNDO_STATE_TRUE, UNDO_STATE_UNCHANGED, 
				undostruct->changewidget);
				
	if (g_list_length(undostruct->redo) == 0)
	        undostruct->change_func(undostruct, UNDO_STATE_UNCHANGED, 
					UNDO_STATE_FALSE, undostruct->changewidget);
}

void undo_insert_text_cb(GtkEditable *editable, gchar *new_text,
			 gint new_text_length, gint *position, 
			 UndoMain *undostruct) 
{
	guchar *text_to_insert;

	text_to_insert = g_strndup(new_text, new_text_length);
	undo_add(text_to_insert, *position, (*position + new_text_length), 
		 UNDO_ACTION_INSERT, undostruct);
	g_free (text_to_insert);
}

void undo_delete_text_cb(GtkEditable *editable, gint start_pos,
			 gint end_pos, UndoMain *undostruct) 
{
        guchar *text_to_delete;

        if (start_pos == end_pos )
		return;
		
        text_to_delete = gtk_editable_get_chars(GTK_EDITABLE(editable), 
						start_pos, end_pos);
	undo_add(text_to_delete, start_pos, end_pos, UNDO_ACTION_DELETE, undostruct);
	g_free (text_to_delete);
}

void undo_paste_clipboard_cb (GtkEditable *editable, UndoMain *undostruct) 
{
	debug_print("befor Paste: %d\n", undostruct->paste);
	if (undo_get_selection(editable, NULL, NULL)) 
		undostruct->paste = TRUE;
	debug_print("after Paste: %d\n", undostruct->paste);
}

/**
 * undo_get_selection:
 * @text: Text to get the selection from
 * @start: return here the start position of the selection
 * @end: return here the end position of the selection
 *
 * Gets the current selection for View
 *
 * Return Value: TRUE if there is a selection active, FALSE if not
 **/
static gint undo_get_selection(GtkEditable *text, guint *start, guint *end) 
{
        guint start_pos, end_pos;

        start_pos = text->selection_start_pos;
        end_pos   = text->selection_end_pos;

        /* The user can select from end to start too. If so, swap it*/
        if (end_pos < start_pos) {
                guint swap_pos;
                swap_pos  = end_pos;
                end_pos   = start_pos;
                start_pos = swap_pos;
        }

        if (start != NULL)
                *start = start_pos;
		
        if (end != NULL)
                *end = end_pos;

        if ((start_pos > 0 || end_pos > 0) && (start_pos != end_pos))
                return TRUE;
        else
                return FALSE;
}
