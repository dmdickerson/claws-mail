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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_SSL

#include <openssl/ssl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "../common/intl.h"
#include "../ssl_certificate.h"
#include "../common/utils.h"
#include "../alertpanel.h"

GtkWidget *cert_presenter(SSLCertificate *cert)
{
	GtkWidget *vbox = NULL;
	GtkWidget *hbox = NULL;
	GtkWidget *frame_owner = NULL;
	GtkWidget *frame_signer = NULL;
	GtkWidget *frame_status = NULL;
	GtkTable *owner_table = NULL;
	GtkTable *signer_table = NULL;
	GtkTable *status_table = NULL;
	GtkWidget *label = NULL;
	char *ret, buf[100];
	char *issuer_commonname, *issuer_location, *issuer_organization;
	char *subject_commonname, *subject_location, *subject_organization;
	char *fingerprint, *sig_status;
	unsigned int n;
	unsigned char md[EVP_MAX_MD_SIZE];	
	
	/* issuer */	
	if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_commonName, buf, 100) >= 0)
		issuer_commonname = g_strdup(buf);
	else
		issuer_commonname = g_strdup(_("<not in certificate>"));
	if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_localityName, buf, 100) >= 0) {
		issuer_location = g_strdup(buf);
		if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
			issuer_location = g_strconcat(issuer_location,", ",buf, NULL);
	} else if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
		issuer_location = g_strdup(buf);
	else
		issuer_location = g_strdup(_("<not in certificate>"));

	if (X509_NAME_get_text_by_NID(X509_get_issuer_name(cert->x509_cert), 
				       NID_organizationName, buf, 100) >= 0)
		issuer_organization = g_strdup(buf);
	else 
		issuer_organization = g_strdup(_("<not in certificate>"));
	 
	/* subject */	
	if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_commonName, buf, 100) >= 0)
		subject_commonname = g_strdup(buf);
	else
		subject_commonname = g_strdup(_("<not in certificate>"));
	if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_localityName, buf, 100) >= 0) {
		subject_location = g_strdup(buf);
		if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
			subject_location = g_strconcat(subject_location,", ",buf, NULL);
	} else if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_countryName, buf, 100) >= 0)
		subject_location = g_strdup(buf);
	else
		subject_location = g_strdup(_("<not in certificate>"));

	if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert->x509_cert), 
				       NID_organizationName, buf, 100) >= 0)
		subject_organization = g_strdup(buf);
	else 
		subject_organization = g_strdup(_("<not in certificate>"));
	 
	/* fingerprint */
	X509_digest(cert->x509_cert, EVP_md5(), md, &n);
	fingerprint = readable_fingerprint(md, (int)n);

	/* signature */
	sig_status = ssl_certificate_check_signer(cert->x509_cert);

	if (sig_status==NULL)
		sig_status = g_strdup(_("correct"));

	vbox = gtk_vbox_new(FALSE, 5);
	hbox = gtk_hbox_new(FALSE, 5);
	
	frame_owner  = gtk_frame_new(_("Owner"));
	frame_signer = gtk_frame_new(_("Signer"));
	frame_status = gtk_frame_new(_("Status"));
	
	owner_table = gtk_table_new(3, 2, FALSE);
	signer_table = gtk_table_new(3, 2, FALSE);
	status_table = gtk_table_new(2, 2, FALSE);
	
	label = gtk_label_new(_("Name: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(owner_table, label, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(subject_commonname);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(owner_table, label, 1, 2, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	
	label = gtk_label_new(_("Organization: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(owner_table, label, 0, 1, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(subject_organization);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(owner_table, label, 1, 2, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	
	label = gtk_label_new(_("Location: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(owner_table, label, 0, 1, 2, 3, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(subject_location);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(owner_table, label, 1, 2, 2, 3, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Name: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(signer_table, label, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(issuer_commonname);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(signer_table, label, 1, 2, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	
	label = gtk_label_new(_("Organization: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(signer_table, label, 0, 1, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(issuer_organization);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(signer_table, label, 1, 2, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	
	label = gtk_label_new(_("Location: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(signer_table, label, 0, 1, 2, 3, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(issuer_location);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(signer_table, label, 1, 2, 2, 3, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	label = gtk_label_new(_("Fingerprint: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(status_table, label, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(fingerprint);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(status_table, label, 1, 2, 0, 1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(_("Signature status: "));
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_table_attach(status_table, label, 0, 1, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	label = gtk_label_new(sig_status);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_table_attach(status_table, label, 1, 2, 1, 2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	
	gtk_container_add(GTK_CONTAINER(frame_owner), GTK_WIDGET(owner_table));
	gtk_container_add(GTK_CONTAINER(frame_signer), GTK_WIDGET(signer_table));
	gtk_container_add(GTK_CONTAINER(frame_status), GTK_WIDGET(status_table));
	
	gtk_box_pack_end(GTK_BOX(hbox), frame_signer, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), frame_owner, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), frame_status, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	
	gtk_widget_show_all(vbox);
	
	g_free(issuer_commonname);
	g_free(issuer_location);
	g_free(issuer_organization);
	g_free(subject_commonname);
	g_free(subject_location);
	g_free(subject_organization);
	g_free(fingerprint);
	g_free(sig_status);
	
	return vbox;
}

void sslcertwindow_show_cert(SSLCertificate *cert)
{
	GtkWidget *cert_widget = cert_presenter(cert);
	gchar *buf;
	
	buf = g_strdup_printf(_("SSL certificate for %s"), cert->host);
	alertpanel_with_widget(buf, NULL, _("OK"), NULL, NULL, cert_widget);
	g_free(buf);
}

gboolean sslcertwindow_ask_new_cert(SSLCertificate *cert)
{
	GtkWidget *cert_widget = cert_presenter(cert);
	gchar *buf;
	AlertValue val;
	buf = g_strdup_printf(_("Do you want to accept SSL certificate for %s?"), cert->host);
	val = alertpanel_with_widget(_("Unknown SSL Certificate"), buf, _("Accept and save"), _("Cancel connection"), NULL, cert_widget);
	g_free(buf);
	return (val == G_ALERTDEFAULT);
}

gboolean sslcertwindow_ask_changed_cert(SSLCertificate *old_cert, SSLCertificate *new_cert)
{
	GtkWidget *old_cert_widget = cert_presenter(old_cert);
	GtkWidget *new_cert_widget = cert_presenter(new_cert);
	GtkWidget *vbox;
	GtkWidget *label;
	gchar *buf;
	AlertValue val;
	
	vbox = gtk_vbox_new(FALSE, 5);
	label = gtk_label_new(_("New certificate:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_end(GTK_BOX(vbox), new_cert_widget, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), gtk_hseparator_new(), TRUE, TRUE, 0);
	label = gtk_label_new(_("Known certificate:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_end(GTK_BOX(vbox), old_cert_widget, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	gtk_widget_show_all(vbox);
	
	buf = g_strdup_printf(_("Do you want to accept new SSL certificate for %s?"), new_cert->host);
	val = alertpanel_with_widget(_("Changed SSL Certificate"), buf, _("Accept and save"), _("Cancel connection"), NULL, vbox);
	g_free(buf);
	return (val == G_ALERTDEFAULT);
}
#endif
