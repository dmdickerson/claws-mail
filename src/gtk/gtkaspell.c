/* gtkaspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin (original code for ispell).
 * Copyright (c) 2002 Melvin Hadasht.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
/*
 * Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 * Adapted for Sylpheed (Claws) (c) 2001-2002 by Hiroyuki Yamamoto & 
 * The Sylpheed Claws Team.
 * Adapted for pspell (c) 2001-2002 Melvin Hadasht
 * Adapted for GNU/aspell (c) 2002 Melvin Hadasht
 */
 
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_ASPELL

#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#ifndef WIN32
#include <sys/wait.h>
#endif
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifndef WIN32
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <time.h>
#ifndef WIN32
#include <dirent.h>
#endif

#include <glib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gdk/gdkkeysyms.h>

#ifdef WIN32
#include "w32_aspell_init.h"
#else
#include <aspell.h>
#endif

#include "intl.h"
#include "gtkstext.h"
#include "utils.h"

#include "gtkaspell.h"
#define ASPELL_FASTMODE       1
#define ASPELL_NORMALMODE     2
#define ASPELL_BADSPELLERMODE 3

#define GTKASPELLWORDSIZE 1024

/* size of the text buffer used in various word-processing routines. */
#define BUFSIZE 1024

/* number of suggestions to display on each menu. */
#define MENUCOUNT 15

/* 'config' must be defined as a 'AspellConfig *' */
#define RETURN_FALSE_IF_CONFIG_ERROR() \
{ \
	if (aspell_config_error_number(config) != 0) { \
		gtkaspellcheckers->error_message = g_strdup(aspell_config_error_message(config)); \
		return FALSE; \
	} \
}

#define CONFIG_REPLACE_RETURN_FALSE_IF_FAIL(option, value) { \
	aspell_config_replace(config, option, value);        \
	RETURN_FALSE_IF_CONFIG_ERROR();                      \
	}

typedef struct _GtkAspellCheckers {
	GSList		*checkers;
	GSList		*dictionary_list;
	gchar		*error_message;
} GtkAspellCheckers;

typedef struct _Dictionary {
	gchar *fullname;
	gchar *dictname;
	gchar *encoding;
} Dictionary;

typedef struct _GtkAspeller {
	Dictionary	*dictionary;
	gint		 sug_mode;
	AspellConfig	*config;
	AspellSpeller	*checker;
} GtkAspeller;

typedef void (*ContCheckFunc) (gpointer *gtkaspell);

struct _GtkAspell
{
	GtkAspeller	*gtkaspeller;
	GtkAspeller	*alternate_speller;
	gchar		*dictionary_path;
	gchar 		 theword[GTKASPELLWORDSIZE];
	gint  		 start_pos;
	gint  		 end_pos;
        gint 		 orig_pos;
	gint		 end_check_pos;
	gboolean	 misspelled;
	gboolean	 check_while_typing;
	gboolean	 use_alternate;

	ContCheckFunc 	 continue_check; 

	GtkWidget	*config_menu;
	GtkWidget	*popup_config_menu;
	GtkWidget	*sug_menu;
	GtkWidget	*replace_entry;

	gint		 default_sug_mode;
	gint		 max_sug;
	GList		*suggestions_list;

	GtkSText	*gtktext;
	GdkColor 	 highlight;
};

typedef AspellConfig GtkAspellConfig;

/******************************************************************************/

static GtkAspellCheckers *gtkaspellcheckers;

/* Error message storage */
static void gtkaspell_checkers_error_message	(gchar		*message);

/* Callbacks */
static void entry_insert_cb			(GtkSText	*gtktext, 
						 gchar		*newtext, 
						 guint		 len, 
						 guint		*ppos, 
					 	 GtkAspell	*gtkaspell);
static void entry_delete_cb			(GtkSText	*gtktext, 
						 gint		 start, 
						 gint		 end,
						 GtkAspell	*gtkaspell);
static gint button_press_intercept_cb		(GtkSText	*gtktext, 
						 GdkEvent	*e, 
						 GtkAspell	*gtkaspell);

/* Checker creation */
static GtkAspeller* gtkaspeller_new		(Dictionary	*dict);
static GtkAspeller* gtkaspeller_real_new	(Dictionary	*dict);
static GtkAspeller* gtkaspeller_delete		(GtkAspeller	*gtkaspeller);
static GtkAspeller* gtkaspeller_real_delete	(GtkAspeller	*gtkaspeller);

/* Checker configuration */
static gint 		set_dictionary   		(AspellConfig *config, 
							 Dictionary *dict);
static void 		set_sug_mode_cb     		(GtkMenuItem *w, 
							 GtkAspell *gtkaspell);
static void 		set_real_sug_mode		(GtkAspell *gtkaspell, 
							 const char *themode);

/* Checker actions */
static gboolean check_at			(GtkAspell	*gtkaspell, 
						 int		 from_pos);
static gboolean	check_next_prev			(GtkAspell	*gtkaspell, 
						 gboolean	 forward);
static GList* misspelled_suggest	 	(GtkAspell	*gtkaspell, 
						 guchar		*word);
static void add_word_to_session_cb		(GtkWidget	*w, 
						 gpointer	 data);
static void add_word_to_personal_cb		(GtkWidget	*w, 
						 gpointer	 data);
static void replace_with_create_dialog_cb	(GtkWidget	*w,
						 gpointer	 data);
static void replace_with_supplied_word_cb	(GtkWidget	*w, 
						 GtkAspell	*gtkaspell);
static void replace_word_cb			(GtkWidget	*w, 
						 gpointer	data); 
static void replace_real_word			(GtkAspell	*gtkaspell, 
						 gchar		*newword);
static void check_with_alternate_cb		(GtkWidget	*w,
						 gpointer	 data);
static void use_alternate_dict			(GtkAspell	*gtkaspell);
static void toggle_check_while_typing_cb	(GtkWidget	*w, 
						 gpointer	 data);

/* Menu creation */
static void popup_menu				(GtkAspell	*gtkaspell, 
						 GdkEventButton	*eb);
static GtkMenu*	make_sug_menu			(GtkAspell	*gtkaspell);
static void populate_submenu			(GtkAspell	*gtkaspell, 
						 GtkWidget	*menu);
static GtkMenu*	make_config_menu		(GtkAspell	*gtkaspell);
static void set_menu_pos			(GtkMenu	*menu, 
						 gint		*x, 
						 gint		*y, 
						 gpointer	 data);
/* Other menu callbacks */
static gboolean cancel_menu_cb			(GtkMenuShell	*w,
						 gpointer	 data);
static void change_dict_cb			(GtkWidget	*w, 
						 GtkAspell	*gtkaspell);
static void switch_to_alternate_cb		(GtkWidget	*w, 
						 gpointer	 data);

/* Misc. helper functions */
static void	 	set_point_continue		(GtkAspell *gtkaspell);
static void 		continue_check			(gpointer *gtkaspell);
static gboolean 	iswordsep			(unsigned char c);
static guchar 		get_text_index_whar		(GtkAspell *gtkaspell, 
							 int pos);
static gboolean 	get_word_from_pos		(GtkAspell *gtkaspell, 
							 gint pos, 
							 unsigned char* buf,
							 gint buflen,
							 gint *pstart, 
							 gint *pend);
static void 		allocate_color			(GtkAspell *gtkaspell,
							 gint rgbvalue);
static void 		change_color			(GtkAspell *gtkaspell, 
			 				 gint start, 
							 gint end, 
							 gchar *newtext,
							 GdkColor *color);
static guchar*		convert_to_aspell_encoding 	(const guchar *encoding);
static gint 		compare_dict			(Dictionary *a, 
							 Dictionary *b);
static void 		dictionary_delete		(Dictionary *dict);
static Dictionary *	dictionary_dup			(const Dictionary *dict);
static void 		free_suggestions_list		(GtkAspell *gtkaspell);
static void 		reset_theword_data		(GtkAspell *gtkaspell);
static void 		free_checkers			(gpointer elt, 
							 gpointer data);
static gint 		find_gtkaspeller		(gconstpointer aa, 
							 gconstpointer bb);
static void		gtkaspell_alert_dialog		(gchar *message);
/* gtkspellconfig - only one config per session */
GtkAspellConfig * gtkaspellconfig;

/******************************************************************************/

void gtkaspell_checkers_init(void)
{
	gtkaspellcheckers 		   = g_new(GtkAspellCheckers, 1);
	gtkaspellcheckers->checkers        = NULL;
	gtkaspellcheckers->dictionary_list = NULL;
	gtkaspellcheckers->error_message   = NULL;
}
	
void gtkaspell_checkers_quit(void)
{
	GSList *checkers;
	GSList *dict_list;

	if (gtkaspellcheckers == NULL) 
		return;

	if ((checkers  = gtkaspellcheckers->checkers)) {
		debug_print("Aspell: number of running checkers to delete %d\n",
				g_slist_length(checkers));

		g_slist_foreach(checkers, free_checkers, NULL);
		g_slist_free(checkers);
	}

	if ((dict_list = gtkaspellcheckers->dictionary_list)) {
		debug_print("Aspell: number of dictionaries to delete %d\n",
				g_slist_length(dict_list));

		gtkaspell_free_dictionary_list(dict_list);
		gtkaspellcheckers->dictionary_list = NULL;
	}

	g_free(gtkaspellcheckers->error_message);

	return;
}

static void gtkaspell_checkers_error_message (gchar *message)
{
	gchar *tmp;
	if (gtkaspellcheckers->error_message) {
		tmp = g_strdup_printf("%s\n%s", 
				      gtkaspellcheckers->error_message, message);
		g_free(message);
		g_free(gtkaspellcheckers->error_message);
		gtkaspellcheckers->error_message = tmp;
	} else 
		gtkaspellcheckers->error_message = message;
}

const char *gtkaspell_checkers_strerror(void)
{
	g_return_val_if_fail(gtkaspellcheckers, "");
	return gtkaspellcheckers->error_message;
}

void gtkaspell_checkers_reset_error(void)
{
	g_return_if_fail(gtkaspellcheckers);
	
	g_free(gtkaspellcheckers->error_message);
	
	gtkaspellcheckers->error_message = NULL;
}

GtkAspell *gtkaspell_new(const gchar *dictionary_path,
			 const gchar *dictionary, 
			 const gchar *encoding,
			 gint  misspelled_color,
			 gboolean check_while_typing,
			 gboolean use_alternate,
			 GtkSText *gtktext)
{
	Dictionary 	*dict;
	GtkAspell 	*gtkaspell;
	GtkAspeller 	*gtkaspeller;

	g_return_val_if_fail(gtktext, NULL);
	
	dict 	       = g_new0(Dictionary, 1);
	dict->fullname = g_strdup(dictionary);
	dict->encoding = g_strdup(encoding);

	gtkaspeller    = gtkaspeller_new(dict); 
	dictionary_delete(dict);

	if (!gtkaspeller)
		return NULL;
	
	gtkaspell = g_new0(GtkAspell, 1);

	gtkaspell->dictionary_path    = g_strdup(dictionary_path);

	gtkaspell->gtkaspeller	      = gtkaspeller;
	gtkaspell->alternate_speller  = NULL;
	gtkaspell->theword[0]	      = 0x00;
	gtkaspell->start_pos	      = 0;
	gtkaspell->end_pos	      = 0;
	gtkaspell->orig_pos	      = -1;
	gtkaspell->end_check_pos      = -1;
	gtkaspell->misspelled	      = -1;
	gtkaspell->check_while_typing = check_while_typing;
	gtkaspell->continue_check     = NULL;
	gtkaspell->config_menu        = NULL;
	gtkaspell->popup_config_menu  = NULL;
	gtkaspell->sug_menu	      = NULL;
	gtkaspell->replace_entry      = NULL;
	gtkaspell->gtktext	      = gtktext;
	gtkaspell->default_sug_mode   = ASPELL_FASTMODE;
	gtkaspell->max_sug	      = -1;
	gtkaspell->suggestions_list   = NULL;
	gtkaspell->use_alternate      = use_alternate;

	allocate_color(gtkaspell, misspelled_color);

	gtk_signal_connect_after(GTK_OBJECT(gtktext), "insert-text",
		                 GTK_SIGNAL_FUNC(entry_insert_cb), gtkaspell);
	gtk_signal_connect_after(GTK_OBJECT(gtktext), "delete-text",
		                 GTK_SIGNAL_FUNC(entry_delete_cb), gtkaspell);
	gtk_signal_connect(GTK_OBJECT(gtktext), "button-press-event",
		           GTK_SIGNAL_FUNC(button_press_intercept_cb), gtkaspell);
	
	debug_print("Aspell: created gtkaspell %0x\n", (guint) gtkaspell);

	return gtkaspell;
}

void gtkaspell_delete(GtkAspell * gtkaspell) 
{
	GtkSText *gtktext = gtkaspell->gtktext;
	
        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_insert_cb),
				      gtkaspell);
        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_delete_cb),
				      gtkaspell);
	gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(button_press_intercept_cb),
				      gtkaspell);

	gtkaspell_uncheck_all(gtkaspell);
	
	gtkaspeller_delete(gtkaspell->gtkaspeller);

	if (gtkaspell->use_alternate && gtkaspell->alternate_speller)
		gtkaspeller_delete(gtkaspell->alternate_speller);

	if (gtkaspell->sug_menu)
		gtk_widget_destroy(gtkaspell->sug_menu);

	if (gtkaspell->popup_config_menu)
		gtk_widget_destroy(gtkaspell->popup_config_menu);

	if (gtkaspell->config_menu)
		gtk_widget_destroy(gtkaspell->config_menu);

	if (gtkaspell->suggestions_list)
		free_suggestions_list(gtkaspell);

	g_free((gchar *)gtkaspell->dictionary_path);
	
	debug_print("Aspell: deleting gtkaspell %0x\n", (guint) gtkaspell);

	g_free(gtkaspell);

	gtkaspell = NULL;
}

static void entry_insert_cb(GtkSText *gtktext,
			    gchar *newtext, 
			    guint len,
			    guint *ppos, 
                            GtkAspell *gtkaspell) 
{
	size_t wlen;

	g_return_if_fail(gtkaspell->gtkaspeller->checker);

	if (!gtkaspell->check_while_typing)
		return;
	
	/* We must insert ourselves the character so the
	 * color of the inserted character is the default color.
	 * Never mess with set_insertion when frozen.
	 */

	gtk_stext_freeze(gtktext);
	if (MB_CUR_MAX > 1) {
		gchar *str;
		Xstrndup_a(str, newtext, len, return);
		wlen = mbstowcs(NULL, str, 0);
		if (wlen < 0)
			return;
	} else
		wlen = len;
	
#ifdef WIN32
	{
		gsize newlen;
		gchar *loctext;
		loctext = g_locale_from_utf8(newtext, wlen, NULL, &newlen, NULL);
		gtk_stext_backward_delete(GTK_STEXT(gtktext), newlen);
		gtk_stext_insert(GTK_STEXT(gtktext), NULL, NULL, NULL, loctext, newlen);
		g_free(loctext);
	}
#else
	gtk_stext_backward_delete(GTK_STEXT(gtktext), wlen);
	gtk_stext_insert(GTK_STEXT(gtktext), NULL, NULL, NULL, newtext, len);
#endif
	*ppos = gtk_stext_get_point(GTK_STEXT(gtktext));
	       
	if (iswordsep(newtext[0])) {
		/* did we just end a word? */
		if (*ppos >= 2) 
			check_at(gtkaspell, *ppos - 2);

		/* did we just split a word? */
		if (*ppos < gtk_stext_get_length(gtktext))
			check_at(gtkaspell, *ppos + 1);
	} else {
		/* check as they type, *except* if they're typing at the end (the most
                 * common case).
                 */
		if (*ppos < gtk_stext_get_length(gtktext) &&
		    !iswordsep(get_text_index_whar(gtkaspell, *ppos))) {
			check_at(gtkaspell, *ppos - 1);
		}
	}

	gtk_stext_thaw(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), *ppos);
}

static void entry_delete_cb(GtkSText *gtktext,
			    gint start, 
			    gint end, 
			    GtkAspell *gtkaspell) 
{
	int origpos;
    
	g_return_if_fail(gtkaspell->gtkaspeller->checker);

	if (!gtkaspell->check_while_typing)
		return;

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	if (start) {
		check_at(gtkaspell, start - 1);
		check_at(gtkaspell, start);
	}

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_stext_set_point(gtktext, origpos);
	/* this is to *UNDO* the selection, in case they were holding shift
         * while hitting backspace. */
	gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);
}

/* ok, this is pretty wacky:
 * we need to let the right-mouse-click go through, so it moves the cursor,
 * but we *can't* let it go through, because GtkText interprets rightclicks as
 * weird selection modifiers.
 *
 * so what do we do?  forge rightclicks as leftclicks, then popup the menu.
 * HACK HACK HACK.
 */
static gint button_press_intercept_cb(GtkSText *gtktext, GdkEvent *e, GtkAspell *gtkaspell) 
{
	GdkEventButton *eb;
	gboolean retval;

	g_return_val_if_fail(gtkaspell->gtkaspeller->checker, FALSE);

	if (e->type != GDK_BUTTON_PRESS) 
		return FALSE;
	eb = (GdkEventButton*) e;

	if (eb->button != 3) 
		return FALSE;

	/* forge the leftclick */
	eb->button = 1;

        gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
					 GTK_SIGNAL_FUNC(button_press_intercept_cb), 
					 gtkaspell);
	gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "button-press-event",
				e, &retval);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
					   GTK_SIGNAL_FUNC(button_press_intercept_cb), 
					   gtkaspell);
	gtk_signal_emit_stop_by_name(GTK_OBJECT(gtktext), "button-press-event");
    
	/* now do the menu wackiness */
	popup_menu(gtkaspell, eb);
	gtk_grab_remove(GTK_WIDGET(gtktext));
	return TRUE;
}

/* Checker creation */
static GtkAspeller *gtkaspeller_new(Dictionary *dictionary)
{
	GSList 		*exist;
	GtkAspeller	*gtkaspeller = NULL;
	GtkAspeller	*tmp;
	Dictionary	*dict;

	g_return_val_if_fail(gtkaspellcheckers, NULL);

	g_return_val_if_fail(dictionary, NULL);
#ifdef WIN32
	g_return_val_if_fail(w32_aspell_loaded(), NULL);
#endif

	if (dictionary->fullname == NULL)
		gtkaspell_checkers_error_message(g_strdup(_("No dictionary selected.")));
	
	g_return_val_if_fail(dictionary->fullname, NULL);
	
	if (dictionary->dictname == NULL) {
		gchar *tmp;

		tmp = strrchr(dictionary->fullname, G_DIR_SEPARATOR);
#ifdef WIN32
		if (!tmp) tmp = strrchr(dictionary->fullname, '/');
#endif

		if (tmp == NULL)
			dictionary->dictname = dictionary->fullname;
		else
			dictionary->dictname = tmp + 1;
	}

	dict = dictionary_dup(dictionary);

	tmp = g_new0(GtkAspeller, 1);
	tmp->dictionary = dict;

	exist = g_slist_find_custom(gtkaspellcheckers->checkers, tmp, 
				    find_gtkaspeller);
	
	g_free(tmp);

	if ((gtkaspeller = gtkaspeller_real_new(dict)) != NULL) {
		gtkaspellcheckers->checkers = g_slist_append(
				gtkaspellcheckers->checkers,
				gtkaspeller);

		debug_print("Aspell: Created a new gtkaspeller %0x\n",
				(gint) gtkaspeller);
	} else {
		dictionary_delete(dict);

		debug_print("Aspell: Could not create spell checker.\n");
	}

	debug_print("Aspell: number of existing checkers %d\n", 
			g_slist_length(gtkaspellcheckers->checkers));

	return gtkaspeller;
}

static GtkAspeller *gtkaspeller_real_new(Dictionary *dict)
{
	GtkAspeller		*gtkaspeller;
	AspellConfig		*config;
	AspellCanHaveError 	*ret;
	
	g_return_val_if_fail(gtkaspellcheckers, NULL);
	g_return_val_if_fail(dict, NULL);

	gtkaspeller = g_new(GtkAspeller, 1);
	
	gtkaspeller->dictionary = dict;
	gtkaspeller->sug_mode   = ASPELL_FASTMODE;

	config = new_aspell_config();

	if (!set_dictionary(config, dict))
		return NULL;
	
	ret = new_aspell_speller(config);
	delete_aspell_config(config);

	if (aspell_error_number(ret) != 0) {
		gtkaspellcheckers->error_message = g_strdup(aspell_error_message(ret));
		
		delete_aspell_can_have_error(ret);
		
		return NULL;
	}

	gtkaspeller->checker = to_aspell_speller(ret);
	gtkaspeller->config  = aspell_speller_config(gtkaspeller->checker);

	return gtkaspeller;
}

static GtkAspeller *gtkaspeller_delete(GtkAspeller *gtkaspeller)
{
	g_return_val_if_fail(gtkaspellcheckers, NULL);
	
	gtkaspellcheckers->checkers = 
		g_slist_remove(gtkaspellcheckers->checkers, 
				gtkaspeller);

	debug_print("Aspell: Deleting gtkaspeller %0x.\n", 
			(gint) gtkaspeller);

	gtkaspeller_real_delete(gtkaspeller);

	debug_print("Aspell: number of existing checkers %d\n", 
			g_slist_length(gtkaspellcheckers->checkers));

	return gtkaspeller;
}

static GtkAspeller *gtkaspeller_real_delete(GtkAspeller *gtkaspeller)
{
	g_return_val_if_fail(gtkaspeller,          NULL);
	g_return_val_if_fail(gtkaspeller->checker, NULL);

	aspell_speller_save_all_word_lists(gtkaspeller->checker);

	delete_aspell_speller(gtkaspeller->checker);

	dictionary_delete(gtkaspeller->dictionary);

	debug_print("Aspell: gtkaspeller %0x deleted.\n", 
		    (gint) gtkaspeller);

	g_free(gtkaspeller);

	return NULL;
}

/*****************************************************************************/
/* Checker configuration */

static gboolean set_dictionary(AspellConfig *config, Dictionary *dict)
{
	gchar *language = NULL;
	gchar *jargon = NULL;
	gchar *size   = NULL;
	gchar  buf[BUFSIZE];
	
	g_return_val_if_fail(config, FALSE);
	g_return_val_if_fail(dict,   FALSE);

	strncpy(buf, dict->fullname, BUFSIZE-1);
	buf[BUFSIZE-1] = 0x00;

	buf[dict->dictname - dict->fullname] = 0x00;

	CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("dict-dir", buf);
	debug_print("Aspell: looking for dictionaries in path %s.\n", buf);

	strncpy(buf, dict->dictname, BUFSIZE-1);
	language = buf;
	
	if ((size = strrchr(buf, '-')) && isdigit((int) size[1]))
		*size++ = 0x00;
	else
		size = NULL;
				
	if ((jargon = strchr(language, '-')) != NULL) 
		*jargon++ = 0x00;
	
	if (size != NULL && jargon == size)
		jargon = NULL;

	debug_print("Aspell: language: %s, jargon: %s, size: %s\n",
		    language, jargon, size);
	
	if (language)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("lang", language);
	if (jargon)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("jargon", jargon);
	if (size)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("size", size);
	if (dict->encoding) {
		gchar *aspell_enc;
	
		aspell_enc = convert_to_aspell_encoding (dict->encoding);
		aspell_config_replace(config, "encoding", (const char *) aspell_enc);
		g_free(aspell_enc);

		RETURN_FALSE_IF_CONFIG_ERROR();
	}
	
	return TRUE;
}

guchar *gtkaspell_get_dict(GtkAspell *gtkaspell)
{

	g_return_val_if_fail(gtkaspell->gtkaspeller->config,     NULL);
	g_return_val_if_fail(gtkaspell->gtkaspeller->dictionary, NULL);
 	
	return g_strdup(gtkaspell->gtkaspeller->dictionary->dictname);
}
  
guchar *gtkaspell_get_path(GtkAspell *gtkaspell)
{
	guchar *path;
	Dictionary *dict;

	g_return_val_if_fail(gtkaspell->gtkaspeller->config, NULL);
	g_return_val_if_fail(gtkaspell->gtkaspeller->dictionary, NULL);

	dict = gtkaspell->gtkaspeller->dictionary;
	path = g_strndup(dict->fullname, dict->dictname - dict->fullname);

	return path;
}

/* set_sug_mode_cb() - Menu callback: Set the suggestion mode */
static void set_sug_mode_cb(GtkMenuItem *w, GtkAspell *gtkaspell)
{
	char *themode;
	
	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar **) &themode);
	
	set_real_sug_mode(gtkaspell, themode);

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);
}

static void set_real_sug_mode(GtkAspell *gtkaspell, const char *themode)
{
	gint result;
	gint mode = ASPELL_FASTMODE;
	g_return_if_fail(gtkaspell);
	g_return_if_fail(gtkaspell->gtkaspeller);
	g_return_if_fail(themode);

	if (!strcmp(themode,_("Normal Mode")))
		mode = ASPELL_NORMALMODE;
	else if (!strcmp( themode,_("Bad Spellers Mode")))
		mode = ASPELL_BADSPELLERMODE;

	result = gtkaspell_set_sug_mode(gtkaspell, mode);

	if(!result) {
		debug_print("Aspell: error while changing suggestion mode:%s\n",
			    gtkaspellcheckers->error_message);
		gtkaspell_checkers_reset_error();
	}
}
  
/* gtkaspell_set_sug_mode() - Set the suggestion mode */
gboolean gtkaspell_set_sug_mode(GtkAspell *gtkaspell, gint themode)
{
	AspellConfig *config;

	g_return_val_if_fail(gtkaspell, FALSE);
	g_return_val_if_fail(gtkaspell->gtkaspeller, FALSE);
	g_return_val_if_fail(gtkaspell->gtkaspeller->config, FALSE);

	debug_print("Aspell: setting sug mode of gtkaspeller %0x to %d\n",
			(guint) gtkaspell->gtkaspeller, themode);

	config = gtkaspell->gtkaspeller->config;

	switch (themode) {
		case ASPELL_FASTMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", "fast");
			break;
		case ASPELL_NORMALMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", "normal");
			break;
		case ASPELL_BADSPELLERMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", 
							    "bad-spellers");
			break;
		default: 
			gtkaspellcheckers->error_message = 
				g_strdup(_("Unknown suggestion mode."));
			return FALSE;
		}

	gtkaspell->gtkaspeller->sug_mode = themode;
	gtkaspell->default_sug_mode      = themode;

	return TRUE;
}

/* misspelled_suggest() - Create a suggestion list for  word  */
static GList *misspelled_suggest(GtkAspell *gtkaspell, guchar *word) 
{
	const guchar          *newword;
	GList                 *list = NULL;
	const AspellWordList  *suggestions;
	AspellStringEnumeration *elements;

	g_return_val_if_fail(word, NULL);

	if (!aspell_speller_check(gtkaspell->gtkaspeller->checker, word, -1)) {
		free_suggestions_list(gtkaspell);

		suggestions = aspell_speller_suggest(gtkaspell->gtkaspeller->checker, 
						     (const char *)word, -1);
		elements    = aspell_word_list_elements(suggestions);
		list        = g_list_append(list, g_strdup(word)); 
		
		while ((newword = aspell_string_enumeration_next(elements)) != NULL)
			list = g_list_append(list, g_strdup(newword));

		gtkaspell->max_sug          = g_list_length(list) - 1;
		gtkaspell->suggestions_list = list;

		return list;
	}

	free_suggestions_list(gtkaspell);

	return NULL;
}

/* misspelled_test() - Just test if word is correctly spelled */  
static int misspelled_test(GtkAspell *gtkaspell, unsigned char *word) 
{
	return aspell_speller_check(gtkaspell->gtkaspeller->checker, word, -1) ? 0 : 1; 
}


static gboolean iswordsep(unsigned char c) 
{
	return !isalpha(c) && c != '\'';
}

static guchar get_text_index_whar(GtkAspell *gtkaspell, int pos) 
{
	guchar a;
	gchar *text;
	
	text = gtk_editable_get_chars(GTK_EDITABLE(gtkaspell->gtktext), pos, 
				      pos + 1);
	if (text == NULL) 
		return 0;
#ifdef WIN32
	locale_from_utf8(&text);
#endif
	a = (guchar) *text;

	g_free(text);

	return a;
}

/* get_word_from_pos () - return the word pointed to. */
/* Handles correctly the quotes. */
static gboolean get_word_from_pos(GtkAspell *gtkaspell, gint pos, 
                                  unsigned char* buf, gint buflen,
                                  gint *pstart, gint *pend) 
{

	/* TODO : when correcting a word into quotes, change the color of */
	/* the quotes too, as may be they were highlighted before. To do  */
	/* this, we can use two others pointers that points to the whole    */
	/* word including quotes. */

	gint start;
	gint end;
		  
	guchar c;
	GtkSText *gtktext;
	
	gtktext = gtkaspell->gtktext;
	if (iswordsep(get_text_index_whar(gtkaspell, pos))) 
		return FALSE;
	
	/* The apostrophe character is somtimes used for quotes 
	 * So include it in the word only if it is not surrounded 
	 * by other characters. 
	 */
	 
	for (start = pos; start >= 0; --start) {
		c = get_text_index_whar(gtkaspell, start);
		if (c == '\'') {
			if (start > 0) {
				if (!isalpha(get_text_index_whar(gtkaspell,
								 start - 1))) {
					/* start_quote = TRUE; */
					break;
				}
			}
			else {
				/* start_quote = TRUE; */
				break;
			}
		}
		else if (!isalpha(c))
				break;
	}

	start++;

	for (end = pos; end < gtk_stext_get_length(gtktext); end++) {
		c = get_text_index_whar(gtkaspell, end); 
		if (c == '\'') {
			if (end < gtk_stext_get_length(gtktext)) {
				if (!isalpha(get_text_index_whar(gtkaspell,
								 end + 1))) {
					/* end_quote = TRUE; */
					break;
				}
			}
			else {
				/* end_quote = TRUE; */
				break;
			}
		}
		else if(!isalpha(c))
				break;
	}
						
	if (pstart) 
		*pstart = start;
	if (pend) 
		*pend = end;

	if (buf) {
		if (end - start < buflen) {
			for (pos = start; pos < end; pos++) 
				buf[pos - start] =
					get_text_index_whar(gtkaspell, pos);
			buf[pos - start] = 0;
		} else
			return FALSE;
	}

	return TRUE;
}

static gboolean check_at(GtkAspell *gtkaspell, gint from_pos) 
{
	gint	      start, end;
	unsigned char buf[GTKASPELLWORDSIZE];
	GtkSText     *gtktext;
#ifdef WIN32
	unsigned char *locbuf;
	gsize oldsize,newsize;
#endif

	g_return_val_if_fail(from_pos >= 0, FALSE);
    
	gtktext = gtkaspell->gtktext;

	if (!get_word_from_pos(gtkaspell, from_pos, buf, sizeof(buf), 
			       &start, &end))
		return FALSE;

	if (misspelled_test(gtkaspell, buf)) {
		strncpy(gtkaspell->theword, buf, GTKASPELLWORDSIZE - 1);
		gtkaspell->theword[GTKASPELLWORDSIZE - 1] = 0;
		gtkaspell->start_pos  = start;
		gtkaspell->end_pos    = end;
		free_suggestions_list(gtkaspell);

		change_color(gtkaspell, start, end, buf, &(gtkaspell->highlight));
		return TRUE;
	} else {
		change_color(gtkaspell, start, end, buf, NULL);
		return FALSE;
	}
}

static gboolean check_next_prev(GtkAspell *gtkaspell, gboolean forward)
{
	gint pos;
	gint minpos;
	gint maxpos;
	gint direc = -1;
	gboolean misspelled;
	
	minpos = 0;
	maxpos = gtkaspell->end_check_pos;

	if (forward) {
		minpos = -1;
		direc = 1;
		maxpos--;
	} 

	pos = gtk_editable_get_position(GTK_EDITABLE(gtkaspell->gtktext));
	gtkaspell->orig_pos = pos;
	while (iswordsep(get_text_index_whar(gtkaspell, pos)) &&
	       pos > minpos && pos <= maxpos) 
		pos += direc;
	while (!(misspelled = check_at(gtkaspell, pos)) &&
	       pos > minpos && pos <= maxpos) {

		while (!iswordsep(get_text_index_whar(gtkaspell, pos)) &&
		       pos > minpos && pos <= maxpos)
			pos += direc;

		while (iswordsep(get_text_index_whar(gtkaspell, pos)) && 
		       pos > minpos && pos <= maxpos) 
			pos += direc;
	}
	if (misspelled) {
		misspelled_suggest(gtkaspell, gtkaspell->theword);

		if (forward)
			gtkaspell->orig_pos = gtkaspell->end_pos;

		gtk_stext_set_point(GTK_STEXT(gtkaspell->gtktext),
				gtkaspell->end_pos);
		gtk_editable_set_position(GTK_EDITABLE(gtkaspell->gtktext),
				gtkaspell->end_pos);
		gtk_menu_popup(make_sug_menu(gtkaspell), NULL, NULL, 
				set_menu_pos, gtkaspell, 0, GDK_CURRENT_TIME);
	} else {
		reset_theword_data(gtkaspell);

		gtkaspell_alert_dialog(_("No misspelled word found."));
		gtk_stext_set_point(GTK_STEXT(gtkaspell->gtktext),
				    gtkaspell->orig_pos);
		gtk_editable_set_position(GTK_EDITABLE(gtkaspell->gtktext),
					  gtkaspell->orig_pos);

		
	}
	return misspelled;
}

void gtkaspell_check_backwards(GtkAspell *gtkaspell)
{
	gtkaspell->continue_check = NULL;
	gtkaspell->end_check_pos =
		gtk_stext_get_length(GTK_STEXT(gtkaspell->gtktext));
	check_next_prev(gtkaspell, FALSE);
}

void gtkaspell_check_forwards_go(GtkAspell *gtkaspell)
{

	gtkaspell->continue_check = NULL;
	gtkaspell->end_check_pos
		= gtk_stext_get_length(GTK_STEXT(gtkaspell->gtktext));
	check_next_prev(gtkaspell, TRUE);
}

void gtkaspell_check_all(GtkAspell *gtkaspell)
{	
	GtkWidget *gtktext;
	gint start, end;

	g_return_if_fail(gtkaspell);
	g_return_if_fail(gtkaspell->gtktext);

	gtktext = (GtkWidget *) gtkaspell->gtktext;

	start = 0;	
	end   = gtk_stext_get_length(GTK_STEXT(gtktext));

	if (GTK_EDITABLE(gtktext)->has_selection) {
		start = GTK_EDITABLE(gtktext)->selection_start_pos;
		end   = GTK_EDITABLE(gtktext)->selection_end_pos;
	}

	if (start > end) {
		gint tmp;

		tmp   = start;
		start = end;
		end   = tmp;
	}
		
	
	gtk_editable_set_position(GTK_EDITABLE(gtktext), start);
	gtk_stext_set_point(GTK_STEXT(gtktext), start);

	gtkaspell->continue_check = continue_check;
	gtkaspell->end_check_pos  = end;

	gtkaspell->misspelled = check_next_prev(gtkaspell, TRUE);

}	

static void continue_check(gpointer *data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;
	gint pos = gtk_editable_get_position(GTK_EDITABLE(gtkaspell->gtktext));
	if (pos < gtkaspell->end_check_pos && gtkaspell->misspelled)
		gtkaspell->misspelled = check_next_prev(gtkaspell, TRUE);
	else
		gtkaspell->continue_check = NULL;
		
}

void gtkaspell_highlight_all(GtkAspell *gtkaspell) 
{
	guint     origpos;
	guint     pos = 0;
	guint     len;
	GtkSText *gtktext;
	gfloat    adj_value;

	g_return_if_fail(gtkaspell->gtkaspeller->checker);	

	gtktext = gtkaspell->gtktext;

	adj_value = gtktext->vadj->value;

	len = gtk_stext_get_length(gtktext);

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

	while (pos < len) {
		while (pos < len && 
		       iswordsep(get_text_index_whar(gtkaspell, pos)))
			pos++;
		while (pos < len &&
		       !iswordsep(get_text_index_whar(gtkaspell, pos)))
			pos++;
		if (pos > 0)
			check_at(gtkaspell, pos - 1);
	}
	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_stext_set_point(GTK_STEXT(gtktext), origpos);
	gtk_adjustment_set_value(gtktext->vadj, adj_value);
}

static void replace_with_supplied_word_cb(GtkWidget *w, GtkAspell *gtkaspell) 
{
	unsigned char *newword;
	GdkEvent *e= (GdkEvent *) gtk_get_current_event();
	
	newword = gtk_editable_get_chars(GTK_EDITABLE(gtkaspell->replace_entry),
					 0, -1);
	
	if (strcmp(newword, gtkaspell->theword)) {
		replace_real_word(gtkaspell, newword);

		if ((e->type == GDK_KEY_PRESS && 
		    ((GdkEventKey *) e)->state & GDK_MOD1_MASK)) {
			aspell_speller_store_replacement(gtkaspell->gtkaspeller->checker, 
							 gtkaspell->theword, -1, 
							 newword, -1);
		}
		gtkaspell->replace_entry = NULL;
	}

	g_free(newword);

	set_point_continue(gtkaspell);
}


static void replace_word_cb(GtkWidget *w, gpointer data)
{
	unsigned char *newword;
	GtkAspell *gtkaspell = (GtkAspell *) data;
	GdkEvent *e= (GdkEvent *) gtk_get_current_event();

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar**) &newword);

	replace_real_word(gtkaspell, newword);

	if ((e->type == GDK_KEY_PRESS && 
	    ((GdkEventKey *) e)->state & GDK_MOD1_MASK) ||
	    (e->type == GDK_BUTTON_RELEASE && 
	     ((GdkEventButton *) e)->state & GDK_MOD1_MASK)) {
		aspell_speller_store_replacement(gtkaspell->gtkaspeller->checker, 
						 gtkaspell->theword, -1, 
						 newword, -1);
	}

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->parent));

	set_point_continue(gtkaspell);
}

static void replace_real_word(GtkAspell *gtkaspell, gchar *newword)
{
	int		oldlen, newlen, wordlen;
	gint		origpos;
	gint		pos;
	gint 		start = gtkaspell->start_pos;
	GtkSText       *gtktext;
    
	if (!newword) return;

	gtktext = gtkaspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));
	origpos = gtkaspell->orig_pos;
	pos     = origpos;
	oldlen  = gtkaspell->end_pos - gtkaspell->start_pos;
	wordlen = strlen(gtkaspell->theword);

	newlen = strlen(newword); /* FIXME: multybyte characters? */

	gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
					 GTK_SIGNAL_FUNC(entry_insert_cb), 
					 gtkaspell);
	gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
					 GTK_SIGNAL_FUNC(entry_delete_cb), 
					 gtkaspell);

	gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "delete-text", 
				gtkaspell->start_pos, gtkaspell->end_pos);
	gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "insert-text", 
				newword, newlen, &start);
	
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
					   GTK_SIGNAL_FUNC(entry_insert_cb), 
					   gtkaspell);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
					   GTK_SIGNAL_FUNC(entry_delete_cb), 
					   gtkaspell);
	
	/* Put the point and the position where we clicked with the mouse
	 * It seems to be a hack, as I must thaw,freeze,thaw the widget
	 * to let it update correctly the word insertion and then the
	 * point & position position. If not, SEGV after the first replacement
	 * If the new word ends before point, put the point at its end.
	 */
    
	if (origpos - gtkaspell->start_pos < oldlen && 
	    origpos - gtkaspell->start_pos >= 0) {
		/* Original point was in the word.
		 * Let it there unless point is going to be outside of the word
		 */
		if (origpos - gtkaspell->start_pos >= newlen) {
			pos = gtkaspell->start_pos + newlen;
		}
	}
	else if (origpos >= gtkaspell->end_pos) {
		/* move the position according to the change of length */
		pos = origpos + newlen - oldlen;
	}
	
	gtkaspell->end_pos = gtkaspell->start_pos + strlen(newword); /* FIXME: multibyte characters? */
	
	gtk_stext_thaw(GTK_STEXT(gtktext));
	gtk_stext_freeze(GTK_STEXT(gtktext));

	if (GTK_STEXT(gtktext)->text_len < pos)
		pos = gtk_stext_get_length(GTK_STEXT(gtktext));

	gtkaspell->orig_pos = pos;

	gtk_editable_set_position(GTK_EDITABLE(gtktext), gtkaspell->orig_pos);
	gtk_stext_set_point(GTK_STEXT(gtktext), 
			    gtk_editable_get_position(GTK_EDITABLE(gtktext)));

	gtk_stext_thaw(GTK_STEXT(gtktext));
}

/* Accept this word for this session */
static void add_word_to_session_cb(GtkWidget *w, gpointer data)
{
	guint     pos;
	GtkSText *gtktext;
   	GtkAspell *gtkaspell = (GtkAspell *) data; 
	gtktext = gtkaspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));

	pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
    
	aspell_speller_add_to_session(gtkaspell->gtkaspeller->checker,
				      gtkaspell->theword, 
				      strlen(gtkaspell->theword));

	check_at(gtkaspell, gtkaspell->start_pos);

	gtk_stext_thaw(gtkaspell->gtktext);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	set_point_continue(gtkaspell);
}

/* add_word_to_personal_cb() - add word to personal dict. */
static void add_word_to_personal_cb(GtkWidget *w, gpointer data)
{
   	GtkAspell *gtkaspell = (GtkAspell *) data; 
	GtkSText *gtktext    = gtkaspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));
    
	aspell_speller_add_to_personal(gtkaspell->gtkaspeller->checker,
				       gtkaspell->theword,
				       strlen(gtkaspell->theword));
    
	check_at(gtkaspell, gtkaspell->start_pos);

	gtk_stext_thaw(gtkaspell->gtktext);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));
	set_point_continue(gtkaspell);
}

static void check_with_alternate_cb(GtkWidget *w, gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;
	gint misspelled;

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	use_alternate_dict(gtkaspell);
	misspelled = check_at(gtkaspell, gtkaspell->start_pos);

	if (!gtkaspell->continue_check) {

		gtkaspell->misspelled = misspelled;

		if (gtkaspell->misspelled) {

			misspelled_suggest(gtkaspell, gtkaspell->theword);

			gtk_stext_set_point(GTK_STEXT(gtkaspell->gtktext),
					    gtkaspell->end_pos);
			gtk_editable_set_position(GTK_EDITABLE(gtkaspell->gtktext),
						  gtkaspell->end_pos);

			gtk_menu_popup(make_sug_menu(gtkaspell), NULL, NULL, 
				       set_menu_pos, gtkaspell, 0, 
				       GDK_CURRENT_TIME);
			return;
		}
	} else
		gtkaspell->orig_pos = gtkaspell->start_pos;

	set_point_continue(gtkaspell);
}
	
static void replace_with_create_dialog_cb(GtkWidget *w, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
	gchar *thelabel;
	gint xx, yy;
	GtkAspell *gtkaspell = (GtkAspell *) data;

	gdk_window_get_origin((GTK_WIDGET(w)->parent)->window, &xx, &yy);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	dialog = gtk_dialog_new();

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_window_set_title(GTK_WINDOW(dialog),_("Replace unknown word"));
	gtk_widget_set_uposition(dialog, xx, yy);

	gtk_signal_connect_object(GTK_OBJECT(dialog), "destroy",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				  GTK_OBJECT(dialog));

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);

	thelabel = g_strdup_printf(_("Replace \"%s\" with: "), 
				   gtkaspell->theword);
	label = gtk_label_new(thelabel);
	g_free(thelabel);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtkaspell->replace_entry = entry;
	gtk_entry_set_text(GTK_ENTRY(entry), gtkaspell->theword);
	gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
	gtk_signal_connect(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(replace_with_supplied_word_cb), 
			   gtkaspell);
	gtk_signal_connect_object(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(gtk_widget_destroy), 
			   GTK_OBJECT(dialog));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, 
			   TRUE, 0);
	label = gtk_label_new(_("Holding down MOD1 key while pressing "
				"Enter\nwill learn from mistake.\n"));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	gtk_misc_set_padding(GTK_MISC(label), 8, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, 
			TRUE, TRUE, 0);

	hbox = gtk_hbox_new(TRUE, 0);

	ok_button = gtk_button_new_with_label(_("OK"));
	gtk_box_pack_start(GTK_BOX(hbox), ok_button, TRUE, TRUE, 8);
	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			GTK_SIGNAL_FUNC(replace_with_supplied_word_cb), 
			gtkaspell);
	gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
			GTK_SIGNAL_FUNC(gtk_widget_destroy), 
			GTK_OBJECT(dialog));

	cancel_button = gtk_button_new_with_label(_("Cancel"));
	gtk_box_pack_start(GTK_BOX(hbox), cancel_button, TRUE, TRUE, 8);
	gtk_signal_connect_object(GTK_OBJECT(cancel_button), "clicked",
			GTK_SIGNAL_FUNC(gtk_widget_destroy), 
			GTK_OBJECT(dialog));

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), hbox);

	gtk_widget_grab_focus(entry);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	gtk_widget_show_all(dialog);
}

void gtkaspell_uncheck_all(GtkAspell * gtkaspell) 
{
	gint 	  origpos;
	gchar	 *text;
	gfloat 	  adj_value;
	GtkSText *gtktext;
	
	gtktext = gtkaspell->gtktext;

	adj_value = gtktext->vadj->value;

	gtk_stext_freeze(gtktext);

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

	text = gtk_editable_get_chars(GTK_EDITABLE(gtktext), 0, -1);

	gtk_stext_set_point(gtktext, 0);
	gtk_stext_forward_delete(gtktext, gtk_stext_get_length(gtktext));
	gtk_stext_insert(gtktext, NULL, NULL, NULL, text, strlen(text));

	gtk_stext_thaw(gtktext);

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_stext_set_point(gtktext, origpos);
	gtk_adjustment_set_value(gtktext->vadj, adj_value);

	g_free(text);

}

static void toggle_check_while_typing_cb(GtkWidget *w, gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;

	gtkaspell->check_while_typing = gtkaspell->check_while_typing == FALSE;

	if (!gtkaspell->check_while_typing)
		gtkaspell_uncheck_all(gtkaspell);

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);
}

static GSList *create_empty_dictionary_list(void)
{
	GSList *list = NULL;
	Dictionary *dict;

	dict = g_new0(Dictionary, 1);
	dict->fullname = g_strdup(_("None"));
	dict->dictname = dict->fullname;
	dict->encoding = NULL;

	return g_slist_append(list, dict);
}

/* gtkaspell_get_dictionary_list() - returns list of dictionary names */
GSList *gtkaspell_get_dictionary_list(const gchar *aspell_path, gint refresh)
{
	GSList *list;
	Dictionary *dict;
	AspellConfig *config;
	AspellDictInfoList *dlist;
	AspellDictInfoEnumeration *dels;
	const AspellDictInfo *entry;

	if (!gtkaspellcheckers)
		gtkaspell_checkers_init();

	if (gtkaspellcheckers->dictionary_list && !refresh)
		return gtkaspellcheckers->dictionary_list;
	else
		gtkaspell_free_dictionary_list(gtkaspellcheckers->dictionary_list);
	list = NULL;

#ifdef WIN32
	g_return_val_if_fail(w32_aspell_loaded(), NULL);
	if (!aspell_path)
		aspell_path = "";
#endif /* WIN32 */

	config = new_aspell_config();
#if 0 
	aspell_config_replace(config, "rem-all-word-list-path", "");
	if (aspell_config_error_number(config) != 0) {
		gtkaspellcheckers->error_message = g_strdup(
				aspell_config_error_message(config));
		gtkaspellcheckers->dictionary_list =
			create_empty_dictionary_list();

		return gtkaspellcheckers->dictionary_list; 
	}
#endif
	aspell_config_replace(config, "dict-dir", aspell_path);
	if (aspell_config_error_number(config) != 0) {
		gtkaspellcheckers->error_message = g_strdup(
				aspell_config_error_message(config));
		gtkaspellcheckers->dictionary_list =
			create_empty_dictionary_list();

		return gtkaspellcheckers->dictionary_list; 
	}

	dlist = get_aspell_dict_info_list(config);
	delete_aspell_config(config);

	debug_print("Aspell: checking for dictionaries in %s\n", aspell_path);
	dels = aspell_dict_info_list_elements(dlist);
	while ( (entry = aspell_dict_info_enumeration_next(dels)) != 0) 
	{
		dict = g_new0(Dictionary, 1);
		dict->fullname = g_strdup_printf("%s%s", aspell_path, 
				entry->name);
		dict->dictname = dict->fullname + strlen(aspell_path);
		dict->encoding = g_strdup(entry->code);
		debug_print("Aspell: found dictionary %s %s\n", dict->fullname,
				dict->dictname);
		list = g_slist_insert_sorted(list, dict,
				(GCompareFunc) compare_dict);
	}

	delete_aspell_dict_info_enumeration(dels);
	
        if(list==NULL){
		
		debug_print("Aspell: error when searching for dictionaries: "
			      "No dictionary found.\n");
		list = create_empty_dictionary_list();
	}

	gtkaspellcheckers->dictionary_list = list;

	return list;
}

void gtkaspell_free_dictionary_list(GSList *list)
{
	Dictionary *dict;
	GSList *walk;
	for (walk = list; walk != NULL; walk = g_slist_next(walk))
		if (walk->data) {
			dict = (Dictionary *) walk->data;
			dictionary_delete(dict);
		}				
	g_slist_free(list);
}

GtkWidget *gtkaspell_dictionary_option_menu_new(const gchar *aspell_path)
{
	GSList *dict_list, *tmp;
	GtkWidget *item;
	GtkWidget *menu;
	Dictionary *dict;

	dict_list = gtkaspell_get_dictionary_list(aspell_path, TRUE);
	g_return_val_if_fail(dict_list, NULL);

	menu = gtk_menu_new();
	
	for (tmp = dict_list; tmp != NULL; tmp = g_slist_next(tmp)) {
#ifdef WIN32
	  	gchar *dictname_utf8, *fullname_utf8;
#endif /* WIN32 */
		dict = (Dictionary *) tmp->data;
#ifdef WIN32
	  	dictname_utf8 = g_locale_to_utf8(dict->dictname, -1, NULL, NULL, NULL);
	  	fullname_utf8 = g_locale_to_utf8(dict->fullname, -1, NULL, NULL, NULL);
		item = gtk_menu_item_new_with_label(dictname_utf8);
		gtk_object_set_data(GTK_OBJECT(item), "dict_name",
				    fullname_utf8); 
#else
		item = gtk_menu_item_new_with_label(dict->dictname);
		gtk_object_set_data(GTK_OBJECT(item), "dict_name",
				    dict->fullname); 
#endif /* WIN32 */
					 
		gtk_menu_append(GTK_MENU(menu), item);					 
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);

	return menu;
}

gchar *gtkaspell_get_dictionary_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	gchar *dict_fullname;
	gchar *label;

	g_return_val_if_fail(GTK_IS_MENU(menu), NULL);

	menuitem = gtk_menu_get_active(GTK_MENU(menu));
        dict_fullname = (gchar *) gtk_object_get_data(GTK_OBJECT(menuitem), 
						      "dict_name");
        g_return_val_if_fail(dict_fullname, NULL);

	label = g_strdup(dict_fullname);

        return label;
  
}

gint gtkaspell_set_dictionary_menu_active_item(GtkWidget *menu, const gchar *dictionary)
{
	GList *cur;
	gint n;

	g_return_val_if_fail(menu != NULL, 0);
	g_return_val_if_fail(dictionary != NULL, 0);
	g_return_val_if_fail(GTK_IS_OPTION_MENU(menu), 0);

	n = 0;
	for (cur = GTK_MENU_SHELL(gtk_option_menu_get_menu(GTK_OPTION_MENU(menu)))->children;
	     cur != NULL; cur = cur->next) {
		GtkWidget *menuitem;
		gchar *dict_name;

		menuitem = GTK_WIDGET(cur->data);
		dict_name = gtk_object_get_data(GTK_OBJECT(menuitem), 
						"dict_name");
		if ((dict_name != NULL) && !strcmp2(dict_name, dictionary)) {
			gtk_option_menu_set_history(GTK_OPTION_MENU(menu), n);

			return 1;
		}
		n++;
	}

	return 0;
}

GtkWidget *gtkaspell_sugmode_option_menu_new(gint sugmode)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	item = gtk_menu_item_new_with_label(_("Fast Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_object_set_data(GTK_OBJECT(item), "sugmode", GINT_TO_POINTER(ASPELL_FASTMODE));

	item = gtk_menu_item_new_with_label(_("Normal Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_object_set_data(GTK_OBJECT(item), "sugmode", GINT_TO_POINTER(ASPELL_NORMALMODE));
	
	item = gtk_menu_item_new_with_label(_("Bad Spellers Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_object_set_data(GTK_OBJECT(item), "sugmode", GINT_TO_POINTER(ASPELL_BADSPELLERMODE));

	return menu;
}
	
void gtkaspell_sugmode_option_menu_set(GtkOptionMenu *optmenu, gint sugmode)
{
	g_return_if_fail(GTK_IS_OPTION_MENU(optmenu));

	g_return_if_fail(sugmode == ASPELL_FASTMODE ||
			 sugmode == ASPELL_NORMALMODE ||
			 sugmode == ASPELL_BADSPELLERMODE);

	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), sugmode - 1);
}

gint gtkaspell_get_sugmode_from_option_menu(GtkOptionMenu *optmenu)
{
	gint sugmode;
	GtkWidget *item;
	
	g_return_val_if_fail(GTK_IS_OPTION_MENU(optmenu), -1);

	item = gtk_menu_get_active(GTK_MENU(gtk_option_menu_get_menu(optmenu)));
	
	sugmode = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item),
						      "sugmode"));

	return sugmode;
}

static void use_alternate_dict(GtkAspell *gtkaspell)
{
	GtkAspeller *tmp;

	tmp = gtkaspell->gtkaspeller;
	gtkaspell->gtkaspeller = gtkaspell->alternate_speller;
	gtkaspell->alternate_speller = tmp;

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);
}

static void popup_menu(GtkAspell *gtkaspell, GdkEventButton *eb) 
{
	GtkSText * gtktext;
	
	gtktext = gtkaspell->gtktext;
	gtkaspell->orig_pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

	if (!(eb->state & GDK_SHIFT_MASK)) {
		if (check_at(gtkaspell, gtkaspell->orig_pos)) {

			gtk_editable_set_position(GTK_EDITABLE(gtktext), 
						  gtkaspell->orig_pos);
			gtk_stext_set_point(gtktext, gtkaspell->orig_pos);

			if (misspelled_suggest(gtkaspell, gtkaspell->theword)) {
				gtk_menu_popup(make_sug_menu(gtkaspell), 
					       NULL, NULL, NULL, NULL,
					       eb->button, GDK_CURRENT_TIME);
				
				return;
			}
		} else {
			gtk_editable_set_position(GTK_EDITABLE(gtktext), 
						  gtkaspell->orig_pos);
			gtk_stext_set_point(gtktext, gtkaspell->orig_pos);
		}
	}

	gtk_menu_popup(make_config_menu(gtkaspell), NULL, NULL, NULL, NULL,
		       eb->button, GDK_CURRENT_TIME);
}

/* make_sug_menu() - Add menus to accept this word for this session 
 * and to add it to personal dictionary 
 */
static GtkMenu *make_sug_menu(GtkAspell *gtkaspell) 
{
	GtkWidget 	*menu, *item;
	unsigned char	*caption;
	GtkSText 	*gtktext;
	GtkAccelGroup 	*accel;
	GList 		*l = gtkaspell->suggestions_list;

	gtktext = gtkaspell->gtktext;

	accel = gtk_accel_group_new();
	menu = gtk_menu_new(); 

	if (gtkaspell->sug_menu)
		gtk_widget_destroy(gtkaspell->sug_menu);

	gtkaspell->sug_menu = menu;	

	gtk_signal_connect(GTK_OBJECT(menu), "cancel",
		GTK_SIGNAL_FUNC(cancel_menu_cb), gtkaspell);

	caption = g_strdup_printf(_("\"%s\" unknown in %s"), 
				  (unsigned char*) l->data, 
				  gtkaspell->gtkaspeller->dictionary->dictname);
#ifdef WIN32
	locale_to_utf8(&caption);
#endif
	item = gtk_menu_item_new_with_label(caption);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_misc_set_alignment(GTK_MISC(GTK_BIN(item)->child), 0.5, 0.5);
	g_free(caption);

	item = gtk_menu_item_new();
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_menu_item_new_with_label(_("Accept in this session"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(add_word_to_session_cb), 
			   gtkaspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_space,
				   GDK_MOD1_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

	item = gtk_menu_item_new_with_label(_("Add to personal dictionary"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(add_word_to_personal_cb), 
			   gtkaspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_Return,
				   GDK_MOD1_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

        item = gtk_menu_item_new_with_label(_("Replace with..."));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(replace_with_create_dialog_cb), 
			   gtkaspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_R, 0,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

	if (gtkaspell->use_alternate && gtkaspell->alternate_speller) {
		caption = g_strdup_printf(_("Check with %s"), 
			gtkaspell->alternate_speller->dictionary->dictname);
		item = gtk_menu_item_new_with_label(caption);
		g_free(caption);
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu), item);
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				GTK_SIGNAL_FUNC(check_with_alternate_cb),
				gtkaspell);
		gtk_widget_add_accelerator(item, "activate", accel, GDK_X, 0,
					   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);
	}

	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

	l = l->next;
        if (l == NULL) {
		item = gtk_menu_item_new_with_label(_("(no suggestions)"));
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu), item);
        } else {
		GtkWidget *curmenu = menu;
		gint count = 0;
		
		do {
			if (count == MENUCOUNT) {
				count -= MENUCOUNT;

				item = gtk_menu_item_new_with_label(_("More..."));
				gtk_widget_show(item);
				gtk_menu_append(GTK_MENU(curmenu), item);

				curmenu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
							  curmenu);
			}

#ifdef WIN32
			{
				gchar *locdata = g_strdup((unsigned char*)l->data);
				locale_to_utf8(&locdata);
				item = gtk_menu_item_new_with_label(locdata);

			}
#else
			item = gtk_menu_item_new_with_label((unsigned char*)l->data);
#endif
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(curmenu), item);
			gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(replace_word_cb),
					   gtkaspell);

			if (curmenu == menu && count < MENUCOUNT) {
				gtk_widget_add_accelerator(item, "activate",
							   accel,
							   GDK_A + count, 0,
							   GTK_ACCEL_LOCKED | 
							   GTK_ACCEL_VISIBLE);
				gtk_widget_add_accelerator(item, "activate", 
							   accel,
							   GDK_A + count, 
							   GDK_MOD1_MASK,
							   GTK_ACCEL_LOCKED);
				}

			count++;

		} while ((l = l->next) != NULL);
	}

	gtk_accel_group_attach(accel, GTK_OBJECT(menu));
	gtk_accel_group_unref(accel);
	
	return GTK_MENU(menu);
}

static void populate_submenu(GtkAspell *gtkaspell, GtkWidget *menu)
{
	GtkWidget *item, *submenu;
	gchar *dictname;
	GtkAspeller *gtkaspeller = gtkaspell->gtkaspeller;

	if (GTK_MENU_SHELL(menu)->children) {
		GList *amenu, *alist;
		for (amenu = (GTK_MENU_SHELL(menu)->children); amenu; ) {
			alist = amenu->next;
			gtk_widget_destroy(GTK_WIDGET(amenu->data));
			amenu = alist;
		}
	}
	
	dictname = g_strdup_printf(_("Dictionary: %s"),
				   gtkaspeller->dictionary->dictname);
	item = gtk_menu_item_new_with_label(dictname);
	gtk_misc_set_alignment(GTK_MISC(GTK_BIN(item)->child), 0.5, 0.5);
	g_free(dictname);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
		
	if (gtkaspell->use_alternate && gtkaspell->alternate_speller) {
		dictname = g_strdup_printf(_("Use alternate (%s)"), 
				gtkaspell->alternate_speller->dictionary->dictname);
		item = gtk_menu_item_new_with_label(dictname);
		g_free(dictname);
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(switch_to_alternate_cb),
				   gtkaspell);
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu), item);
	}

      	item = gtk_check_menu_item_new_with_label(_("Fast Mode"));
	if (gtkaspell->gtkaspeller->sug_mode == ASPELL_FASTMODE) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(item),FALSE);
	} else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode_cb),
				   gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_check_menu_item_new_with_label(_("Normal Mode"));
	if (gtkaspell->gtkaspeller->sug_mode == ASPELL_NORMALMODE) {
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	} else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode_cb),
				   gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu),item);

	item = gtk_check_menu_item_new_with_label(_("Bad Spellers Mode"));
	if (gtkaspell->gtkaspeller->sug_mode == ASPELL_BADSPELLERMODE) {
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	} else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode_cb),
				   gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	
	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
	
	item = gtk_check_menu_item_new_with_label(_("Check while typing"));
	if (gtkaspell->check_while_typing)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	else	
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(toggle_check_while_typing_cb),
			   gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

	submenu = gtk_menu_new();
        item = gtk_menu_item_new_with_label(_("Change dictionary"));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),submenu);
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

	/* Dict list */
        if (gtkaspellcheckers->dictionary_list == NULL)
		gtkaspell_get_dictionary_list(gtkaspell->dictionary_path, FALSE);
        {
		GtkWidget * curmenu = submenu;
		int count = 0;
		Dictionary *dict;
		GSList *tmp;
		tmp = gtkaspellcheckers->dictionary_list;
		
		for (tmp = gtkaspellcheckers->dictionary_list; tmp != NULL; 
				tmp = g_slist_next(tmp)) {
			if (count == MENUCOUNT) {
				GtkWidget *newmenu;
				
				newmenu = gtk_menu_new();
				item = gtk_menu_item_new_with_label(_("More..."));
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), 
							  newmenu);
				
				gtk_menu_append(GTK_MENU(curmenu), item);
				gtk_widget_show(item);
				curmenu = newmenu;
				count = 0;
			}
			dict = (Dictionary *) tmp->data;
			item = gtk_check_menu_item_new_with_label(dict->dictname);
			gtk_object_set_data(GTK_OBJECT(item), "dict_name",
					    dict->fullname); 
			if (strcmp2(dict->fullname,
			    gtkaspell->gtkaspeller->dictionary->fullname))
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
			else {
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
				gtk_widget_set_sensitive(GTK_WIDGET(item),
							 FALSE);
			}
			gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(change_dict_cb),
					   gtkaspell);
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(curmenu), item);
			
			count++;
		}
        }  
}

static GtkMenu *make_config_menu(GtkAspell *gtkaspell)
{
	if (!gtkaspell->popup_config_menu)
		gtkaspell->popup_config_menu = gtk_menu_new();

	debug_print("Aspell: creating/using popup_config_menu %0x\n", 
			(guint) gtkaspell->popup_config_menu);
	populate_submenu(gtkaspell, gtkaspell->popup_config_menu);

        return GTK_MENU(gtkaspell->popup_config_menu);
}

void gtkaspell_populate_submenu(GtkAspell *gtkaspell, GtkWidget *menuitem)
{
	GtkWidget *menu;

	menu = GTK_WIDGET(GTK_MENU_ITEM(menuitem)->submenu);
	
	debug_print("Aspell: using config menu %0x\n", 
			(guint) gtkaspell->popup_config_menu);
	populate_submenu(gtkaspell, menu);
	
	gtkaspell->config_menu = menu;
	
}

static void set_menu_pos(GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	GtkAspell 	*gtkaspell = (GtkAspell *) data;
	gint 		 xx = 0, yy = 0;
	gint 		 sx,     sy;
	gint 		 wx,     wy;
	GtkSText 	*text = GTK_STEXT(gtkaspell->gtktext);
	GtkRequisition 	 r;

	gdk_window_get_origin(GTK_WIDGET(gtkaspell->gtktext)->window, &xx, &yy);
	
	sx = gdk_screen_width();
	sy = gdk_screen_height();
	
	gtk_widget_get_child_requisition(GTK_WIDGET(menu), &r);
	
	wx =  r.width;
	wy =  r.height;
	
	*x = gtkaspell->gtktext->cursor_pos_x + xx +
	     gdk_char_width(GTK_WIDGET(text)->style->font, ' ');
	*y = gtkaspell->gtktext->cursor_pos_y + yy;

	if (*x + wx > sx)
		*x = sx - wx;
	if (*y + wy > sy)
		*y = *y - wy - 
		     gdk_string_height((GTK_WIDGET(gtkaspell->gtktext))->style->font, 
				       gtkaspell->theword);

}

/* Menu call backs */

static gboolean cancel_menu_cb(GtkMenuShell *w, gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;

	gtkaspell->continue_check = NULL;
	set_point_continue(gtkaspell);

	return FALSE;
	
}

/* change_dict_cb() - Menu callback : change dict */
static void change_dict_cb(GtkWidget *w, GtkAspell *gtkaspell)
{
	Dictionary 	*dict;       
	gchar		*fullname;
	GtkAspeller 	*gtkaspeller;
	gint 		 sug_mode;
  
        fullname = (gchar *) gtk_object_get_data(GTK_OBJECT(w), "dict_name");
	
	if (!strcmp2(fullname, _("None")))
		return;

	sug_mode  = gtkaspell->default_sug_mode;

	dict = g_new0(Dictionary, 1);
	dict->fullname = g_strdup(fullname);
	dict->encoding = g_strdup(gtkaspell->gtkaspeller->dictionary->encoding);

	if (gtkaspell->use_alternate && gtkaspell->alternate_speller &&
	    dict == gtkaspell->alternate_speller->dictionary) {
		use_alternate_dict(gtkaspell);
		dictionary_delete(dict);
		return;
	}
	
	gtkaspeller = gtkaspeller_new(dict);

	if (!gtkaspeller) {
		gchar *message;
		message = g_strdup_printf(_("The spell checker could not change dictionary.\n%s"), 
					  gtkaspellcheckers->error_message);

		gtkaspell_alert_dialog(message); 
		g_free(message);
	} else {
		if (gtkaspell->use_alternate) {
			if (gtkaspell->alternate_speller)
				gtkaspeller_delete(gtkaspell->alternate_speller);
			gtkaspell->alternate_speller = gtkaspell->gtkaspeller;
		} else
			gtkaspeller_delete(gtkaspell->gtkaspeller);

		gtkaspell->gtkaspeller = gtkaspeller;
		gtkaspell_set_sug_mode(gtkaspell, sug_mode);
	}
	
	dictionary_delete(dict);

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);
}

static void switch_to_alternate_cb(GtkWidget *w,
				   gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;
	use_alternate_dict(gtkaspell);
}

/* Misc. helper functions */

static void set_point_continue(GtkAspell *gtkaspell)
{
	GtkSText  *gtktext;

	gtktext = gtkaspell->gtktext;

	gtk_stext_freeze(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext),gtkaspell->orig_pos);
	gtk_stext_set_point(gtktext, gtkaspell->orig_pos);
	gtk_stext_thaw(gtktext);

	if (gtkaspell->continue_check)
		gtkaspell->continue_check((gpointer *) gtkaspell);
}

static void allocate_color(GtkAspell *gtkaspell, gint rgbvalue)
{
	GdkColormap *gc;
	GdkColor *color = &(gtkaspell->highlight);

	gc = gtk_widget_get_colormap(GTK_WIDGET(gtkaspell->gtktext));

	if (gtkaspell->highlight.pixel)
		gdk_colormap_free_colors(gc, &(gtkaspell->highlight), 1);

	/* Shameless copy from Sylpheed's gtkutils.c */
	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0)
			* 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0)
			* 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0)
			* 65535.0);

	gdk_colormap_alloc_color(gc, &(gtkaspell->highlight), FALSE, TRUE);
}

static void change_color(GtkAspell * gtkaspell, 
			 gint start, gint end,
			 gchar *newtext,
                         GdkColor *color) 
{
	GtkSText *gtktext;

	g_return_if_fail(start < end);
    
	gtktext = gtkaspell->gtktext;
    
	gtk_stext_freeze(gtktext);
	if (newtext) {
		gtk_stext_set_point(gtktext, start);
		gtk_stext_forward_delete(gtktext, end - start);
		gtk_stext_insert(gtktext, NULL, color, NULL, newtext,
				 end - start);
	}
	gtk_stext_thaw(gtktext);
}

/* convert_to_aspell_encoding () - converts ISO-8859-* strings to iso8859-* 
 * as needed by aspell. Returns an allocated string.
 */

static guchar *convert_to_aspell_encoding (const guchar *encoding)
{
	guchar * aspell_encoding;

	if (strstr2(encoding, "ISO-8859-")) {
		aspell_encoding = g_strdup_printf("iso8859%s", encoding+8);
	}
	else {
		if (!strcmp2(encoding, "US-ASCII"))
			aspell_encoding = g_strdup("iso8859-1");
		else
			aspell_encoding = g_strdup(encoding);
	}

	return aspell_encoding;
}

/* compare_dict () - compare 2 dict names */
static gint compare_dict(Dictionary *a, Dictionary *b)
{
	guint   aparts = 0,  bparts = 0;
	guint  	i;

	for (i=0; i < strlen(a->dictname); i++)
		if (a->dictname[i] == '-')
			aparts++;
	for (i=0; i < strlen(b->dictname); i++)
		if (b->dictname[i] == '-')
			bparts++;

	if (aparts != bparts) 
		return (aparts < bparts) ? -1 : +1;
	else {
		gint compare;
		compare = strcmp2(a->dictname, b->dictname);
		if (!compare)
			compare = strcmp2(a->fullname, b->fullname);
		return compare;
	}
}

static void dictionary_delete(Dictionary *dict)
{
	g_free(dict->fullname);
	g_free(dict->encoding);
	g_free(dict);
}

static Dictionary *dictionary_dup(const Dictionary *dict)
{
	Dictionary *dict2;

	dict2 = g_new(Dictionary, 1); 

	dict2->fullname = g_strdup(dict->fullname);
	dict2->dictname = dict->dictname - dict->fullname + dict2->fullname;
	dict2->encoding = g_strdup(dict->encoding);

	return dict2;
}

static void free_suggestions_list(GtkAspell *gtkaspell)
{
	GList *list;

	for (list = gtkaspell->suggestions_list; list != NULL;
	     list = list->next)
		g_free(list->data);

	g_list_free(list);
	
	gtkaspell->max_sug          = -1;
	gtkaspell->suggestions_list = NULL;
}

static void reset_theword_data(GtkAspell *gtkaspell)
{
	gtkaspell->start_pos     =  0;
	gtkaspell->end_pos       =  0;
	gtkaspell->theword[0]    =  0;
	gtkaspell->max_sug       = -1;

	free_suggestions_list(gtkaspell);
}

static void free_checkers(gpointer elt, gpointer data)
{
	GtkAspeller *gtkaspeller = elt;

	g_return_if_fail(gtkaspeller);

	gtkaspeller_real_delete(gtkaspeller);
}

static gint find_gtkaspeller(gconstpointer aa, gconstpointer bb)
{
	Dictionary *a = ((GtkAspeller *) aa)->dictionary;
	Dictionary *b = ((GtkAspeller *) bb)->dictionary;

	if (a && b && a->fullname && b->fullname  &&
	    strcmp(a->fullname, b->fullname) == 0 &&
	    a->encoding && b->encoding)
		return strcmp(a->encoding, b->encoding);

	return 1;
}

static void gtkaspell_alert_dialog(gchar *message)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *ok_button;

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_MOUSE);
	gtk_signal_connect_object(GTK_OBJECT(dialog), "destroy",
				   GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				   GTK_OBJECT(dialog));

	label  = gtk_label_new(message);
	gtk_misc_set_padding(GTK_MISC(label), 8, 8);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	
	hbox = gtk_hbox_new(FALSE, 0);

	ok_button = gtk_button_new_with_label(_("OK"));
	GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbox), ok_button, TRUE, TRUE, 8);	

	gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
				   GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				   GTK_OBJECT(dialog));
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), hbox);
			
	gtk_widget_grab_default(ok_button);
	gtk_widget_grab_focus(ok_button);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	gtk_widget_show_all(dialog);
}
#endif
