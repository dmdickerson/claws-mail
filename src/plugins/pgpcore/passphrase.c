/* passphrase.c - GTK+ based passphrase callback
 *      Copyright (C) 2001-2006 Werner Koch (dd9jn) and the Sylpheed-Claws team
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
#  include <config.h>
#endif

#if USE_GPGME

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkdisplay.h>
#ifdef GDK_WINDOWING_X11
#  include <gdk/gdkx.h>
#endif /* GDK_WINDOWING_X11 */
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtksignal.h>
#include <string.h>
#include <sys/types.h>
#ifdef G_OS_WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "passphrase.h"
#include "prefs_common.h"
#include "prefs_gpg.h"
#include "manage_window.h"
#include "utils.h"
#include "mainwindow.h"
#include "summaryview.h"

static gboolean grab_all = FALSE;

static gboolean pass_ack;
static gchar *last_pass = NULL;

static void passphrase_ok_cb(GtkWidget *widget, gpointer data);
static void passphrase_cancel_cb(GtkWidget *widget, gpointer data);
static gint passphrase_deleted(GtkWidget *widget, GdkEventAny *event,
			       gpointer data);
static gboolean passphrase_key_pressed(GtkWidget *widget, GdkEventKey *event,
				       gpointer data);
static gchar* passphrase_mbox(const gchar *uid_hint, const gchar *pass_hint,
			      gint prev_bad);

static GtkWidget *create_description(const gchar *uid_hint,
				     const gchar *pass_hint, gint prev_bad);

void
gpgmegtk_set_passphrase_grab(gint yes)
{
    grab_all = yes;
}

static gchar*
passphrase_mbox(const gchar *uid_hint, const gchar *pass_hint, gint prev_bad)
{
    gchar *the_passphrase = NULL;
    GtkWidget *vbox, *hbox;
    GtkWidget *confirm_box;
    GtkWidget *window;
    GtkWidget *pass_entry;
    GtkWidget *ok_button;
    GtkWidget *cancel_button;

    SummaryView *summaryview = mainwindow_get_mainwindow()->summaryview;
    
    gtk_menu_popdown(GTK_MENU(summaryview->popupmenu));

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Passphrase"));
    gtk_widget_set_size_request(window, 450, -1);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
    g_signal_connect(G_OBJECT(window), "delete_event",
                     G_CALLBACK(passphrase_deleted), NULL);
    g_signal_connect(G_OBJECT(window), "key_press_event",
                     G_CALLBACK(passphrase_key_pressed), NULL);
    MANAGE_WINDOW_SIGNALS_CONNECT(window);
    manage_window_set_transient(GTK_WINDOW(window));

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    if (uid_hint || pass_hint) {
        GtkWidget *label, *icon;
        label = create_description (uid_hint, pass_hint, prev_bad);
	icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION,
        			GTK_ICON_SIZE_DIALOG); 

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_widget_show (hbox);
        gtk_box_pack_start (GTK_BOX(hbox), icon, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    }

    pass_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), pass_entry, FALSE, FALSE, 0);
    gtk_entry_set_visibility(GTK_ENTRY(pass_entry), FALSE);
    gtk_widget_grab_focus(pass_entry);

    gtkut_stock_button_set_create(&confirm_box, 
				  &cancel_button, GTK_STOCK_CANCEL,
		    		  &ok_button, GTK_STOCK_OK,
				  NULL, NULL);
    gtk_box_pack_end(GTK_BOX(vbox), confirm_box, FALSE, FALSE, 0);
    gtk_widget_grab_default(ok_button);

    g_signal_connect(G_OBJECT(ok_button), "clicked",
                     G_CALLBACK(passphrase_ok_cb), NULL);
    g_signal_connect(G_OBJECT(pass_entry), "activate",
                     G_CALLBACK(passphrase_ok_cb), NULL);
    g_signal_connect(G_OBJECT(cancel_button), "clicked",
                     G_CALLBACK(passphrase_cancel_cb), NULL);

    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    if (grab_all)   
        gtk_window_set_policy (GTK_WINDOW(window), FALSE, FALSE, TRUE);
    
    gtk_widget_show_all(window);

    if (grab_all) {
        int err = 0, cnt = 0;
        /* make sure that window is viewable */
        gtk_widget_show_now(window);
	gdk_window_process_updates(window->window, TRUE);
	gdk_flush();
	while(gtk_events_pending()) {
		gtk_main_iteration();
	}
try_again:
        if ((err = gdk_pointer_grab(window->window, TRUE, 0,
                             window->window, NULL, GDK_CURRENT_TIME))) {
	    if (err == GDK_GRAB_NOT_VIEWABLE && cnt < 10) {
	        cnt++;
		g_warning("trying to grab mouse again\n");
		gtk_main_iteration();
		goto try_again;
            } else {
                g_warning("OOPS: Could not grab mouse\n");
                gtk_widget_destroy(window);
                return NULL;
	    }
        }
        if (gdk_keyboard_grab(window->window, FALSE, GDK_CURRENT_TIME)) {
            gdk_display_pointer_ungrab(gdk_display_get_default(),
			 	       GDK_CURRENT_TIME);
            g_warning("OOPS: Could not grab keyboard\n");
            gtk_widget_destroy(window);
            return NULL;
        }
    }

    gtk_main();

    if (grab_all) {
        gdk_display_keyboard_ungrab(gdk_display_get_default(),
				    GDK_CURRENT_TIME);
        gdk_display_pointer_ungrab(gdk_display_get_default(), GDK_CURRENT_TIME);
        gdk_flush();
    }

    manage_window_focus_out(window, NULL, NULL);

    if (pass_ack) {
        const gchar *entry_text;
        entry_text = gtk_entry_get_text(GTK_ENTRY(pass_entry));
	the_passphrase = g_locale_from_utf8(entry_text, -1, NULL, NULL, NULL);
        if (the_passphrase == NULL) 
            the_passphrase = g_strdup (entry_text);
    }
    gtk_widget_destroy (window);

    return the_passphrase;
}


static void 
passphrase_ok_cb(GtkWidget *widget, gpointer data)
{
    pass_ack = TRUE;
    gtk_main_quit();
}

static void 
passphrase_cancel_cb(GtkWidget *widget, gpointer data)
{
    pass_ack = FALSE;
    gtk_main_quit();
}


static gint
passphrase_deleted(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    passphrase_cancel_cb(NULL, NULL);
    return TRUE;
}


static gboolean
passphrase_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event && event->keyval == GDK_Escape)
        passphrase_cancel_cb(NULL, NULL);
    return FALSE;
}

static gint 
linelen (const gchar *s)
{
    gint i;

    for (i = 0; *s && *s != '\n'; s++, i++)
        ;

    return i;
}

static GtkWidget *
create_description(const gchar *uid_hint, const gchar *pass_hint, gint prev_bad)
{
    const gchar *uid = NULL, *info = NULL;
    gchar *buf;
    GtkWidget *label;
    gchar *my_uid = NULL;
    if (!uid_hint)
        uid = _("[no user id]");
    else
        uid = uid_hint;
    if (!pass_hint)
        info = "";
    else
        info = pass_hint;

    my_uid = g_strdup(uid);
    while (strchr(my_uid, '<')) 
    	*(strchr(my_uid, '<')) = '(';
    while (strchr(my_uid, '>')) 
    	*(strchr(my_uid, '>')) = ')';

    buf = g_strdup_printf (_("<span weight=\"bold\" size=\"larger\">%sPlease enter the passphrase for:</span>\n\n"
                           "%.*s\n"),
                           prev_bad ?
                           _("Bad passphrase.\n") : "",
                           linelen (my_uid), my_uid);
    g_free(my_uid);
    label = gtk_label_new (buf);
    gtk_label_set_use_markup(GTK_LABEL (label), TRUE);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL (label), TRUE);
    g_free (buf);

    return label;
}

static int free_passphrase(gpointer _unused)
{
    if (last_pass != NULL) {
#ifndef G_PLATFORM_WIN32
        munlock(last_pass, strlen(last_pass));
#endif
        g_free(last_pass);
        last_pass = NULL;
        debug_print("%% passphrase removed");
    }
    
    return FALSE;
}

gpgme_error_t
gpgmegtk_passphrase_cb(void *opaque, const char *uid_hint,
        const char *passphrase_hint, int prev_bad, int fd)
{
    const char *pass;

    if (prefs_gpg_get_config()->store_passphrase && last_pass && !prev_bad)
        pass = last_pass;
    else {
    gpgmegtk_set_passphrase_grab (prefs_gpg_get_config()->passphrase_grab);
    debug_print ("%% requesting passphrase for '%s': ", uid_hint);
    pass = passphrase_mbox (uid_hint, passphrase_hint, prev_bad);
    gpgmegtk_free_passphrase();
    if (!pass) {
        debug_print ("%% cancel passphrase entry");
        write(fd, "\n", 1);
        return GPG_ERR_CANCELED;
    }
    else {
        if (prefs_gpg_get_config()->store_passphrase) {
            last_pass = g_strdup(pass);
#ifndef G_PLATFORM_WIN32
            if (mlock(last_pass, strlen(last_pass)) == -1)
                debug_print("%% locking passphrase failed");
#endif
            if (prefs_gpg_get_config()->store_passphrase_timeout > 0) {
                    gtk_timeout_add(prefs_gpg_get_config()
                                    ->store_passphrase_timeout*60*1000,
                                free_passphrase, NULL);
            }
        }
        debug_print ("%% sending passphrase");
    }
    }

#ifdef G_OS_WIN32
    {
        /* Under Windows FD is actually a System handle. */
        DWORD nwritten;
        WriteFile ((HANDLE)fd, pass, strlen (pass), &nwritten, NULL);
        WriteFile ((HANDLE)fd, "\n", 1, &nwritten, NULL);
    }
#else
    write(fd, pass, strlen(pass));
    write(fd, "\n", 1);
#endif
    return GPG_ERR_NO_ERROR;
}

void gpgmegtk_free_passphrase()
{
    (void)free_passphrase(NULL); /* could be inline */
}

#endif /* USE_GPGME */