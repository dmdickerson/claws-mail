/* passphrase.h - GTK+ based passphrase callback
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

#ifndef GPGMEGTK_PASSPHRASE_H
#define GPGMEGTK_PASSPHRASE_H

#include <glib.h>
#include <gpgme.h>

struct passphrase_cb_info_s {
    gpgme_ctx_t c;
    int did_it;
};

void gpgmegtk_set_passphrase_grab (gint yesno);
gpgme_error_t gpgmegtk_passphrase_cb(void *opaque, const char *uid_hint,
const char *passphrase_info, int prev_bad, int fd);
void gpgmegtk_free_passphrase();

#endif /* GPGMEGTK_PASSPHRASE_H */