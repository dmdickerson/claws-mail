/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#ifndef CLAMAV_PLUGIN_H
#define CLAMAV_PLUGIN_H 1

#include <glib.h>

typedef struct _ClamAvConfig ClamAvConfig;

struct _ClamAvConfig
{
	gboolean	 clamav_enable;
	gboolean 	 clamav_enable_arc;
	guint 		 clamav_max_size;
	gboolean 	 clamav_recv_infected;
	gchar 		*clamav_save_folder;
};

ClamAvConfig *clamav_get_config		(void);
void	      clamav_save_config	(void);

#endif
