/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>

#include "intl.h"
#include "utils.h"
#include "socket.h"

#define BUFFSIZE	8192

static gboolean debug_mode = FALSE;

static void hash_free_strings_func(gpointer key, gpointer value, gpointer data);

void list_free_strings(GList *list)
{
	list = g_list_first(list);

	while (list != NULL) {
		g_free(list->data);
		list = list->next;
	}
}

void slist_free_strings(GSList *list)
{
	while (list != NULL) {
		g_free(list->data);
		list = list->next;
	}
}

GSList *slist_concat_unique (GSList *first, GSList *second)
{
	GSList *tmp, *ret;
	if (first == NULL) {
		if (second == NULL)
			return NULL;
		else 
			return second;
	} else if (second == NULL)
		return first;
	ret = first;
	for (tmp = second; tmp != NULL; tmp = g_slist_next(tmp)) {
		if (g_slist_find(ret, tmp->data) == NULL)
			ret = g_slist_prepend(ret, tmp->data);
	}
	return ret;
}
 
static void hash_free_strings_func(gpointer key, gpointer value, gpointer data)
{
	g_free(key);
}

void hash_free_strings(GHashTable *table)
{
	g_hash_table_foreach(table, hash_free_strings_func, NULL);
}

static void hash_free_value_mem_func(gpointer key, gpointer value,
				     gpointer data)
{
	g_free(value);
}

void hash_free_value_mem(GHashTable *table)
{
	g_hash_table_foreach(table, hash_free_value_mem_func, NULL);
}

void ptr_array_free_strings(GPtrArray *array)
{
	gint i;
	gchar *str;

	g_return_if_fail(array != NULL);

	for (i = 0; i < array->len; i++) {
		str = g_ptr_array_index(array, i);
		g_free(str);
	}
}

gint to_number(const gchar *nstr)
{
	register const gchar *p;

	if (*nstr == '\0') return -1;

	for (p = nstr; *p != '\0'; p++)
		if (!isdigit(*p)) return -1;

	return atoi(nstr);
}

/* convert integer into string,
   nstr must be not lower than 11 characters length */
gchar *itos_buf(gchar *nstr, gint n)
{
	g_snprintf(nstr, 11, "%d", n);
	return nstr;
}

/* convert integer into string */
gchar *itos(gint n)
{
	static gchar nstr[11];

	return itos_buf(nstr, n);
}

gchar *to_human_readable(off_t size)
{
	static gchar str[10];

	if (size < 1024)
		g_snprintf(str, sizeof(str), "%dB", (gint)size);
	else if (size >> 10 < 1024)
		g_snprintf(str, sizeof(str), "%.1fKB", (gfloat)size / (1 << 10));
	else if (size >> 20 < 1024)
		g_snprintf(str, sizeof(str), "%.2fMB", (gfloat)size / (1 << 20));
	else
		g_snprintf(str, sizeof(str), "%.2fGB", (gfloat)size / (1 << 30));

	return str;
}

/* strcmp with NULL-checking */
gint strcmp2(const gchar *s1, const gchar *s2)
{
	if (s1 == NULL || s2 == NULL)
		return -1;
	else
		return strcmp(s1, s2);
}
/* strstr with NULL-checking */
gchar *strstr2(const gchar *s1, const gchar *s2)
{
	if (s1 == NULL || s2 == NULL)
		return NULL;
	else
		return strstr(s1, s2);
}
/* compare paths */
gint path_cmp(const gchar *s1, const gchar *s2)
{
	gint len1, len2;

	if (s1 == NULL || s2 == NULL) return -1;
	if (*s1 == '\0' || *s2 == '\0') return -1;

	len1 = strlen(s1);
	len2 = strlen(s2);

	if (s1[len1 - 1] == G_DIR_SEPARATOR) len1--;
	if (s2[len2 - 1] == G_DIR_SEPARATOR) len2--;

	return strncmp(s1, s2, MAX(len1, len2));
}

/* remove trailing return code */
gchar *strretchomp(gchar *str)
{
	register gchar *s;

	if (!*str) return str;

	for (s = str + strlen(str) - 1;
	     s >= str && (*s == '\n' || *s == '\r');
	     s--)
		*s = '\0';

	return str;
}

/* remove trailing character */
gchar *strtailchomp(gchar *str, gchar tail_char)
{
	register gchar *s;

	if (!*str) return str;
	if (tail_char == '\0') return str;

	for (s = str + strlen(str) - 1; s >= str && *s == tail_char; s--)
		*s = '\0';

	return str;
}

/* remove CR (carriage return) */
gchar *strcrchomp(gchar *str)
{
	register gchar *s;

	if (!*str) return str;

	s = str + strlen(str) - 1;
	if (*s == '\n' && s > str && *(s - 1) == '\r') {
		*(s - 1) = '\n';
		*s = '\0';
	}

	return str;
}

/* Similar to `strstr' but this function ignores the case of both strings.  */
gchar *strcasestr(const gchar *haystack, const gchar *needle)
{
	register size_t haystack_len, needle_len;

	haystack_len = strlen(haystack);
	needle_len   = strlen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!strncasecmp(haystack, needle, needle_len))
			return (gchar *)haystack;
		else {
			haystack++;
			haystack_len--;
		}
	}

	return NULL;
}

/* Copy no more than N characters of SRC to DEST, with NULL terminating.  */
gchar *strncpy2(gchar *dest, const gchar *src, size_t n)
{
	register gchar c;
	gchar *s = dest;

	do {
		if (--n == 0) {
			*dest = '\0';
			return s;
		}
		c = *src++;
		*dest++ = c;
	} while (c != '\0');

	/* don't do zero fill */
	return s;
}

#if !HAVE_ISWALNUM
int iswalnum(wint_t wc)
{
	return isalnum((int)wc);
}
#endif

#if !HAVE_ISWSPACE
int iswspace(wint_t wc)
{
	return isspace((int)wc);
}
#endif

#if !HAVE_TOWLOWER
wint_t towlower(wint_t wc)
{
	if (wc >= L'A' && wc <= L'Z')
		return wc + L'a' - L'A';

	return wc;
}
#endif

#if !HAVE_WCSLEN
size_t wcslen(const wchar_t *s)
{
	size_t len = 0;

	while (*s != L'\0')
		++len, ++s;

	return len;
}
#endif

#if !HAVE_WCSCPY
/* Copy SRC to DEST.  */
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src)
{
	wint_t c;
	wchar_t *s = dest;

	do {
		c = *src++;
		*dest++ = c;
	} while (c != L'\0');

	return s;
}
#endif

#if !HAVE_WCSNCPY
/* Copy no more than N wide-characters of SRC to DEST.  */
wchar_t *wcsncpy (wchar_t *dest, const wchar_t *src, size_t n)
{
	wint_t c;
	wchar_t *s = dest;

	do {
		c = *src++;
		*dest++ = c;
		if (--n == 0)
			return s;
	} while (c != L'\0');

	/* zero fill */
	do
		*dest++ = L'\0';
	while (--n > 0);

	return s;
}
#endif

/* Duplicate S, returning an identical malloc'd string. */
wchar_t *wcsdup(const wchar_t *s)
{
	wchar_t *new_str;

	if (s) {
		new_str = g_new(wchar_t, wcslen(s) + 1);
		wcscpy(new_str, s);
	} else
		new_str = NULL;

	return new_str;
}

/* Duplicate no more than N wide-characters of S,
   returning an identical malloc'd string. */
wchar_t *wcsndup(const wchar_t *s, size_t n)
{
	wchar_t *new_str;

	if (s) {
		new_str = g_new(wchar_t, n + 1);
		wcsncpy(new_str, s, n);
		new_str[n] = (wchar_t)0;
	} else
		new_str = NULL;

	return new_str;
}

wchar_t *strdup_mbstowcs(const gchar *s)
{
	wchar_t *new_str;

	if (s) {
		new_str = g_new(wchar_t, strlen(s) + 1);
		if (mbstowcs(new_str, s, strlen(s) + 1) < 0) {
			g_free(new_str);
			new_str = NULL;
		} else
			new_str = g_realloc(new_str,
					    sizeof(wchar_t) * (wcslen(new_str) + 1));
	} else
		new_str = NULL;

	return new_str;
}

gchar *strdup_wcstombs(const wchar_t *s)
{
	gchar *new_str;
	size_t len;

	if (s) {
		len = wcslen(s) * MB_CUR_MAX + 1;
		new_str = g_new(gchar, len);
		if (wcstombs(new_str, s, len) < 0) {
			g_free(new_str);
			new_str = NULL;
		} else
			new_str = g_realloc(new_str, strlen(new_str) + 1);
	} else
		new_str = NULL;

	return new_str;
}

/* Compare S1 and S2, ignoring case.  */
gint wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	wint_t c1;
	wint_t c2;

	while (n--) {
		c1 = towlower(*s1++);
		c2 = towlower(*s2++);
		if (c1 != c2)
			return c1 - c2;
		else if (c1 == 0 && c2 == 0)
			break;
	}

	return 0;
}

/* Find the first occurrence of NEEDLE in HAYSTACK, ignoring case.  */
wchar_t *wcscasestr(const wchar_t *haystack, const wchar_t *needle)
{
	register size_t haystack_len, needle_len;

	haystack_len = wcslen(haystack);
	needle_len   = wcslen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!wcsncasecmp(haystack, needle, needle_len))
			return (wchar_t *)haystack;
		else {
			haystack++;
			haystack_len--;
		}
	}

	return NULL;
}

/* Examine if next block is non-ASCII string */
gboolean is_next_nonascii(const guchar *s)
{
	const guchar *p;

	/* skip head space */
	for (p = s; *p != '\0' && isspace(*p); p++)
		;
	for (; *p != '\0' && !isspace(*p); p++) {
		if (*p > 127 || *p < 32)
			return TRUE;
	}

	return FALSE;
}

gint get_next_word_len(const gchar *s)
{
	gint len = 0;

	for (; *s != '\0' && !isspace(*s); s++, len++)
		;

	return len;
}

/* compare subjects */
gint subject_compare(const gchar *s1, const gchar *s2)
{
	gchar *str1, *str2;

	if (!s1 || !s2) return -1;
	if (!*s1 || !*s2) return -1;

	Xstrdup_a(str1, s1, return -1);
	Xstrdup_a(str2, s2, return -1);

	trim_subject(str1);
	trim_subject(str2);

	if (!*str1 || !*str2) return -1;

	return strcmp(str1, str2);
}

void trim_subject(gchar *str)
{
	gchar *srcp;

	eliminate_parenthesis(str, '[', ']');
	eliminate_parenthesis(str, '(', ')');
	g_strstrip(str);

	while (!strncasecmp(str, "Re:", 3)) {
		srcp = str + 3;
		while (isspace(*srcp)) srcp++;
		memmove(str, srcp, strlen(srcp) + 1);
	}
}

void eliminate_parenthesis(gchar *str, gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;

	srcp = destp = str;

	while ((destp = strchr(destp, op))) {
		in_brace = 1;
		srcp = destp + 1;
		while (*srcp) {
			if (*srcp == op)
				in_brace++;
			else if (*srcp == cl)
				in_brace--;
			srcp++;
			if (in_brace == 0)
				break;
		}
		while (isspace(*srcp)) srcp++;
		memmove(destp, srcp, strlen(srcp) + 1);
	}
}

void extract_parenthesis(gchar *str, gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;

	srcp = destp = str;

	while ((srcp = strchr(destp, op))) {
		if (destp > str)
			*destp++ = ' ';
		memmove(destp, srcp + 1, strlen(srcp));
		in_brace = 1;
		while(*destp) {
			if (*destp == op)
				in_brace++;
			else if (*destp == cl)
				in_brace--;

			if (in_brace == 0)
				break;

			destp++;
		}
	}
	*destp = '\0';
}

void extract_one_parenthesis_with_skip_quote(gchar *str, gchar quote_chr,
					     gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;
	gboolean in_quote = FALSE;

	srcp = destp = str;

	if ((srcp = strchr_with_skip_quote(destp, quote_chr, op))) {
		memmove(destp, srcp + 1, strlen(srcp));
		in_brace = 1;
		while(*destp) {
			if (*destp == op && !in_quote)
				in_brace++;
			else if (*destp == cl && !in_quote)
				in_brace--;
			else if (*destp == quote_chr)
				in_quote ^= TRUE;

			if (in_brace == 0)
				break;

			destp++;
		}
	}
	*destp = '\0';
}

void extract_parenthesis_with_skip_quote(gchar *str, gchar quote_chr,
					 gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;
	gboolean in_quote = FALSE;

	srcp = destp = str;

	while ((srcp = strchr_with_skip_quote(destp, quote_chr, op))) {
		if (destp > str)
			*destp++ = ' ';
		memmove(destp, srcp + 1, strlen(srcp));
		in_brace = 1;
		while(*destp) {
			if (*destp == op && !in_quote)
				in_brace++;
			else if (*destp == cl && !in_quote)
				in_brace--;
			else if (*destp == quote_chr)
				in_quote ^= TRUE;

			if (in_brace == 0)
				break;

			destp++;
		}
	}
	*destp = '\0';
}

void eliminate_quote(gchar *str, gchar quote_chr)
{
	register gchar *srcp, *destp;

	srcp = destp = str;

	while ((destp = strchr(destp, quote_chr))) {
		if ((srcp = strchr(destp + 1, quote_chr))) {
			srcp++;
			while (isspace(*srcp)) srcp++;
			memmove(destp, srcp, strlen(srcp) + 1);
		} else {
			*destp = '\0';
			break;
		}
	}
}

void extract_quote(gchar *str, gchar quote_chr)
{
	register gchar *p;

	if ((str = strchr(str, quote_chr))) {
		p = str;
		while ((p = strchr(p + 1, quote_chr)) && (p[-1] == '\\')) {
			memmove(p - 1, p, strlen(p) + 1);
			p--;
		}
		if(p) {
			*p = '\0';
			memmove(str, str + 1, p - str);
		}
	}
}

void eliminate_address_comment(gchar *str)
{
	register gchar *srcp, *destp;
	gint in_brace;

	srcp = destp = str;

	while ((destp = strchr(destp, '"'))) {
		if ((srcp = strchr(destp + 1, '"'))) {
			srcp++;
			if (*srcp == '@') {
				destp = srcp + 1;
			} else {
				while (isspace(*srcp)) srcp++;
				memmove(destp, srcp, strlen(srcp) + 1);
			}
		} else {
			*destp = '\0';
			break;
		}
	}

	srcp = destp = str;

	while ((destp = strchr_with_skip_quote(destp, '"', '('))) {
		in_brace = 1;
		srcp = destp + 1;
		while (*srcp) {
			if (*srcp == '(')
				in_brace++;
			else if (*srcp == ')')
				in_brace--;
			srcp++;
			if (in_brace == 0)
				break;
		}
		while (isspace(*srcp)) srcp++;
		memmove(destp, srcp, strlen(srcp) + 1);
	}
}

gchar *strchr_with_skip_quote(const gchar *str, gint quote_chr, gint c)
{
	gboolean in_quote = FALSE;

	while (*str) {
		if (*str == c && !in_quote)
			return (gchar *)str;
		if (*str == quote_chr)
			in_quote ^= TRUE;
		str++;
	}

	return NULL;
}

gchar *strrchr_with_skip_quote(const gchar *str, gint quote_chr, gint c)
{
	gboolean in_quote = FALSE;
	const gchar *p;

	p = str + strlen(str) - 1;
	while (p >= str) {
		if (*p == c && !in_quote)
			return (gchar *)p;
		if (*p == quote_chr)
			in_quote ^= TRUE;
		p--;
	}

	return NULL;
}

void extract_address(gchar *str)
{
	eliminate_address_comment(str);
	if (strchr_with_skip_quote(str, '"', '<'))
		extract_parenthesis_with_skip_quote(str, '"', '<', '>');
	g_strstrip(str);
}

GSList *address_list_append(GSList *addr_list, const gchar *str)
{
	gchar *work;
	gchar *workp;

	if (!str) return addr_list;

	Xstrdup_a(work, str, return addr_list);

	eliminate_address_comment(work);
	workp = work;

	while (workp && *workp) {
		gchar *p, *next;

		if ((p = strchr_with_skip_quote(workp, '"', ','))) {
			*p = '\0';
			next = p + 1;
		} else
			next = NULL;

		if (strchr_with_skip_quote(workp, '"', '<'))
			extract_parenthesis_with_skip_quote
				(workp, '"', '<', '>');

		g_strstrip(workp);
		if (*workp)
			addr_list = g_slist_append(addr_list, g_strdup(workp));

		workp = next;
	}

	return addr_list;
}

GSList *references_list_append(GSList *msgid_list, const gchar *str)
{
	const gchar *strp;

	if (!str) return msgid_list;
	strp = str;

	while (strp && *strp) {
		const gchar *start, *end;
		gchar *msgid;

		if ((start = strchr(strp, '<')) != NULL) {
			end = strchr(start + 1, '>');
			if (!end) break;
		} else
			break;

		msgid = g_strndup(start + 1, end - start - 1);
		g_strstrip(msgid);
		if (*msgid)
			msgid_list = g_slist_append(msgid_list, msgid);
		else
			g_free(msgid);

		strp = end + 1;
	}

	return msgid_list;
}

GSList *newsgroup_list_append(GSList *group_list, const gchar *str)
{
	gchar *work;
	gchar *workp;

	if (!str) return group_list;

	Xstrdup_a(work, str, return group_list);

	workp = work;

	while (workp && *workp) {
		gchar *p, *next;

		if ((p = strchr_with_skip_quote(workp, '"', ','))) {
			*p = '\0';
			next = p + 1;
		} else
			next = NULL;

		g_strstrip(workp);
		if (*workp)
			group_list = g_slist_append(group_list,
						    g_strdup(workp));

		workp = next;
	}

	return group_list;
}

GList *add_history(GList *list, const gchar *str)
{
	GList *old;

	g_return_val_if_fail(str != NULL, list);

	old = g_list_find_custom(list, (gpointer)str, (GCompareFunc)strcmp2);
	if (old) {
		g_free(old->data);
		list = g_list_remove(list, old->data);
	} else if (g_list_length(list) >= MAX_HISTORY_SIZE) {
		GList *last;

		last = g_list_last(list);
		if (last) {
			g_free(last->data);
			g_list_remove(list, last->data);
		}
	}

	list = g_list_prepend(list, g_strdup(str));

	return list;
}

void remove_return(gchar *str)
{
	register gchar *p = str;

	while (*p) {
		if (*p == '\n' || *p == '\r')
			memmove(p, p + 1, strlen(p));
		else
			p++;
	}
}

void remove_space(gchar *str)
{
	register gchar *p = str;
	register gint spc;

	while (*p) {
		spc = 0;
		while (isspace(*(p + spc)))
			spc++;
		if (spc)
			memmove(p, p + spc, strlen(p + spc) + 1);
		else
			p++;
	}
}

void unfold_line(gchar *str)
{
	register gchar *p = str;
	register gint spc;

	while (*p) {
		if (*p == '\n' || *p == '\r') {
			*p++ = ' ';
			spc = 0;
			while (isspace(*(p + spc)))
				spc++;
			if (spc)
				memmove(p, p + spc, strlen(p + spc) + 1);
		} else
			p++;
	}
}

void subst_char(gchar *str, gchar orig, gchar subst)
{
	register gchar *p = str;

	while (*p) {
		if (*p == orig)
			*p = subst;
		p++;
	}
}

void subst_chars(gchar *str, gchar *orig, gchar subst)
{
	register gchar *p = str;

	while (*p) {
		if (strchr(orig, *p) != NULL)
			*p = subst;
		p++;
	}
}

void subst_for_filename(gchar *str)
{
	subst_chars(str, " \t\r\n\"/\\", '_');
}

gboolean is_header_line(const gchar *str)
{
	if (str[0] == ':') return FALSE;

	while (*str != '\0' && *str != ' ') {
		if (*str == ':')
			return TRUE;
		str++;
	}

	return FALSE;
}

gboolean is_ascii_str(const guchar *str)
{
	while (*str != '\0') {
		if (*str != '\t' && *str != ' ' &&
		    *str != '\r' && *str != '\n' &&
		    (*str < 32 || *str >= 127))
			return FALSE;
		str++;
	}

	return TRUE;
}

gint get_quote_level(const gchar *str, const gchar *quote_chars)
{
	const gchar *first_pos;
	const gchar *last_pos;
	const gchar *p = str;
	gint quote_level = -1;

	/* speed up line processing by only searching to the last '>' */
	if ((first_pos = line_has_quote_char(str, quote_chars)) != NULL) {
		/* skip a line if it contains a '<' before the initial '>' */
		if (memchr(str, '<', first_pos - str) != NULL)
			return -1;
		last_pos = line_has_quote_char_last(first_pos, quote_chars);
	} else
		return -1;

	while (p <= last_pos) {
		while (p < last_pos) {
			if (isspace(*p))
				p++;
			else
				break;
		}

		if (strchr(quote_chars, *p))
			quote_level++;
		else if (*p != '-' && !isspace(*p) && p <= last_pos) {
			/* any characters are allowed except '-' and space */
			while (*p != '-' 
			       && !strchr(quote_chars, *p) 
			       && !isspace(*p) 
			       && p < last_pos)
				p++;
			if (strchr(quote_chars, *p))
				quote_level++;
			else
				break;
		}

		p++;
	}

	return quote_level;
}

const gchar * line_has_quote_char(const gchar * str, const gchar *quote_chars) 
{
	gchar * position = NULL;
	gchar * tmp_pos = NULL;
	int i;

	if (quote_chars == NULL)
		return FALSE;
	
	for (i = 0; i < strlen(quote_chars); i++) {
		tmp_pos = strchr (str,	quote_chars[i]);
		if(position == NULL 
		   || (tmp_pos != NULL && position >= tmp_pos) )
			position = tmp_pos;
	}
	return position; 
}

const gchar * line_has_quote_char_last(const gchar * str, const gchar *quote_chars) 
{
	gchar * position = NULL;
	gchar * tmp_pos = NULL;
	int i;

	if (quote_chars == NULL)
		return FALSE;
	
	for (i = 0; i < strlen(quote_chars); i++) {
		tmp_pos = strrchr (str,	quote_chars[i]);
		if(position == NULL 
		   || (tmp_pos != NULL && position <= tmp_pos) )
			position = tmp_pos;
	}
	return position; 
}

gchar *strstr_with_skip_quote(const gchar *haystack, const gchar *needle)
{
	register guint haystack_len, needle_len;
	gboolean in_squote = FALSE, in_dquote = FALSE;

	haystack_len = strlen(haystack);
	needle_len   = strlen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!in_squote && !in_dquote &&
		    !strncmp(haystack, needle, needle_len))
			return (gchar *)haystack;

		/* 'foo"bar"' -> foo"bar"
		   "foo'bar'" -> foo'bar' */
		if (*haystack == '\'') {
			if (in_squote)
				in_squote = FALSE;
			else if (!in_dquote)
				in_squote = TRUE;
		} else if (*haystack == '\"') {
			if (in_dquote)
				in_dquote = FALSE;
			else if (!in_squote)
				in_dquote = TRUE;
		}

		haystack++;
		haystack_len--;
	}

	return NULL;
}

gchar *strchr_parenthesis_close(const gchar *str, gchar op, gchar cl)
{
	const gchar *p;
	gchar quote_chr = '"';
	gint in_brace;
	gboolean in_quote = FALSE;

	p = str;

	if ((p = strchr_with_skip_quote(p, quote_chr, op))) {
		p++;
		in_brace = 1;
		while (*p) {
			if (*p == op && !in_quote)
				in_brace++;
			else if (*p == cl && !in_quote)
				in_brace--;
			else if (*p == quote_chr)
				in_quote ^= TRUE;

			if (in_brace == 0)
				return (gchar *)p;

			p++;
		}
	}

	return NULL;
}

gchar **strsplit_parenthesis(const gchar *str, gchar op, gchar cl,
			     gint max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array;
	const gchar *s_op, *s_cl;
	guint i, n = 1;

	g_return_val_if_fail(str != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s_op = strchr_with_skip_quote(str, '"', op);
	if (!s_op) return NULL;
	str = s_op;
	s_cl = strchr_parenthesis_close(str, op, cl);
	if (s_cl) {
		do {
			guint len;
			gchar *new_string;

			str++;
			len = s_cl - str;
			new_string = g_new(gchar, len + 1);
			strncpy(new_string, str, len);
			new_string[len] = 0;
			string_list = g_slist_prepend(string_list, new_string);
			n++;
			str = s_cl + 1;

			while (*str && isspace(*str)) str++;
			if (*str != op) {
				string_list = g_slist_prepend(string_list,
							      g_strdup(""));
				n++;
				s_op = strchr_with_skip_quote(str, '"', op);
				if (!--max_tokens || !s_op) break;
				str = s_op;
			} else
				s_op = str;
			s_cl = strchr_parenthesis_close(str, op, cl);
		} while (--max_tokens && s_cl);
	}

	str_array = g_new(gchar*, n);

	i = n - 1;

	str_array[i--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[i--] = slist->data;

	g_slist_free(string_list);

	return str_array;
}

gchar **strsplit_with_quote(const gchar *str, const gchar *delim,
			    gint max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s, *new_str;
	guint i, n = 1, len;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(delim != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s = strstr_with_skip_quote(str, delim);
	if (s) {
		guint delimiter_len = strlen(delim);

		do {
			len = s - str;
			new_str = g_strndup(str, len);

			if (new_str[0] == '\'' || new_str[0] == '\"') {
				if (new_str[len - 1] == new_str[0]) {
					new_str[len - 1] = '\0';
					memmove(new_str, new_str + 1, len - 1);
				}
			}
			string_list = g_slist_prepend(string_list, new_str);
			n++;
			str = s + delimiter_len;
			s = strstr_with_skip_quote(str, delim);
		} while (--max_tokens && s);
	}

	if (*str) {
		new_str = g_strdup(str);
		if (new_str[0] == '\'' || new_str[0] == '\"') {
			len = strlen(str);
			if (new_str[len - 1] == new_str[0]) {
				new_str[len - 1] = '\0';
				memmove(new_str, new_str + 1, len - 1);
			}
		}
		string_list = g_slist_prepend(string_list, new_str);
		n++;
	}

	str_array = g_new(gchar*, n);

	i = n - 1;

	str_array[i--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[i--] = slist->data;

	g_slist_free(string_list);

	return str_array;
}

gchar *get_abbrev_newsgroup_name(const gchar *group, gint len)
{
	gchar *abbrev_group;
	gchar *ap;
	const gchar *p = group;
	gint  count = 0;

	abbrev_group = ap = g_malloc(strlen(group) + 1);

	while (*p) {
		while (*p == '.')
			*ap++ = *p++;

		if ((strlen( p) + count) > len && strchr(p, '.')) {
			*ap++ = *p++;
			while (*p != '.') p++;
		} else {
			strcpy( ap, p);
			return abbrev_group;
		}
		count = count + 2;
	}

	*ap = '\0';
	return abbrev_group;
}

gchar *trim_string(const gchar *str, gint len)
{
	const gchar *p = str;
	gint mb_len;
	gchar *new_str;
	gint new_len = 0;

	if (!str) return NULL;
	if (strlen(str) <= len)
		return g_strdup(str);

	while (*p != '\0') {
		mb_len = mblen(p, MB_LEN_MAX);
		if (mb_len == 0)
			break;
		else if (mb_len < 0)
			return g_strdup(str);
		else if (new_len + mb_len > len)
			break;
		else
			new_len += mb_len;
		p += mb_len;
	}

	Xstrndup_a(new_str, str, new_len, return g_strdup(str));
	return g_strconcat(new_str, "...", NULL);
}

GList *uri_list_extract_filenames(const gchar *uri_list)
{
	GList *result = NULL;
	const gchar *p, *q;
	gchar *file;

	p = uri_list;

	while (p) {
		if (*p != '#') {
			while (isspace(*p)) p++;
			if (!strncmp(p, "file:", 5)) {
				p += 5;
				q = p;
				while (*q && *q != '\n' && *q != '\r') q++;

				if (q > p) {
					q--;
					while (q > p && isspace(*q)) q--;
					file = g_malloc(q - p + 2);
					strncpy(file, p, q - p + 1);
					file[q - p + 1] = '\0';
					result = g_list_append(result,file);
				}
			}
		}
		p = strchr(p, '\n');
		if (p) p++;
	}

	return result;
}

#define HEX_TO_INT(val, hex) \
{ \
	gchar c = hex; \
 \
	if ('0' <= c && c <= '9') { \
		val = c - '0'; \
	} else if ('a' <= c && c <= 'f') { \
		val = c - 'a' + 10; \
	} else if ('A' <= c && c <= 'F') { \
		val = c - 'A' + 10; \
	} else { \
		val = 0; \
	} \
}

gint scan_mailto_url(const gchar *mailto, gchar **to, gchar **cc, gchar **bcc,
		     gchar **subject, gchar **body)
{
	gchar *tmp_mailto;
	gchar *p;

	Xstrdup_a(tmp_mailto, mailto, return -1);

	if (!strncmp(tmp_mailto, "mailto:", 7))
		tmp_mailto += 7;

	p = strchr(tmp_mailto, '?');
	if (p) {
		*p = '\0';
		p++;
	}

	if (to && !*to)
		*to = g_strdup(tmp_mailto);

	while (p) {
		gchar *field, *value;

		field = p;

		p = strchr(p, '=');
		if (!p) break;
		*p = '\0';
		p++;

		value = p;

		p = strchr(p, '&');
		if (p) {
			*p = '\0';
			p++;
		}

		if (*value == '\0') continue;

		if (cc && !*cc && !g_strcasecmp(field, "cc")) {
			*cc = g_strdup(value);
		} else if (bcc && !*bcc && !g_strcasecmp(field, "bcc")) {
			*bcc = g_strdup(value);
		} else if (subject && !*subject &&
			   !g_strcasecmp(field, "subject")) {
			*subject = g_malloc(strlen(value) + 1);
			decode_uri(*subject, value);
		} else if (body && !*body && !g_strcasecmp(field, "body")) {
			*body = g_malloc(strlen(value) + 1);
			decode_uri(*body, value);
		}
	}

	return 0;
}

/*
 * We need this wrapper around g_get_home_dir(), so that
 * we can fix some Windoze things here.  Should be done in glibc of course
 * but as long as we are not able to do our own extensions to glibc, we do
 * it here.
 */
gchar *get_home_dir(void)
{
#if HAVE_DOSISH_SYSTEM
    static gchar *home_dir;

    if (!home_dir) {
        home_dir = read_w32_registry_string(NULL,
                                            "Software\\Sylpheed", "HomeDir" );
        if (!home_dir || !*home_dir) {
            if (getenv ("HOMEDRIVE") && getenv("HOMEPATH")) {
                const char *s = g_get_home_dir();
                if (s && *s)
                    home_dir = g_strdup (s);
            }
            if (!home_dir || !*home_dir) 
                home_dir = g_strdup ("c:\\sylpheed");
        }
        debug_print("initialized home_dir to `%s'\n", home_dir);
    }
    return home_dir;
#else /* standard glib */
    return g_get_home_dir();
#endif
}

gchar *get_rc_dir(void)
{
	static gchar *rc_dir = NULL;

	if (!rc_dir)
		rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				     RC_DIR, NULL);

	return rc_dir;
}

gchar *get_news_cache_dir(void)
{
	static gchar *news_cache_dir = NULL;

	if (!news_cache_dir)
		news_cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					     NEWS_CACHE_DIR, NULL);

	return news_cache_dir;
}

gchar *get_imap_cache_dir(void)
{
	static gchar *imap_cache_dir = NULL;

	if (!imap_cache_dir)
		imap_cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					     IMAP_CACHE_DIR, NULL);

	return imap_cache_dir;
}

gchar *get_mbox_cache_dir(void)
{
	static gchar *mbox_cache_dir = NULL;

	if (!mbox_cache_dir)
		mbox_cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					     MBOX_CACHE_DIR, NULL);

	return mbox_cache_dir;
}

gchar *get_mime_tmp_dir(void)
{
	static gchar *mime_tmp_dir = NULL;

	if (!mime_tmp_dir)
		mime_tmp_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					   MIME_TMP_DIR, NULL);

	return mime_tmp_dir;
}

gchar *get_template_dir(void)
{
	static gchar *template_dir = NULL;

	if (!template_dir)
		template_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					   TEMPLATE_DIR, NULL);

	return template_dir;
}

gchar *get_header_cache_dir(void)
{
	static gchar *header_dir = NULL;

	if (!header_dir)
		header_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					 HEADER_CACHE_DIR, NULL);

	return header_dir;
}

gchar *get_tmp_dir(void)
{
	static gchar *tmp_dir = NULL;

	if (!tmp_dir)
		tmp_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				      TMP_DIR, NULL);

	return tmp_dir;
}

gchar *get_tmp_file(void)
{
	gchar *tmp_file;
	static guint32 id = 0;

	tmp_file = g_strdup_printf("%s%ctmpfile.%08x",
				   get_tmp_dir(), G_DIR_SEPARATOR, id++);

	return tmp_file;
}

gchar *get_domain_name(void)
{
	static gchar *domain_name = NULL;

	if (!domain_name) {
		gchar buf[128] = "";
		struct hostent *hp;

		if (gethostname(buf, sizeof(buf)) < 0) {
			perror("gethostname");
			domain_name = "unknown";
		} else {
			buf[sizeof(buf) - 1] = '\0';
			if ((hp = my_gethostbyname(buf)) == NULL) {
				perror("gethostbyname");
				domain_name = g_strdup(buf);
			} else {
				domain_name = g_strdup(hp->h_name);
			}
		}

		debug_print("domain name = %s\n", domain_name);
	}

	return domain_name;
}

off_t get_file_size(const gchar *file)
{
	struct stat s;

	if (stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		return -1;
	}

	return s.st_size;
}

off_t get_file_size_as_crlf(const gchar *file)
{
	FILE *fp;
	off_t size = 0;
	gchar buf[BUFFSIZE];

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		size += strlen(buf) + 2;
	}

	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fgets");
		size = -1;
	}

	fclose(fp);

	return size;
}

off_t get_left_file_size(FILE *fp)
{
	glong pos;
	glong end;
	off_t size;

	if ((pos = ftell(fp)) < 0) {
		perror("ftell");
		return -1;
	}
	if (fseek(fp, 0L, SEEK_END) < 0) {
		perror("fseek");
		return -1;
	}
	if ((end = ftell(fp)) < 0) {
		perror("fseek");
		return -1;
	}
	size = end - pos;
	if (fseek(fp, pos, SEEK_SET) < 0) {
		perror("fseek");
		return -1;
	}

	return size;
}

gboolean file_exist(const gchar *file, gboolean allow_fifo)
{
	struct stat s;

	if (file == NULL)
		return FALSE;

	if (stat(file, &s) < 0) {
		if (ENOENT != errno) FILE_OP_ERROR(file, "stat");
		return FALSE;
	}

	if (S_ISREG(s.st_mode) || (allow_fifo && S_ISFIFO(s.st_mode)))
		return TRUE;

	return FALSE;
}

gboolean is_dir_exist(const gchar *dir)
{
	struct stat s;

	if (dir == NULL)
		return FALSE;

	if (stat(dir, &s) < 0) {
		if (ENOENT != errno) FILE_OP_ERROR(dir, "stat");
		return FALSE;
	}

	if (S_ISDIR(s.st_mode))
		return TRUE;

	return FALSE;
}

gboolean is_file_entry_exist(const gchar *file)
{
	struct stat s;

	if (file == NULL)
		return FALSE;

	if (stat(file, &s) < 0) {
		if (ENOENT != errno) FILE_OP_ERROR(file, "stat");
		return FALSE;
	}

	return TRUE;
}

gint change_dir(const gchar *dir)
{
	gchar *prevdir = NULL;

	if (debug_mode)
		prevdir = g_get_current_dir();

	if (chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		if (debug_mode) g_free(prevdir);
		return -1;
	} else if (debug_mode) {
		gchar *cwd;

		cwd = g_get_current_dir();
		if (strcmp(prevdir, cwd) != 0)
			g_print("current dir: %s\n", cwd);
		g_free(cwd);
		g_free(prevdir);
	}

	return 0;
}

gint make_dir(const gchar *dir)
{
	if (mkdir(dir, S_IRWXU) < 0) {
		FILE_OP_ERROR(dir, "mkdir");
		return -1;
	}
	if (chmod(dir, S_IRWXU) < 0)
		FILE_OP_ERROR(dir, "chmod");

	return 0;
}

gint make_dir_hier(const gchar *dir)
{
	gchar *parent_dir;
	const gchar *p;

	for (p = dir; (p = strchr(p, G_DIR_SEPARATOR)) != NULL; p++) {
		parent_dir = g_strndup(dir, p - dir);
		if (*parent_dir != '\0') {
			if (!is_dir_exist(parent_dir)) {
				if (make_dir(parent_dir) < 0) {
					g_free(parent_dir);
					return -1;
				}
			}
		}
		g_free(parent_dir);
	}

	if (!is_dir_exist(dir)) {
		if (make_dir(dir) < 0)
			return -1;
	}

	return 0;
}

gint remove_all_files(const gchar *dir)
{
	DIR *dp;
	struct dirent *d;
	gchar *prev_dir;

	prev_dir = g_get_current_dir();

	if (chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(dir, "opendir");
		g_free(prev_dir);
		return -1;
	}

	while ((d = readdir(dp)) != NULL) {
		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;

		if (unlink(d->d_name) < 0)
			FILE_OP_ERROR(d->d_name, "unlink");
	}

	closedir(dp);

	if (chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_numbered_files(const gchar *dir, guint first, guint last)
{
	DIR *dp;
	struct dirent *d;
	gchar *prev_dir;
	gint fileno;

	prev_dir = g_get_current_dir();

	if (chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(dir, "opendir");
		g_free(prev_dir);
		return -1;
	}

	while ((d = readdir(dp)) != NULL) {
		fileno = to_number(d->d_name);
		if (fileno >= 0 && first <= fileno && fileno <= last) {
			if (is_dir_exist(d->d_name))
				continue;
			if (unlink(d->d_name) < 0)
				FILE_OP_ERROR(d->d_name, "unlink");
		}
	}

	closedir(dp);

	if (chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_numbered_files_not_in_list(const gchar *dir, GSList *numberlist)
{
	DIR *dp;
	struct dirent *d;
	gchar *prev_dir;
	gint fileno;

	prev_dir = g_get_current_dir();

	if (chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(dir, "opendir");
		g_free(prev_dir);
		return -1;
	}

	while ((d = readdir(dp)) != NULL) {
		fileno = to_number(d->d_name);
		if (fileno >= 0 && (g_slist_find(numberlist, GINT_TO_POINTER(fileno)) == NULL)) {
			debug_print("removing unwanted file %d from %s\n", fileno, dir);
			if (is_dir_exist(d->d_name))
				continue;
			if (unlink(d->d_name) < 0)
				FILE_OP_ERROR(d->d_name, "unlink");
		}
	}

	closedir(dp);

	if (chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_all_numbered_files(const gchar *dir)
{
	return remove_numbered_files(dir, 0, UINT_MAX);
}

gint remove_expired_files(const gchar *dir, guint hours)
{
	DIR *dp;
	struct dirent *d;
	struct stat s;
	gchar *prev_dir;
	gint fileno;
	time_t mtime, now, expire_time;

	prev_dir = g_get_current_dir();

	if (chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(dir, "opendir");
		g_free(prev_dir);
		return -1;
	}

	now = time(NULL);
	expire_time = hours * 60 * 60;

	while ((d = readdir(dp)) != NULL) {
		fileno = to_number(d->d_name);
		if (fileno >= 0) {
			if (stat(d->d_name, &s) < 0) {
				FILE_OP_ERROR(d->d_name, "stat");
				continue;
			}
			if (S_ISDIR(s.st_mode))
				continue;
			mtime = MAX(s.st_mtime, s.st_atime);
			if (now - mtime > expire_time) {
				if (unlink(d->d_name) < 0)
					FILE_OP_ERROR(d->d_name, "unlink");
			}
		}
	}

	closedir(dp);

	if (chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_dir_recursive(const gchar *dir)
{
	struct stat s;
	DIR *dp;
	struct dirent *d;
	gchar *prev_dir;

	/* g_print("dir = %s\n", dir); */

	if (stat(dir, &s) < 0) {
		FILE_OP_ERROR(dir, "stat");
		if (ENOENT == errno) return 0;
		return -1;
	}

	if (!S_ISDIR(s.st_mode)) {
		if (unlink(dir) < 0) {
			FILE_OP_ERROR(dir, "unlink");
			return -1;
		}

		return 0;
	}

	prev_dir = g_get_current_dir();
	/* g_print("prev_dir = %s\n", prev_dir); */

	if (!path_cmp(prev_dir, dir)) {
		g_free(prev_dir);
		if (chdir("..") < 0) {
			FILE_OP_ERROR(dir, "chdir");
			return -1;
		}
		prev_dir = g_get_current_dir();
	}

	if (chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(dir, "opendir");
		chdir(prev_dir);
		g_free(prev_dir);
		return -1;
	}

	/* remove all files in the directory */
	while ((d = readdir(dp)) != NULL) {
		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;

		if (stat(d->d_name, &s) < 0) {
			FILE_OP_ERROR(d->d_name, "stat");
			continue;
		}

		/* g_print("removing %s\n", d->d_name); */

		if (S_ISDIR(s.st_mode)) {
			if (remove_dir_recursive(d->d_name) < 0) {
				g_warning("can't remove directory\n");
				return -1;
			}
		} else {
			if (unlink(d->d_name) < 0)
				FILE_OP_ERROR(d->d_name, "unlink");
		}
	}

	closedir(dp);

	if (chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	if (rmdir(dir) < 0) {
		FILE_OP_ERROR(dir, "rmdir");
		return -1;
	}

	return 0;
}

#if 0
/* this seems to be slower than the stdio version... */
gint copy_file(const gchar *src, const gchar *dest)
{
	gint src_fd, dest_fd;
	gint n_read;
	gint n_write;
	gchar buf[BUFSIZ];
	gchar *dest_bak = NULL;

	if ((src_fd = open(src, O_RDONLY)) < 0) {
		FILE_OP_ERROR(src, "open");
		return -1;
	}

	if (is_file_exist(dest)) {
		dest_bak = g_strconcat(dest, ".bak", NULL);
		if (rename(dest, dest_bak) < 0) {
			FILE_OP_ERROR(dest, "rename");
			close(src_fd);
			g_free(dest_bak);
			return -1;
		}
	}

	if ((dest_fd = open(dest, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) < 0) {
		FILE_OP_ERROR(dest, "open");
		close(src_fd);
		if (dest_bak) {
			if (rename(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	while ((n_read = read(src_fd, buf, sizeof(buf))) > 0) {
		gint len = n_read;
		gchar *bufp = buf;

		while (len > 0) {
			n_write = write(dest_fd, bufp, len);
			if (n_write <= 0) {
				g_warning("writing to %s failed.\n", dest);
				close(dest_fd);
				close(src_fd);
				unlink(dest);
				if (dest_bak) {
					if (rename(dest_bak, dest) < 0)
						FILE_OP_ERROR(dest_bak, "rename");
					g_free(dest_bak);
				}
				return -1;
			}
			len -= n_write;
			bufp += n_write;
		}
	}

	close(src_fd);
	close(dest_fd);

	if (n_read < 0 || get_file_size(src) != get_file_size(dest)) {
		g_warning("File copy from %s to %s failed.\n", src, dest);
		unlink(dest);
		if (dest_bak) {
			if (rename(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}
	g_free(dest_bak);

	return 0;
}
#endif


/*
 * Append src file body to the tail of dest file.
 * Now keep_backup has no effects.
 */
gint append_file(const gchar *src, const gchar *dest, gboolean keep_backup)
{
	FILE *src_fp, *dest_fp;
	gint n_read;
	gchar buf[BUFSIZ];

	gboolean err = FALSE;

	if ((src_fp = fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "fopen");
		return -1;
	}
	
	if ((dest_fp = fopen(dest, "ab")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		fclose(src_fp);
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	while ((n_read = fread(buf, sizeof(gchar), sizeof(buf), src_fp)) > 0) {
		if (n_read < sizeof(buf) && ferror(src_fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning("writing to %s failed.\n", dest);
			fclose(dest_fp);
			fclose(src_fp);
			unlink(dest);
			return -1;
		}
	}

	if (ferror(src_fp)) {
		FILE_OP_ERROR(src, "fread");
		err = TRUE;
	}
	fclose(src_fp);
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		unlink(dest);
		return -1;
	}

	return 0;
}

gint copy_file(const gchar *src, const gchar *dest, gboolean keep_backup)
{
	FILE *src_fp, *dest_fp;
	gint n_read;
	gchar buf[BUFSIZ];
	gchar *dest_bak = NULL;
	gboolean err = FALSE;

	if ((src_fp = fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "fopen");
		return -1;
	}
	if (is_file_exist(dest)) {
		dest_bak = g_strconcat(dest, ".bak", NULL);
		if (rename(dest, dest_bak) < 0) {
			FILE_OP_ERROR(dest, "rename");
			fclose(src_fp);
			g_free(dest_bak);
			return -1;
		}
	}

	if ((dest_fp = fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		fclose(src_fp);
		if (dest_bak) {
			if (rename(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	while ((n_read = fread(buf, sizeof(gchar), sizeof(buf), src_fp)) > 0) {
		if (n_read < sizeof(buf) && ferror(src_fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning("writing to %s failed.\n", dest);
			fclose(dest_fp);
			fclose(src_fp);
			unlink(dest);
			if (dest_bak) {
				if (rename(dest_bak, dest) < 0)
					FILE_OP_ERROR(dest_bak, "rename");
				g_free(dest_bak);
			}
			return -1;
		}
	}

	if (ferror(src_fp)) {
		FILE_OP_ERROR(src, "fread");
		err = TRUE;
	}
	fclose(src_fp);
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		unlink(dest);
		if (dest_bak) {
			if (rename(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	if (keep_backup == FALSE && dest_bak)
		unlink(dest_bak);

	g_free(dest_bak);

	return 0;
}

gint move_file(const gchar *src, const gchar *dest, gboolean overwrite)
{
	if (overwrite == FALSE && is_file_exist(dest)) {
		g_warning("move_file(): file %s already exists.", dest);
		return -1;
	}

	if (rename(src, dest) == 0) return 0;

	if (EXDEV != errno) {
		FILE_OP_ERROR(src, "rename");
		return -1;
	}

	if (copy_file(src, dest, FALSE) < 0) return -1;

	unlink(src);

	return 0;
}

gint copy_file_part(FILE *fp, off_t offset, size_t length, const gchar *dest)
{
	FILE *dest_fp;
	gint n_read;
	gint bytes_left, to_read;
	gchar buf[BUFSIZ];
	gboolean err = FALSE;

	if (fseek(fp, offset, SEEK_SET) < 0) {
		perror("fseek");
		return -1;
	}

	if ((dest_fp = fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	bytes_left = length;
	to_read = MIN(bytes_left, sizeof(buf));

	while ((n_read = fread(buf, sizeof(gchar), to_read, fp)) > 0) {
		if (n_read < to_read && ferror(fp))
			break;
		if (fwrite(buf, n_read, 1, dest_fp) < 1) {
			g_warning("writing to %s failed.\n", dest);
			fclose(dest_fp);
			unlink(dest);
			return -1;
		}
		bytes_left -= n_read;
		if (bytes_left == 0)
			break;
		to_read = MIN(bytes_left, sizeof(buf));
	}

	if (ferror(fp)) {
		perror("fread");
		err = TRUE;
	}
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		unlink(dest);
		return -1;
	}

	return 0;
}

/* convert line endings into CRLF. If the last line doesn't end with
 * linebreak, add it.
 */
gint canonicalize_file(const gchar *src, const gchar *dest)
{
	FILE *src_fp, *dest_fp;
	gchar buf[BUFFSIZE];
	gint len;
	gboolean err = FALSE;
	gboolean last_linebreak = FALSE;

	if ((src_fp = fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "fopen");
		return -1;
	}

	if ((dest_fp = fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "fopen");
		fclose(src_fp);
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode\n");
	}

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		gint r = 0;

		len = strlen(buf);
		if (len == 0) break;
		last_linebreak = FALSE;

		if (buf[len - 1] != '\n') {
			last_linebreak = TRUE;
			r = fputs(buf, dest_fp);
		} else if (len > 1 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			r = fputs(buf, dest_fp);
		} else {
			if (len > 1) {
				r = fwrite(buf, len - 1, 1, dest_fp);
				if (r != 1)
					r = EOF;
			}
			if (r != EOF)
				r = fputs("\r\n", dest_fp);
		}

		if (r == EOF) {
			g_warning("writing to %s failed.\n", dest);
			fclose(dest_fp);
			fclose(src_fp);
			unlink(dest);
			return -1;
		}
	}

	if (last_linebreak == TRUE) {
		if (fputs("\r\n", dest_fp) == EOF)
			err = TRUE;
	}

	if (ferror(src_fp)) {
		FILE_OP_ERROR(src, "fread");
		err = TRUE;
	}
	fclose(src_fp);
	if (fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "fclose");
		err = TRUE;
	}

	if (err) {
		unlink(dest);
		return -1;
	}

	return 0;
}

gint canonicalize_file_replace(const gchar *file)
{
	gchar *tmp_file;

	tmp_file = get_tmp_file();

	if (canonicalize_file(file, tmp_file) < 0) {
		g_free(tmp_file);
		return -1;
	}

	if (move_file(tmp_file, file, TRUE) < 0) {
		g_warning("can't replace %s .\n", file);
		unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_free(tmp_file);
	return 0;
}

gint change_file_mode_rw(FILE *fp, const gchar *file)
{
#if HAVE_FCHMOD
	return fchmod(fileno(fp), S_IRUSR|S_IWUSR);
#else
	return chmod(file, S_IRUSR|S_IWUSR);
#endif
}

FILE *my_tmpfile(void)
{
#if HAVE_MKSTEMP
	const gchar suffix[] = ".XXXXXX";
	const gchar *tmpdir;
	guint tmplen;
	const gchar *progname;
	guint proglen;
	gchar *fname;
	gint fd;
	FILE *fp;

	tmpdir = get_tmp_dir();
	tmplen = strlen(tmpdir);
	progname = g_get_prgname();
	proglen = strlen(progname);
	Xalloca(fname, tmplen + 1 + proglen + sizeof(suffix),
		return tmpfile());

	memcpy(fname, tmpdir, tmplen);
	fname[tmplen] = G_DIR_SEPARATOR;
	memcpy(fname + tmplen + 1, progname, proglen);
	memcpy(fname + tmplen + 1 + proglen, suffix, sizeof(suffix));

	fd = mkstemp(fname);
	if (fd < 0)
		return tmpfile();

	unlink(fname);

	fp = fdopen(fd, "w+b");
	if (!fp)
		close(fd);
	else
		return fp;
#endif /* HAVE_MKSTEMP */

	return tmpfile();
}

FILE *str_open_as_stream(const gchar *str)
{
	FILE *fp;
	size_t len;

	g_return_val_if_fail(str != NULL, NULL);

	fp = my_tmpfile();
	if (!fp) {
		FILE_OP_ERROR("str_open_as_stream", "my_tmpfile");
		return NULL;
	}

	len = strlen(str);
	if (len == 0) return fp;

	if (fwrite(str, len, 1, fp) != 1) {
		FILE_OP_ERROR("str_open_as_stream", "fwrite");
		fclose(fp);
		return NULL;
	}

	rewind(fp);
	return fp;
}

gint str_write_to_file(const gchar *str, const gchar *file)
{
	FILE *fp;
	size_t len;

	g_return_val_if_fail(str != NULL, -1);
	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	len = strlen(str);
	if (len == 0) {
		fclose(fp);
		return 0;
	}

	if (fwrite(str, len, 1, fp) != 1) {
		FILE_OP_ERROR(file, "fwrite");
		fclose(fp);
		unlink(file);
		return -1;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}

	return 0;
}

gchar *file_read_to_str(const gchar *file)
{
	GByteArray *array;
	FILE *fp;
	gchar buf[BUFSIZ];
	gint n_read;
	gchar *str;

	g_return_val_if_fail(file != NULL, NULL);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return NULL;
	}

	array = g_byte_array_new();

	while ((n_read = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (n_read < sizeof(buf) && ferror(fp))
			break;
		g_byte_array_append(array, buf, n_read);
	}

	if (ferror(fp)) {
		FILE_OP_ERROR(file, "fread");
		fclose(fp);
		g_byte_array_free(array, TRUE);
		return NULL;
	}

	fclose(fp);

	buf[0] = '\0';
	g_byte_array_append(array, buf, 1);
	str = (gchar *)array->data;
	g_byte_array_free(array, FALSE);

	return str;
}

gint execute_async(gchar *const argv[])
{
	pid_t pid;

	if ((pid = fork()) < 0) {
		perror("fork");
		return -1;
	}

	if (pid == 0) {			/* child process */
		pid_t gch_pid;

		if ((gch_pid = fork()) < 0) {
			perror("fork");
			_exit(1);
		}

		if (gch_pid == 0) {	/* grandchild process */
			execvp(argv[0], argv);

			perror("execvp");
			_exit(1);
		}

		_exit(0);
	}

	waitpid(pid, NULL, 0);

	return 0;
}

gint execute_sync(gchar *const argv[])
{
	pid_t pid;

	if ((pid = fork()) < 0) {
		perror("fork");
		return -1;
	}

	if (pid == 0) {		/* child process */
		execvp(argv[0], argv);

		perror("execvp");
		_exit(1);
	}

	waitpid(pid, NULL, 0);

	return 0;
}

gint execute_command_line(const gchar *cmdline, gboolean async)
{
	gchar **argv;
	gint ret;

	argv = strsplit_with_quote(cmdline, " ", 0);

	if (async)
		ret = execute_async(argv);
	else
		ret = execute_sync(argv);
	g_strfreev(argv);

	return ret;
}

static gint is_unchanged_uri_char(char c)
{
	switch (c) {
		case '(':
		case ')':
		case ',':
			return 0;
		default:
			return 1;
	}
}

void encode_uri(gchar *encoded_uri, gint bufsize, const gchar *uri)
{
	int i;
	int k;

	k = 0;
	for(i = 0; i < strlen(uri) ; i++) {
		if (is_unchanged_uri_char(uri[i])) {
			if (k + 2 >= bufsize)
				break;
			encoded_uri[k++] = uri[i];
		}
		else {
			char * hexa = "0123456789ABCDEF";
			
			if (k + 4 >= bufsize)
				break;
			encoded_uri[k++] = '%';
			encoded_uri[k++] = hexa[uri[i] / 16];
			encoded_uri[k++] = hexa[uri[i] % 16];
		}
	}
	encoded_uri[k] = 0;
}

/* Converts two-digit hexadecimal to decimal.  Used for unescaping escaped 
 * characters
 */
static gint axtoi(const gchar *hexstr)
{
	gint hi, lo, result;
       
	hi = hexstr[0];
	if ('0' <= hi && hi <= '9') {
		hi -= '0';
	} else
		if ('a' <= hi && hi <= 'f') {
			hi -= ('a' - 10);
		} else
			if ('A' <= hi && hi <= 'F') {
				hi -= ('A' - 10);
			}

	lo = hexstr[1];
	if ('0' <= lo && lo <= '9') {
		lo -= '0';
	} else
		if ('a' <= lo && lo <= 'f') {
			lo -= ('a'-10);
		} else
			if ('A' <= lo && lo <= 'F') {
				lo -= ('A' - 10);
			}
	result = lo + (16 * hi);
	return result;
}


/* Decodes URL-Encoded strings (i.e. strings in which spaces are replaced by
 * plusses, and escape characters are used)
 */

void decode_uri(gchar *decoded_uri, const gchar *encoded_uri)
{
	const gchar *encoded;
	gchar *decoded;

	encoded = encoded_uri;
	decoded = decoded_uri;

	while (*encoded) {
		if (*encoded == '%') {
			encoded++;
			if (isxdigit(encoded[0])
			    && isxdigit(encoded[1])) {
				*decoded = (gchar) axtoi(encoded);
				decoded++;
				encoded += 2;
			}
		}
		else if (*encoded == '+') {
			*decoded = ' ';
			decoded++;
			encoded++;
		}
		else {
			*decoded = *encoded;
			decoded++;
			encoded++;
		}
	}

	*decoded = '\0';
}


gint open_uri(const gchar *uri, const gchar *cmdline)
{
	gchar buf[BUFFSIZE];
	gchar *p;
	gchar encoded_uri[BUFFSIZE];
	
	g_return_val_if_fail(uri != NULL, -1);

	/* an option to choose whether to use encode_uri or not ? */
	encode_uri(encoded_uri, BUFFSIZE, uri);
	
	if (cmdline &&
	    (p = strchr(cmdline, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf), cmdline, encoded_uri);
	else {
		if (cmdline)
			g_warning("Open URI command line is invalid: `%s'",
				  cmdline);
		g_snprintf(buf, sizeof(buf), DEFAULT_BROWSER_CMD, encoded_uri);
	}
	
	execute_command_line(buf, TRUE);

	return 0;
}

time_t remote_tzoffset_sec(const gchar *zone)
{
	static gchar ustzstr[] = "PSTPDTMSTMDTCSTCDTESTEDT";
	gchar zone3[4];
	gchar *p;
	gchar c;
	gint iustz;
	gint offset;
	time_t remoteoffset;

	strncpy(zone3, zone, 3);
	zone3[3] = '\0';
	remoteoffset = 0;

	if (sscanf(zone, "%c%d", &c, &offset) == 2 &&
	    (c == '+' || c == '-')) {
		remoteoffset = ((offset / 100) * 60 + (offset % 100)) * 60;
		if (c == '-')
			remoteoffset = -remoteoffset;
	} else if (!strncmp(zone, "UT" , 2) ||
		   !strncmp(zone, "GMT", 2)) {
		remoteoffset = 0;
	} else if (strlen(zone3) == 3 &&
		   (p = strstr(ustzstr, zone3)) != NULL &&
		   (p - ustzstr) % 3 == 0) {
		iustz = ((gint)(p - ustzstr) / 3 + 1) / 2 - 8;
		remoteoffset = iustz * 3600;
	} else if (strlen(zone3) == 1) {
		switch (zone[0]) {
		case 'Z': remoteoffset =   0; break;
		case 'A': remoteoffset =  -1; break;
		case 'B': remoteoffset =  -2; break;
		case 'C': remoteoffset =  -3; break;
		case 'D': remoteoffset =  -4; break;
		case 'E': remoteoffset =  -5; break;
		case 'F': remoteoffset =  -6; break;
		case 'G': remoteoffset =  -7; break;
		case 'H': remoteoffset =  -8; break;
		case 'I': remoteoffset =  -9; break;
		case 'K': remoteoffset = -10; break; /* J is not used */
		case 'L': remoteoffset = -11; break;
		case 'M': remoteoffset = -12; break;
		case 'N': remoteoffset =   1; break;
		case 'O': remoteoffset =   2; break;
		case 'P': remoteoffset =   3; break;
		case 'Q': remoteoffset =   4; break;
		case 'R': remoteoffset =   5; break;
		case 'S': remoteoffset =   6; break;
		case 'T': remoteoffset =   7; break;
		case 'U': remoteoffset =   8; break;
		case 'V': remoteoffset =   9; break;
		case 'W': remoteoffset =  10; break;
		case 'X': remoteoffset =  11; break;
		case 'Y': remoteoffset =  12; break;
		default:  remoteoffset =   0; break;
		}
		remoteoffset = remoteoffset * 3600;
	}

	return remoteoffset;
}

time_t tzoffset_sec(time_t *now)
{
	struct tm gmt, *lt;
	gint off;

	gmt = *gmtime(now);
	lt = localtime(now);

	off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;

	if (lt->tm_year < gmt.tm_year)
		off -= 24 * 60;
	else if (lt->tm_year > gmt.tm_year)
		off += 24 * 60;
	else if (lt->tm_yday < gmt.tm_yday)
		off -= 24 * 60;
	else if (lt->tm_yday > gmt.tm_yday)
		off += 24 * 60;

	if (off >= 24 * 60)		/* should be impossible */
		off = 23 * 60 + 59;	/* if not, insert silly value */
	if (off <= -24 * 60)
		off = -(23 * 60 + 59);
	if (off > 12 * 60)
		off -= 24 * 60;
	if (off < -12 * 60)
		off += 24 * 60;

	return off * 60;
}

/* calculate timezone offset */
gchar *tzoffset(time_t *now)
{
	static gchar offset_string[6];
	struct tm gmt, *lt;
	gint off;
	gchar sign = '+';

	gmt = *gmtime(now);
	lt = localtime(now);

	off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;

	if (lt->tm_year < gmt.tm_year)
		off -= 24 * 60;
	else if (lt->tm_year > gmt.tm_year)
		off += 24 * 60;
	else if (lt->tm_yday < gmt.tm_yday)
		off -= 24 * 60;
	else if (lt->tm_yday > gmt.tm_yday)
		off += 24 * 60;

	if (off < 0) {
		sign = '-';
		off = -off;
	}

	if (off >= 24 * 60)		/* should be impossible */
		off = 23 * 60 + 59;	/* if not, insert silly value */

	sprintf(offset_string, "%c%02d%02d", sign, off / 60, off % 60);

	return offset_string;
}

void get_rfc822_date(gchar *buf, gint len)
{
	struct tm *lt;
	time_t t;
	gchar day[4], mon[4];
	gint dd, hh, mm, ss, yyyy;

	t = time(NULL);
	lt = localtime(&t);

	sscanf(asctime(lt), "%3s %3s %d %d:%d:%d %d\n",
	       day, mon, &dd, &hh, &mm, &ss, &yyyy);
	g_snprintf(buf, len, "%s, %d %s %d %02d:%02d:%02d %s",
		   day, dd, mon, yyyy, hh, mm, ss, tzoffset(&t));
}

void debug_set_mode(gboolean mode)
{
	debug_mode = mode;
}

gboolean debug_get_mode()
{
	return debug_mode;
}

void debug_print_real(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	if (!debug_mode) return;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	fputs(buf, stdout);
}

void * subject_table_lookup(GHashTable *subject_table, gchar * subject)
{
	if (subject == NULL)
		subject = "";

	if (g_strncasecmp(subject, "Re: ", 4) == 0)
		return g_hash_table_lookup(subject_table, subject + 4);
	else
		return g_hash_table_lookup(subject_table, subject);
}

void subject_table_insert(GHashTable *subject_table, gchar * subject,
			  void * data)
{
	if (subject == NULL)
		return;
	if (* subject == 0)
		return;
	if (g_strcasecmp(subject, "Re:") == 0)
		return;
	if (g_strcasecmp(subject, "Re: ") == 0)
		return;

	if (g_strncasecmp(subject, "Re: ", 4) == 0)
		g_hash_table_insert(subject_table, subject + 4, data);
	else
		g_hash_table_insert(subject_table, subject, data);
}

void subject_table_remove(GHashTable *subject_table, gchar * subject)
{
	if (subject == NULL)
		return;

	if (g_strncasecmp(subject, "Re: ", 4) == 0)
		g_hash_table_remove(subject_table, subject + 4);
	else
		g_hash_table_remove(subject_table, subject);
}

gboolean subject_is_reply(const gchar *subject)
{
	/* XXX: just simply here so someone can handle really
	 * advanced Re: detection like "Re[4]", "ANTW:" or
	 * Re: Re: Re: Re: Re: Re: Re: Re:" stuff. */
	if (subject == NULL) return FALSE;
	else return 0 == g_strncasecmp(subject, "Re: ", 4);
}

FILE *get_tmpfile_in_dir(const gchar *dir, gchar **filename)
{
	int fd;
	
	*filename = g_strdup_printf("%s%csylpheed.XXXXXX", dir, G_DIR_SEPARATOR);
	fd = mkstemp(*filename);

	return fdopen(fd, "w+");
}

/* allow Mutt-like patterns in quick search */
gchar *expand_search_string(const gchar *search_string)
{
	int i = 0;
	gchar term_char, save_char;
	gchar *cmd_start, *cmd_end;
	GString *matcherstr;
	gchar *returnstr = NULL;
	gchar *copy_str;
	gboolean casesens, dontmatch;
	/* list of allowed pattern abbreviations */
	struct {
		gchar		*abbreviated;	/* abbreviation */
		gchar		*command;	/* actual matcher command */ 
		gint		numparams;	/* number of params for cmd */
		gboolean	qualifier;	/* do we append regexpcase */
		gboolean	quotes;		/* do we need quotes */
	}
	cmds[] = {
		{ "a",	"all",				0,	FALSE,	FALSE },
		{ "ag",	"age_greater",			1,	FALSE,	FALSE },
		{ "al",	"age_lower",			1,	FALSE,	FALSE },
		{ "b",	"body_part",			1,	TRUE,	TRUE  },
		{ "B",	"message",			1,	TRUE,	TRUE  },
		{ "c",	"cc",				1,	TRUE,	TRUE  },
		{ "C",	"to_or_cc",			1,	TRUE,	TRUE  },
		{ "D",	"deleted",			0,	FALSE,	FALSE },
		{ "e",	"header \"Sender\"",		1,	TRUE,	TRUE  },
		{ "E",	"execute",			1,	FALSE,	TRUE  },
		{ "f",	"from",				1,	TRUE,	TRUE  },
		{ "F",	"forwarded",			0,	FALSE,	FALSE },
		{ "h",	"headers_part",			1,	TRUE,	TRUE  },
		{ "i",	"header \"Message-Id\"",	1,	TRUE,	TRUE  },
		{ "I",	"inreplyto",			1,	TRUE,	TRUE  },
		{ "L",	"locked",			0,	FALSE,	FALSE },
		{ "n",	"newsgroups",			1,	TRUE,	TRUE  },
		{ "N",	"new",				0,	FALSE,	FALSE },
		{ "O",	"~new",				0,	FALSE,	FALSE },
		{ "r",	"replied",			0,	FALSE,	FALSE },
		{ "R",	"~unread",			0,	FALSE,	FALSE },
		{ "s",	"subject",			1,	TRUE,	TRUE  },
		{ "se",	"score_equal",			1,	FALSE,	FALSE },
		{ "sg",	"score_greater",		1,	FALSE,	FALSE },
		{ "sl",	"score_lower",			1,	FALSE,	FALSE },
		{ "Se",	"size_equal",			1,	FALSE,	FALSE },
		{ "Sg",	"size_greater",			1,	FALSE,	FALSE },
		{ "Ss",	"size_smaller",			1,	FALSE,	FALSE },
		{ "t",	"to",				1,	TRUE,	TRUE  },
		{ "T",	"marked",			0,	FALSE,	FALSE },
		{ "U",	"unread",			0,	FALSE,	FALSE },
		{ "x",	"header \"References\"",	1,	TRUE,	TRUE  },
		{ "y",	"header \"X-Label\"",		1,	TRUE,	TRUE  },
		{ "&",	"&",				0,	FALSE,	FALSE },
		{ "|",	"|",				0,	FALSE,	FALSE },
		{ NULL,	NULL,				0,	FALSE,	FALSE }
	};

	if (search_string == NULL)
		return NULL;

	copy_str = g_strdup(search_string);

	/* if it's a full command don't process it so users
	   can still do something like from regexpcase "foo" */
	for (i = 0; cmds[i].command; i++) {
		const gchar *tmp_search_string = search_string;
		cmd_start = cmds[i].command;
		/* allow logical NOT */
		if (*tmp_search_string == '~')
			tmp_search_string++;
		if (!strncmp(tmp_search_string, cmd_start, strlen(cmd_start)))
			break;
	}
	if (cmds[i].command)
		return copy_str;

	matcherstr = g_string_sized_new(16);
	cmd_start = cmd_end = copy_str;
	while (cmd_end && *cmd_end) {
		/* skip all white spaces */
		while (*cmd_end && isspace(*cmd_end))
			cmd_end++;

		/* extract a command */
		while (*cmd_end && !isspace(*cmd_end))
			cmd_end++;

		/* save character */
		save_char = *cmd_end;
		*cmd_end = '\0';

		dontmatch = FALSE;
		casesens = FALSE;

		/* ~ and ! mean logical NOT */
		if (*cmd_start == '~' || *cmd_start == '!')
		{
			dontmatch = TRUE;
			cmd_start++;
		}
		/* % means case sensitive match */
		if (*cmd_start == '%')
		{
			casesens = TRUE;
			cmd_start++;
		}

		/* find matching abbreviation */
		for (i = 0; cmds[i].command; i++) {
			if (!strcmp(cmd_start, cmds[i].abbreviated)) {
				/* restore character */
				*cmd_end = save_char;

				/* copy command */
				if (matcherstr->len > 0) {
					g_string_append(matcherstr, " ");
				}
				if (dontmatch)
					g_string_append(matcherstr, "~");
				g_string_append(matcherstr, cmds[i].command);
				g_string_append(matcherstr, " ");

				/* stop if no params required */
				if (cmds[i].numparams == 0)
					break;

				/* extract a parameter, allow quotes */
				cmd_end++;
				cmd_start = cmd_end;
				if (*cmd_start == '"') {
					term_char = '"';
					cmd_end++;
				}
				else
					term_char = ' ';

				/* extract actual parameter */
				while ((*cmd_end) && (*cmd_end != term_char))
					cmd_end++;

				if (*cmd_end && (*cmd_end != term_char))
					break;

				if (*cmd_end == '"')
					cmd_end++;

				save_char = *cmd_end;
				*cmd_end = '\0';

				if (cmds[i].qualifier) {
					if (casesens)
						g_string_append(matcherstr, "regexp ");
					else
						g_string_append(matcherstr, "regexpcase ");
				}

				/* do we need to add quotes ? */
				if (cmds[i].quotes && term_char != '"')
					g_string_append(matcherstr, "\"");

				/* copy actual parameter */
				g_string_append(matcherstr, cmd_start);

				/* do we need to add quotes ? */
				if (cmds[i].quotes && term_char != '"')
					g_string_append(matcherstr, "\"");

				/* restore original character */
				*cmd_end = save_char;

				break;
			}
		}

		if (*cmd_end) {
			cmd_end++;
			cmd_start = cmd_end;
		}
	}

	g_free(copy_str);
	returnstr = matcherstr->str;
	g_string_free(matcherstr, FALSE);
	return returnstr;
}

guint g_stricase_hash(gconstpointer gptr)
{
	guint hash_result = 0;
	const char *str;

	for (str = gptr; str && *str; str++) {
		if (isupper(*str)) hash_result += (*str + ' ');
		else hash_result += *str;
	}

	return hash_result;
}

gint g_stricase_equal(gconstpointer gptr1, gconstpointer gptr2)
{
	const char *str1 = gptr1;
	const char *str2 = gptr2;

	return !strcasecmp(str1, str2);
}

