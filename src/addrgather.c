/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Match Grun
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
* Dialog for gathering EMail addresses from mail folder.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
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

#include "intl.h"
#include "main.h"
#include "inc.h"
#include "mbox.h"
#include "filesel.h"
#include "foldersel.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "folder.h"
#include "utils.h"

#include "addrharvest.h"
#include "addrindex.h"
#include "addrbook.h"

#define PAGE_WARNING    0
#define PAGE_FIELDS     1
#define PAGE_FINISH     2

#define NUM_FIELDS      6

#define FIELDS_N_COLS              2
#define FIELDS_COL_WIDTH_HEADER    100
#define FIELDS_COL_WIDTH_COUNT     140

#define MIN_FOLDER_SIZE 20
#define DFL_FOLDER_SIZE 50

typedef enum {
	FIELD_COL_HEADER = 0,
	FIELD_COL_COUNT  = 1
} AddrHarvest;

/*
* The dialog.
*/
static struct _AddrHarvest {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *labelFolder;
	GtkWidget *entryBook;
	GtkWidget *checkHeader[ NUM_FIELDS ];
	GtkWidget *spinbtnFolder;
	GtkWidget *checkRecurse;
	GtkWidget *btnOk;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	gint      status_cid;
	gboolean  cancelled;
	gchar     *folderPath;
	GtkWidget *clistCount;
} addrgather_dlg;

static AddressIndex *_harv_addressIndex_;
static AddressBookFile *_harv_addressBook_;
static gchar *_harv_headerNames_[] = {
	HEADER_FROM,
	HEADER_REPLY_TO,
	HEADER_SENDER,
	HEADER_TO,
	HEADER_CC,
	HEADER_ERRORS_TO
};
static GList *_harv_messageList_;

void addrgather_dlg_status_show( gchar *msg ) {
	if( addrgather_dlg.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(addrgather_dlg.statusbar),
			addrgather_dlg.status_cid );
		if( msg ) {
			gtk_statusbar_push(
				GTK_STATUSBAR(addrgather_dlg.statusbar),
				addrgather_dlg.status_cid, msg );
		}
	}
}

static gint addrgather_dlg_delete_event(
	GtkWidget *widget, GdkEventAny *event, gpointer data )
{
	addrgather_dlg.cancelled = TRUE;
	gtk_main_quit();
	return TRUE;
}

static void addrgather_dlg_key_pressed(
	GtkWidget *widget, GdkEventKey *event, gpointer data )
{
	if( event && event->keyval == GDK_Escape ) {
		addrgather_dlg.cancelled = TRUE;
		gtk_main_quit();
	}
}

#define FMT_BUFSIZE 32

static gboolean addrgather_dlg_harvest() {
	GtkCList *clist;
	gchar *text[ FIELDS_N_COLS ];
	AddressHarvester *harvester;
	AddressBookFile *abf;
	gchar *name;
	gchar *newFile;
	gchar str[ FMT_BUFSIZE ];
	gint cnt;
	gint i;
	gint sz;

	name = gtk_editable_get_chars( GTK_EDITABLE(addrgather_dlg.entryBook), 0, -1 );
	if( name == NULL || strlen( name ) < 1 ) {
		addrgather_dlg_status_show(
			_( "Please specify name for address book." ) );
		g_free( name );
		return FALSE;
	}

	/* Create harvest helper */
	harvester = addrharvest_create();
	addrharvest_set_path( harvester, addrgather_dlg.folderPath );

	for( i = 0; i < NUM_FIELDS; i++ ) {
		addrharvest_set_header( harvester, _harv_headerNames_[i],
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(addrgather_dlg.checkHeader[i]) ) );
	}
	addrharvest_set_recurse( harvester,
		gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON( addrgather_dlg.checkRecurse ) ) );

	if( addrharvest_check_header( harvester ) == FALSE ) {
		addrgather_dlg_status_show(
			_( "Please select the mail headers to search." ) );
		addrharvest_free( harvester );
		return FALSE;
	}

	/* Go fer it */
	addrgather_dlg_status_show( _( "Busy harvesting addresses..." ) );
	sz = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON( addrgather_dlg.spinbtnFolder ) );
	addrharvest_set_folder_size( harvester, sz );

	/* Create address book */
	abf = addrbook_create_book();
	addrbook_set_path( abf, _harv_addressIndex_->filePath );
	newFile = addrbook_guess_next_file( abf );
	addrbook_set_file( abf, newFile );
	addrbook_set_name( abf, name );
	g_free( newFile );
	g_free( name );

	/* Harvest addresses */
	addrharvest_harvest(
		harvester, abf->addressCache, _harv_messageList_ );
	addrbook_save_data( abf );
	_harv_addressBook_ = abf;

	/* Update summary count */
	clist = GTK_CLIST(addrgather_dlg.clistCount);
	gtk_clist_clear( clist );
	for( i = 0; i < NUM_FIELDS; i++ ) {
		cnt = addrharvest_get_count( harvester, _harv_headerNames_[i] );
		if( cnt < 1 ) {
			strcpy( str, "-" );
		}
		else {
			sprintf( str, "%d", cnt );
		}
		text[ FIELD_COL_HEADER ] = _harv_headerNames_[i];
		text[ FIELD_COL_COUNT  ] = str;
		gtk_clist_append( clist, text );
	}

	addrharvest_free( harvester );

	addrgather_dlg_status_show( _("Addresses gathered successfully.") );

	/* Display summary page */
	gtk_notebook_set_page(
		GTK_NOTEBOOK(addrgather_dlg.notebook), PAGE_FINISH );
	gtk_widget_set_sensitive( addrgather_dlg.btnOk, FALSE );
	gtk_widget_grab_default( addrgather_dlg.btnCancel );

	return TRUE;
}

static void addrgather_dlg_ok( GtkWidget *widget, gpointer data ) {
	if( addrgather_dlg_harvest() ) {
		addrgather_dlg.cancelled = FALSE;
	}
}

static void addrgather_dlg_cancel( GtkWidget *widget, gpointer data ) {
	gtk_main_quit();
}

#define PACK_CHECK_BUTTON(box, chkbtn, label) \
{ \
	chkbtn = gtk_check_button_new_with_label(label); \
	gtk_widget_show(chkbtn); \
	gtk_box_pack_start(GTK_BOX(box), chkbtn, FALSE, TRUE, 0); \
}

/*
 * Create notebook page for warning message.
 * Enter: pageNum Page number.
 *        pageLbl Page label.
 */
static void addrgather_page_warning( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	gint top;

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( addrgather_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( addrgather_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( addrgather_dlg.notebook ), pageNum ),
		label );

	table = gtk_table_new(3, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8 );

	/* First row */
	top = 0;
	label = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	/* First row */
	top++;
	label = gtk_label_new( _("No folder or message was selected." ) );
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment( GTK_MISC(label), 0, 0.5 );

	/* Second row */
	top++;
	label = gtk_label_new( _(
			"Please select a folder to process from the folder\n"
			"list. Alternatively, select one or messages from\n"
			"the message list." ) );

	gtk_table_attach( GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0);
	gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment( GTK_MISC(label), 0, 0.5 );
}

/*
 * Create notebook page for mail headers.
 * Enter: pageNum Page number.
 *        pageLbl Page label.
 */
static void addrgather_page_fields( gint pageNum, gchar *pageLbl ) {
	GtkWidget *vbox;
	GtkWidget *vboxf;
	GtkWidget *hboxs;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *labelFolder;
	GtkWidget *entryBook;
	GtkWidget *frameHeader;
	GtkWidget *checkHeader[ NUM_FIELDS ];
	GtkWidget *spinbtnFolder;
	GtkObject *adjFolder;
	GtkWidget *checkRecurse;
	gint top;
	gint i;

	/* Container */
	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( addrgather_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), 4 );

	/* Notebook page */
	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( addrgather_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( addrgather_dlg.notebook ), pageNum ),
		label );

	/* Upper area - Field list */
	table = gtk_table_new( 4, 2, FALSE);
	gtk_box_pack_start( GTK_BOX(vbox), table, FALSE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER(table), 8 );
	gtk_table_set_row_spacings( GTK_TABLE(table), 8 );
	gtk_table_set_col_spacings( GTK_TABLE(table), 8 );

	/* First row */
	top = 0;
	label = gtk_label_new( _("Folder :") );
	gtk_table_attach( GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0 );
	gtk_misc_set_alignment( GTK_MISC(label), 1.0, 0.5 );

	labelFolder = gtk_label_new( "" );
	gtk_table_attach( GTK_TABLE(table), labelFolder, 1, 2, top, (top + 1),
		GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0 );
	gtk_misc_set_alignment( GTK_MISC(labelFolder), 0, 0.5 );

	/* Second row */
	top = 1;
	label = gtk_label_new( _("Address Book :") );
	gtk_table_attach( GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0 );
	gtk_misc_set_alignment( GTK_MISC(label), 1.0, 0.5 );

	entryBook = gtk_entry_new();
	gtk_table_attach( GTK_TABLE(table), entryBook, 1, 2, top, (top + 1),
		GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0 );

	/* Third row */
	top = 2;
	label = gtk_label_new( _("Folder Size :") );
	gtk_table_attach( GTK_TABLE(table), label, 0, 1, top, (top + 1), GTK_FILL, 0, 0, 0 );
	gtk_misc_set_alignment( GTK_MISC(label), 1.0, 0.5 );

	hboxs = gtk_hbox_new( FALSE, 8 );
	adjFolder =
		gtk_adjustment_new( DFL_FOLDER_SIZE, MIN_FOLDER_SIZE, G_MAXINT, 1, 10, 100 );
	spinbtnFolder = gtk_spin_button_new( GTK_ADJUSTMENT(adjFolder), 1, 0 );
	gtk_box_pack_start( GTK_BOX(hboxs), spinbtnFolder, FALSE, FALSE, 0);
	gtk_widget_set_usize( spinbtnFolder, 100, -1);
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON(spinbtnFolder), TRUE );
	gtk_table_attach( GTK_TABLE(table), hboxs, 1, 2, top, (top + 1), GTK_FILL, 0, 0, 0 );

	/* Fourth row */
	top = 3;
	frameHeader = gtk_frame_new( _("Process these mail header fields") );
	gtk_widget_show( frameHeader );
	gtk_table_attach( GTK_TABLE(table), frameHeader, 0, 2, top, (top + 4), GTK_FILL, 0, 0, 0 );
	gtk_frame_set_label_align( GTK_FRAME(frameHeader), 0.01, 0.5 );

	/* Check boxes */
	vboxf = gtk_vbox_new( FALSE, 0 );
	gtk_widget_show( vboxf );
	gtk_container_add( GTK_CONTAINER( frameHeader ), vboxf );
	gtk_container_set_border_width (GTK_CONTAINER( vboxf ), 8);

	for( i = 0; i < NUM_FIELDS; i++ ) {
		PACK_CHECK_BUTTON( vboxf, checkHeader[i], _harv_headerNames_[i] );
		addrgather_dlg.checkHeader[i] = checkHeader[i];
	}

	/* Recurse folders */
	top += 4;
	checkRecurse = gtk_check_button_new_with_label( _("Include sub-folders" ) );
	gtk_table_attach( GTK_TABLE(table), checkRecurse, 0, 2, top, (top + 1),
			GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0 );

	addrgather_dlg.labelFolder   = labelFolder;
	addrgather_dlg.entryBook     = entryBook;
	addrgather_dlg.spinbtnFolder = spinbtnFolder;
	addrgather_dlg.checkRecurse  = checkRecurse;
}

/*
 * Create notebook page for summary counts.
 * Enter: pageNum Page number.
 *        pageLbl Page label.
 */
static void addrgather_page_finish( gint pageNum, gchar *pageLbl ) {
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *clistSWin;
	GtkWidget *clistCount;
	gchar *titles[ FIELDS_N_COLS ];
	gint i;

	titles[ FIELD_COL_HEADER ] = _("Header Name");
	titles[ FIELD_COL_COUNT  ] = _("Address Count");

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add( GTK_CONTAINER( addrgather_dlg.notebook ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER (vbox), 8 );

	label = gtk_label_new( pageLbl );
	gtk_widget_show( label );
	gtk_notebook_set_tab_label(
		GTK_NOTEBOOK( addrgather_dlg.notebook ),
		gtk_notebook_get_nth_page( GTK_NOTEBOOK( addrgather_dlg.notebook ), pageNum ),
		label );

	/* Summary count */
	clistSWin = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER(vbox), clistSWin );
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clistSWin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	clistCount = gtk_clist_new_with_titles( FIELDS_N_COLS, titles );
	gtk_container_add( GTK_CONTAINER(clistSWin), clistCount );
	gtk_clist_set_selection_mode( GTK_CLIST(clistCount), GTK_SELECTION_BROWSE );
	gtk_clist_set_column_width(
			GTK_CLIST(clistCount), FIELD_COL_HEADER, FIELDS_COL_WIDTH_HEADER );
	gtk_clist_set_column_width(
			GTK_CLIST(clistCount), FIELD_COL_COUNT, FIELDS_COL_WIDTH_COUNT );

	for( i = 0; i < FIELDS_N_COLS; i++ )
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clistCount)->column[i].button, GTK_CAN_FOCUS);

	addrgather_dlg.clistCount = clistCount;
}

/*
 * Create notebook page for warning message.
 * Enter: pageNum Page number.
 *        pageLbl Page label.
 */
static void addrgather_dlg_create( void ) {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *btnOk;
	GtkWidget *btnCancel;
	GtkWidget *statusbar;
	GtkWidget *vbox;
	GtkWidget *vnbox;
	GtkWidget *hbbox;
	GtkWidget *hsbox;

	window = gtk_window_new( GTK_WINDOW_DIALOG );
	gtk_widget_set_usize( window, 380, -1 );
	gtk_container_set_border_width(GTK_CONTAINER(window), 0);
	gtk_window_set_position( GTK_WINDOW(window), GTK_WIN_POS_CENTER );
	gtk_window_set_modal( GTK_WINDOW(window), TRUE );	
	gtk_signal_connect( GTK_OBJECT(window), "delete_event",
		GTK_SIGNAL_FUNC( addrgather_dlg_delete_event ), NULL );
	gtk_signal_connect( GTK_OBJECT(window), "key_press_event",
		GTK_SIGNAL_FUNC( addrgather_dlg_key_pressed ), NULL );

	vbox = gtk_vbox_new( FALSE, 8 );
	gtk_container_add( GTK_CONTAINER(window), vbox );
	gtk_container_set_border_width( GTK_CONTAINER(vbox), 0 );

	vnbox = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vnbox), 4);
	gtk_widget_show(vnbox);
	gtk_box_pack_start(GTK_BOX(vbox), vnbox, TRUE, TRUE, 0);

	/* Notebook */
	notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK(notebook), FALSE );
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vnbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);

	/* Status line */
	hsbox = gtk_hbox_new( FALSE, 0 );
	gtk_box_pack_end( GTK_BOX(vbox), hsbox, FALSE, FALSE, BORDER_WIDTH );
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start( GTK_BOX(hsbox), statusbar, TRUE, TRUE, BORDER_WIDTH );

	/* Button panel */
	gtkut_button_set_create( &hbbox, &btnOk, _("OK"),
		&btnCancel, _("Cancel"), NULL, NULL );
	gtk_box_pack_end( GTK_BOX(vbox), hbbox, FALSE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER(hbbox), 0 );

	/* Signal handlers */
	gtk_signal_connect( GTK_OBJECT(btnOk), "clicked",
		GTK_SIGNAL_FUNC(addrgather_dlg_ok), NULL );
	gtk_signal_connect( GTK_OBJECT(btnCancel), "clicked",
		GTK_SIGNAL_FUNC(addrgather_dlg_cancel), NULL );

	gtk_widget_show_all( vbox );
	addrgather_dlg.window     = window;
	addrgather_dlg.notebook   = notebook;
	addrgather_dlg.btnOk      = btnOk;
	addrgather_dlg.btnCancel  = btnCancel;
	addrgather_dlg.statusbar  = statusbar;
	addrgather_dlg.status_cid = gtk_statusbar_get_context_id(
		GTK_STATUSBAR(statusbar), "Harvest E-Mail Address Dialog" );

	/* Create notebook pages */
	addrgather_page_warning( PAGE_WARNING, _( "Warning" ) );
	addrgather_page_fields( PAGE_FIELDS, _( "Header Fields" ) );
	addrgather_page_finish( PAGE_FINISH, _( "Finish" ) );
	gtk_widget_show_all( addrgather_dlg.window );
}

/*
 * Harvest addresses main window.
 * Enter: folderItem Source folder.
 *        addrIndex  Address index.
 *        sourceInd  Source indicator: FALSE - Folder, TRUE - Messages.
 *        msgList    List of message numbers, or NULL to process folder.
 * Return: Populated address book file, or NULL if none created.
 */
AddressBookFile *addrgather_dlg_execute(
	FolderItem *folderItem, AddressIndex *addrIndex, gboolean sourceInd,
	GList *msgList )
{
	gboolean errFlag;
	gint i;

	_harv_addressIndex_ = addrIndex;
	_harv_addressBook_ = NULL;
	_harv_messageList_ = msgList;

	/* Create dialog */
	if( ! addrgather_dlg.window ) {
		addrgather_dlg_create();
	}

	errFlag = TRUE;
	if( folderItem && folderItem->path ) {
		gtk_notebook_set_page(
			GTK_NOTEBOOK(addrgather_dlg.notebook), PAGE_FIELDS );
		addrgather_dlg.folderPath = folder_item_get_path( folderItem );

		/* Setup some default values */
		gtk_label_set_text(
			GTK_LABEL(addrgather_dlg.labelFolder), folderItem->path );
		gtk_entry_set_text(
			GTK_ENTRY(addrgather_dlg.entryBook), folderItem->path );

		for( i = 0; i < NUM_FIELDS; i++ ) {
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(addrgather_dlg.checkHeader[i]),
				FALSE );
			if( g_strcasecmp( _harv_headerNames_[i], HEADER_FROM ) == 0 ) {
				gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(addrgather_dlg.checkHeader[i]),
					TRUE );
			}
		}

		gtk_widget_set_sensitive( addrgather_dlg.btnOk, TRUE );
		gtk_widget_grab_default( addrgather_dlg.btnOk );
		errFlag = FALSE;
	}

	/* Apply window title */
	if( sourceInd ) {
		gtk_window_set_title( GTK_WINDOW(addrgather_dlg.window),
			_("Harvest E-Mail Addresses - from Selected Messages") );
		gtk_widget_set_sensitive( addrgather_dlg.checkRecurse, FALSE );
		if( msgList == NULL ) {
			errFlag = TRUE;
		}
	}
	else {
		gtk_window_set_title( GTK_WINDOW(addrgather_dlg.window),
			_("Harvest E-Mail Addresses - from Folder") );
		gtk_widget_set_sensitive( addrgather_dlg.checkRecurse, TRUE );
	}
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON( addrgather_dlg.checkRecurse ), FALSE );

	addrgather_dlg_status_show( "" );
	gtk_widget_show( addrgather_dlg.window );

	if( errFlag ) {
		gtk_notebook_set_page(
			GTK_NOTEBOOK(addrgather_dlg.notebook), PAGE_WARNING );
		gtk_widget_set_sensitive( addrgather_dlg.btnOk, FALSE );
		gtk_widget_grab_default( addrgather_dlg.btnCancel );
	}
	else {
		gtk_widget_grab_focus( addrgather_dlg.entryBook );
	}
	manage_window_set_transient( GTK_WINDOW(addrgather_dlg.window) );
	gtk_main();

	if( ! errFlag ) {
		g_free( addrgather_dlg.folderPath );
		addrgather_dlg.folderPath = NULL;
	}
	gtk_widget_hide( addrgather_dlg.window );
	_harv_addressIndex_ = NULL;

	if( addrgather_dlg.cancelled == TRUE ) return NULL;

	return _harv_addressBook_;
}

/*
* End of Source.
*/


