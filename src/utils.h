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

#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#if HAVE_ALLOCA_H
#  include <alloca.h>
#endif
#if HAVE_WCHAR_H
#  include <wchar.h>
#endif

/* The AC_CHECK_SIZEOF() in configure fails for some machines.
 * we provide some fallback values here */
#if !SIZEOF_UNSIGNED_SHORT
  #undef SIZEOF_UNSIGNED_SHORT
  #define SIZEOF_UNSIGNED_SHORT 2
#endif
#if !SIZEOF_UNSIGNED_INT
  #undef SIZEOF_UNSIGNED_INT
  #define SIZEOF_UNSIGNED_INT 4
#endif
#if !SIZEOF_UNSIGNED_LONG
  #undef SIZEOF_UNSIGNED_LONG
  #define SIZEOF_UNSIGNED_LONG 4
#endif

#ifndef HAVE_U32_TYPEDEF
  #undef u32	    /* maybe there is a macro with this name */
  typedef guint32 u32;
  #define HAVE_U32_TYPEDEF
#endif

#ifndef BIG_ENDIAN_HOST
  #if (G_BYTE_ORDER == G_BIG_ENDIAN)
    #define BIG_ENDIAN_HOST 1
  #endif
#endif

#define CHDIR_RETURN_IF_FAIL(dir) \
{ \
	if (change_dir(dir) < 0) return; \
}

#define CHDIR_RETURN_VAL_IF_FAIL(dir, val) \
{ \
	if (change_dir(dir) < 0) return val; \
}

#define Xalloca(ptr, size, iffail) \
{ \
	if ((ptr = alloca(size)) == NULL) { \
		g_warning("can't allocate memory\n"); \
		iffail; \
	} \
}

#define Xstrdup_a(ptr, str, iffail) \
{ \
	gchar *__tmp; \
 \
	if ((__tmp = alloca(strlen(str) + 1)) == NULL) { \
		g_warning("can't allocate memory\n"); \
		iffail; \
	} else \
		strcpy(__tmp, str); \
 \
	ptr = __tmp; \
}

#define Xstrndup_a(ptr, str, len, iffail) \
{ \
	gchar *__tmp; \
 \
	if ((__tmp = alloca(len + 1)) == NULL) { \
		g_warning("can't allocate memory\n"); \
		iffail; \
	} else { \
		strncpy(__tmp, str, len); \
		__tmp[len] = '\0'; \
	} \
 \
	ptr = __tmp; \
}

#define FILE_OP_ERROR(file, func) \
{ \
	fprintf(stderr, "%s: ", file); \
	perror(func); \
}

/* for macro expansion */
#define Str(x)	#x
#define Xstr(x)	Str(x)

void list_free_strings		(GList		*list);
void slist_free_strings		(GSList		*list);

void hash_free_strings		(GHashTable	*table);
void hash_free_value_mem	(GHashTable	*table);

void ptr_array_free_strings	(GPtrArray	*array);

/* number-string conversion */
gint to_number			(const gchar *nstr);
gchar *itos_buf			(gchar	     *nstr,
				 gint	      n);
gchar *itos			(gint	      n);
gchar *to_human_readable	(off_t	      size);

/* alternative string functions */
gint strcmp2		(const gchar	*s1,
			 const gchar	*s2);
gint path_cmp		(const gchar	*s1,
			 const gchar	*s2);
gchar *strretchomp	(gchar		*str);
gchar *strtailchomp	(gchar		*str,
			 gchar		 tail_char);
gchar *strcasestr	(const gchar	*haystack,
			 const gchar	*needle);
gchar *strncpy2		(gchar		*dest,
			 const gchar	*src,
			 size_t		 n);

/* wide-character functions */
#if !HAVE_ISWALNUM
int iswalnum		(wint_t wc);
#endif
#if !HAVE_ISWSPACE
int iswspace		(wint_t wc);
#endif
#if !HAVE_TOWLOWER
wint_t towlower		(wint_t wc);
#endif

#if !HAVE_WCSLEN
size_t wcslen		(const wchar_t *s);
#endif
#if !HAVE_WCSCPY
wchar_t *wcscpy		(wchar_t       *dest,
			 const wchar_t *src);
#endif
#if !HAVE_WCSNCPY
wchar_t *wcsncpy	(wchar_t       *dest,
			 const wchar_t *src,
			 size_t		n);
#endif

wchar_t *wcsdup			(const wchar_t *s);
wchar_t *wcsndup		(const wchar_t *s,
				 size_t		n);
wchar_t *strdup_mbstowcs	(const gchar   *s);
gchar *strdup_wcstombs		(const wchar_t *s);
gint wcsncasecmp		(const wchar_t *s1,
				 const wchar_t *s2,
				 size_t		n);
wchar_t *wcscasestr		(const wchar_t *haystack,
				 const wchar_t *needle);

gboolean is_next_nonascii	(const wchar_t *s);
gboolean is_next_mbs		(const wchar_t *s);
wchar_t *find_wspace		(const wchar_t *s);

/* functions for string parsing */
gint subject_compare			(const gchar	*s1,
					 const gchar	*s2);
void trim_subject			(gchar		*str);
void eliminate_parenthesis		(gchar		*str,
					 gchar		 op,
					 gchar		 cl);
void extract_parenthesis		(gchar		*str,
					 gchar		 op,
					 gchar		 cl);

void extract_one_parenthesis_with_skip_quote	(gchar		*str,
						 gchar		 quote_chr,
						 gchar		 op,
						 gchar		 cl);
void extract_parenthesis_with_skip_quote	(gchar		*str,
						 gchar		 quote_chr,
						 gchar		 op,
						 gchar		 cl);

void eliminate_quote			(gchar		*str,
					 gchar		 quote_chr);
void extract_quote			(gchar		*str,
					 gchar		 quote_chr);
void eliminate_address_comment		(gchar		*str);
gchar *strchr_with_skip_quote		(const gchar	*str,
					 gint		 quote_chr,
					 gint		 c);
gchar *strrchr_with_skip_quote		(const gchar	*str,
					 gint		 quote_chr,
					 gint		 c);
void extract_address			(gchar		*str);
GSList *address_list_append		(GSList		*addr_list,
					 const gchar	*str);
GSList *references_list_append		(GSList		*msgid_list,
					 const gchar	*str);
GSList *newsgroup_list_append		(GSList		*group_list,
					 const gchar	*str);
void remove_return			(gchar		*str);
void remove_space			(gchar		*str);
void unfold_line			(gchar		*str);
void subst_char				(gchar		*str,
					 gchar		 orig,
					 gchar		 subst);
gboolean is_header_line			(const gchar	*str);
gboolean is_ascii_str			(const guchar	*str);
gint get_quote_level			(const gchar	*str);
GList *uri_list_extract_filenames	(const gchar	*uri_list);
gchar *strstr_with_skip_quote		(const gchar	*haystack,
					 const gchar	*needle);
gchar **strsplit_with_quote		(const gchar	*str,
					 const gchar	*delim,
					 gint		 max_tokens);

/* return static strings */
gchar *get_home_dir		(void);
gchar *get_rc_dir		(void);
gchar *get_news_cache_dir	(void);
gchar *get_imap_cache_dir	(void);
gchar *get_mbox_cache_dir	(void);
gchar *get_mime_tmp_dir		(void);
gchar *get_tmp_file		(void);
gchar *get_domain_name		(void);

/* file / directory handling */
off_t get_file_size		(const gchar	*file);
off_t get_left_file_size	(FILE		*fp);
gboolean file_exist		(const gchar	*file,
				 gboolean	 allow_fifo);
gboolean is_dir_exist		(const gchar	*dir);
gint change_dir			(const gchar	*dir);
gint make_dir_hier		(const gchar	*dir);
gint remove_all_files		(const gchar	*dir);
gint remove_numbered_files	(const gchar	*dir,
				 guint		 first,
				 guint		 last);
gint remove_all_numbered_files	(const gchar	*dir);
gint remove_dir_recursive	(const gchar	*dir);
gint copy_file			(const gchar	*src,
				 const gchar	*dest);
gint move_file			(const gchar	*src,
				 const gchar	*dest);
gint change_file_mode_rw	(FILE		*fp,
				 const gchar	*file);
FILE *my_tmpfile		(void);

#define is_file_exist(file)		file_exist(file, FALSE)
#define is_file_or_fifo_exist(file)	file_exist(file, TRUE)

/* process execution */
gint execute_async		(gchar *const	 argv[]);
gint execute_command_line	(const gchar	*cmdline);

/* open URI with external browser */
gint open_uri(const gchar *uri, const gchar *cmdline);

/* time functions */
time_t remote_tzoffset_sec	(const gchar	*zone);
time_t tzoffset_sec		(time_t		*now);
gchar *tzoffset			(time_t		*now);
void get_rfc822_date		(gchar		*buf,
				 gint		 len);

/* logging */
void set_log_file	(const gchar *filename);
void close_log_file	(void);
void debug_print	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_print		(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_message	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_warning	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_error		(const gchar *format, ...) G_GNUC_PRINTF(1, 2);

#endif /* __UTILS_H__ */
