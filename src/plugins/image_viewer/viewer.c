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
#include <glib/gi18n.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkpixmap.h>

#if HAVE_GDK_PIXBUF
#  include <gdk-pixbuf/gdk-pixbuf.h>
#else
#if HAVE_GDK_IMLIB
#  include <gdk_imlib.h>
#endif
#endif /* HAVE_GDK_PIXBUF */

#include "procmime.h"
#include "utils.h"
#include "mimeview.h"

#include "viewerprefs.h"

typedef struct _ImageViewer ImageViewer;

MimeViewerFactory image_viewer_factory;
static void image_viewer_get_resized_size(gint w, gint h, gint aw, gint ah,
					  gint * sw, gint * sh);
static void image_viewer_clear_viewer(MimeViewer *imageviewer);

struct _ImageViewer
{
	MimeViewer mimeviewer;

	gchar	  *file;
	MimeInfo  *mimeinfo;
	gboolean   resize_img;

	GtkWidget *scrolledwin;
	GtkWidget *image;
	GtkWidget *notebook;
	GtkWidget *filename;
	GtkWidget *filesize;
	GtkWidget *content_type;
};

static GtkWidget *image_viewer_get_widget(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	debug_print("image_viewer_get_widget\n");

	return imageviewer->notebook;
}

#if HAVE_GDK_PIXBUF
static void image_viewer_load_file(ImageViewer *imageviewer, const gchar *imgfile)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_scaled;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	gint avail_width;
	gint avail_height;
	gint new_width;
	gint new_height;
	GError *error = NULL;

	debug_print("image_viewer_show_mimepart\n");

	pixbuf = gdk_pixbuf_new_from_file(imgfile, &error);
	if (error) {
		g_warning(error->message);
		g_error_free(error);
	}
	if (!pixbuf) {
		g_warning("Can't load the image.");	
		return;
	}

	if (imageviewer->resize_img) {
		avail_width = imageviewer->notebook->parent->allocation.width;
		avail_height = imageviewer->notebook->parent->allocation.height;
		if (avail_width > 8) avail_width -= 8;
		if (avail_height > 8) avail_height -= 8;

		image_viewer_get_resized_size(gdk_pixbuf_get_width(pixbuf),
				 gdk_pixbuf_get_height(pixbuf),
				 avail_width, avail_height,
				 &new_width, &new_height);

		pixbuf_scaled = gdk_pixbuf_scale_simple
			(pixbuf, new_width, new_height, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		pixbuf = pixbuf_scaled;
	}

	gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, &mask, 0);

	if (!imageviewer->image) {
		imageviewer->image = gtk_pixmap_new(pixmap, mask);

		gtk_scrolled_window_add_with_viewport
			(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin),
			 imageviewer->image);
	} else
		gtk_pixmap_set(imageviewer->image, pixmap, mask);

	gtk_widget_show(imageviewer->image);

	g_object_unref(pixbuf);
}
#endif /* HAVE_GDK_PIXBUF */

static void image_viewer_set_notebook_page(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	if (!imageviewerprefs.display_img)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(imageviewer->notebook), 0);
	else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(imageviewer->notebook), 1);
}

static void image_viewer_load_image(ImageViewer *imageviewer)
{
	gchar *imgfile;

	if (imageviewer->mimeinfo == NULL)
		return;

	imgfile = procmime_get_tmp_file_name(imageviewer->mimeinfo);
	if (procmime_get_part(imgfile, imageviewer->mimeinfo) < 0) {
		g_warning("Can't get mimepart file");	
		g_free(imgfile);
		return;
	}
	image_viewer_load_file(imageviewer, imgfile);
	unlink(imgfile);
	g_free(imgfile);
}

static void image_viewer_show_mimepart(MimeViewer *_mimeviewer, const gchar *file, MimeInfo *mimeinfo)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	image_viewer_clear_viewer(_mimeviewer);
	g_free(imageviewer->file);
	imageviewer->file = g_strdup(file);
	imageviewer->mimeinfo = mimeinfo;

	if (imageviewerprefs.display_img)
		image_viewer_load_image(imageviewer);
	else {
		gtk_label_set_text(GTK_LABEL(imageviewer->filename),
				   procmime_mimeinfo_get_parameter(mimeinfo, "name"));
		gtk_label_set_text(GTK_LABEL(imageviewer->filesize), to_human_readable(mimeinfo->length));
		gtk_label_set_text(GTK_LABEL(imageviewer->content_type), mimeinfo->subtype);
	}
}

static void image_viewer_clear_viewer(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;
	GtkAdjustment *hadj, *vadj;

	debug_print("image_viewer_clear_viewer\n");

	image_viewer_set_notebook_page(_mimeviewer);

	if (imageviewer->image != NULL)
		gtk_pixmap_set(GTK_PIXMAP(imageviewer->image), NULL, NULL);
	hadj = gtk_scrolled_window_get_hadjustment
		(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin));
	gtk_adjustment_set_value(hadj, 0.0);
	vadj = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin));
	gtk_adjustment_set_value(vadj, 0.0);

	g_free(imageviewer->file);
	imageviewer->file = NULL;
	imageviewer->mimeinfo = NULL;
	imageviewer->resize_img = imageviewerprefs.resize_img;
}

static void image_viewer_destroy_viewer(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	debug_print("image_viewer_destroy_viewer\n");

	image_viewer_clear_viewer(_mimeviewer);
	g_object_unref(imageviewer->notebook);
	g_free(imageviewer);
}

static void image_viewer_get_resized_size(gint w, gint h, gint aw, gint ah,
			     gint *sw, gint *sh)
{
	gfloat wratio = 1.0;
	gfloat hratio = 1.0;
	gfloat ratio  = 1.0;

	if (w > aw)
		wratio = (gfloat)aw / (gfloat)w;
	if (h > ah)
		hratio = (gfloat)ah / (gfloat)h;

	ratio = (wratio > hratio) ? hratio : wratio;

	*sw = (gint)(w * ratio);
	*sh = (gint)(h * ratio);

	/* be paranoid */
	if (*sw <= 0 || *sh <= 0) {
		*sw = w;
		*sh = h;
	}
}

static void load_cb(GtkButton *button, ImageViewer *imageviewer)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(imageviewer->notebook), 1);
	image_viewer_load_image(imageviewer);
}

static gboolean scrolledwin_button_cb(GtkWidget *scrolledwin, GdkEventButton *event,
				      ImageViewer *imageviewer)
{
	if (event->button == 1 && imageviewer->image) {
		imageviewer->resize_img = !imageviewer->resize_img;
		image_viewer_load_image(imageviewer);
		return TRUE;
	}
	return FALSE;
}

static void scrolledwin_resize_cb(GtkWidget *scrolledwin, GtkAllocation *alloc,
				  ImageViewer *imageviewer)
{
	if (imageviewer->resize_img)
		image_viewer_load_image(imageviewer);
}

MimeViewer *image_viewer_create(void)
{
	ImageViewer *imageviewer;
	/*
	 *  glade generated code (do not touch)
	 */
	GtkWidget *notebook;
	GtkWidget *table1;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *filename;
	GtkWidget *filesize;
	GtkWidget *load_button;
	GtkWidget *label5;
	GtkWidget *content_type;
	GtkWidget *scrolledwin;

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	GTK_WIDGET_UNSET_FLAGS(notebook, GTK_CAN_FOCUS);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);

	table1 = gtk_table_new(4, 3, FALSE);
	gtk_widget_show(table1);
	gtk_container_add(GTK_CONTAINER(notebook), table1);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 4);

	label3 = gtk_label_new(_("Filename:"));
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table1), label3, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	label4 = gtk_label_new(_("Filesize:"));
	gtk_widget_show(label4);
	gtk_table_attach(GTK_TABLE(table1), label4, 0, 1, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label4), 0, 0.5);

	filename = gtk_label_new("");
	gtk_widget_show(filename);
	gtk_table_attach(GTK_TABLE(table1), filename, 1, 3, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(filename), 0, 0.5);

	filesize = gtk_label_new("");
	gtk_widget_show(filesize);
	gtk_table_attach(GTK_TABLE(table1), filesize, 1, 3, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(filesize), 0, 0.5);

	load_button = gtk_button_new_with_label(_("Load Image"));
	gtk_widget_show(load_button);
	gtk_table_attach(GTK_TABLE(table1), load_button, 1, 2, 3, 4,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	label5 = gtk_label_new(_("Content-Type:"));
	gtk_widget_show(label5);
	gtk_table_attach(GTK_TABLE(table1), label5, 0, 1, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label5), 0, 0.5);

	content_type = gtk_label_new("");
	gtk_widget_show(content_type);
	gtk_table_attach(GTK_TABLE(table1), content_type, 1, 3, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(content_type), 0, 0.5);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_container_add(GTK_CONTAINER(notebook), scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	/*
	 *  end of glade code
	 */

	debug_print("Creating image view...\n");
	imageviewer = g_new0(ImageViewer, 1);
	imageviewer->mimeviewer.factory = &image_viewer_factory;

	imageviewer->mimeviewer.get_widget = image_viewer_get_widget;
	imageviewer->mimeviewer.show_mimepart = image_viewer_show_mimepart;
	imageviewer->mimeviewer.clear_viewer = image_viewer_clear_viewer;
	imageviewer->mimeviewer.destroy_viewer = image_viewer_destroy_viewer;

	imageviewer->resize_img   = imageviewerprefs.resize_img;

	imageviewer->scrolledwin  = scrolledwin;
	imageviewer->image        = NULL;
	imageviewer->notebook	  = notebook;
	imageviewer->filename	  = filename;
	imageviewer->filesize	  = filesize;
	imageviewer->content_type = content_type;

	gtk_widget_ref(notebook);

	g_signal_connect(G_OBJECT(load_button), "released",
			 G_CALLBACK(load_cb), imageviewer);
	g_signal_connect(G_OBJECT(scrolledwin), "button-press-event",
			 G_CALLBACK(scrolledwin_button_cb), imageviewer);
	g_signal_connect(G_OBJECT(scrolledwin), "size-allocate",
			 G_CALLBACK(scrolledwin_resize_cb), imageviewer);

	image_viewer_set_notebook_page((MimeViewer *)imageviewer);

	return (MimeViewer *) imageviewer;
}

static gchar *content_types[] =
	{"image/*", NULL};

MimeViewerFactory image_viewer_factory =
{
	content_types,
	0,
	
	image_viewer_create,
};

void image_viewer_init(void)
{
#if HAVE_GDK_IMLIB
	gdk_imlib_init();
	gtk_widget_push_visual(gdk_imlib_get_visual());
	gtk_widget_push_colormap(gdk_imlib_get_colormap());
#endif

	mimeview_register_viewer_factory(&image_viewer_factory);
}

void image_viewer_done(void)
{
	mimeview_unregister_viewer_factory(&image_viewer_factory);
}
