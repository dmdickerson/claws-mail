%{

#include "defs.h"

#include <glib.h>
#include <ctype.h>

#include "procmsg.h"
#include "procmime.h"
#include "utils.h"
#include "intl.h"
#include "procheader.h"

#include "quote_fmt.h"
#include "quote_fmt_lex.h"

/* decl */
/*
flex quote_fmt.l
bison -p quote_fmt quote_fmt.y
*/

int yylex(void);

static MsgInfo *msginfo = NULL;
static gboolean *visible = NULL;
static gint maxsize = 0;
static gint stacksize = 0;

static gchar *buffer = NULL;
static gint bufmax = 0;
static gint bufsize = 0;
static const gchar *quote_str = NULL;
static const gchar *body = NULL;
static gint error = 0;

static void add_visibility(gboolean val)
{
	stacksize++;
	if (maxsize < stacksize) {
		maxsize += 128;
		visible = g_realloc(visible, maxsize * sizeof(gboolean));
		if (visible == NULL)
			maxsize = 0;
	}

	visible[stacksize - 1] = val;
}

static void remove_visibility(void)
{
	stacksize--;
}

static void add_buffer(const gchar *s)
{
	gint len;

	len = strlen(s);
	if (bufsize + len + 1 > bufmax) {
		if (bufmax == 0)
			bufmax = 128;
		while (bufsize + len + 1 > bufmax)
			bufmax *= 2;
		buffer = g_realloc(buffer, bufmax);
	}
	strcpy(buffer + bufsize, s);
	bufsize += len;
}

static void flush_buffer(void)
{
	if (buffer != NULL)
		*buffer = '\0';
	bufsize = 0;
}

gchar *quote_fmt_get_buffer(void)
{
	if (error != 0)
		return NULL;
	else
		return buffer;
}

#define INSERT(buf) \
	if (stacksize != 0 && visible[stacksize - 1]) \
		add_buffer(buf)

#define INSERT_CHARACTER(chr) \
	if (stacksize != 0 && visible[stacksize - 1]) { \
		gchar tmp[2]; \
		tmp[0] = (chr); \
		tmp[1] = '\0'; \
		add_buffer(tmp); \
	}

void quote_fmt_init(MsgInfo *info, const gchar *my_quote_str,
		    const gchar *my_body)
{
	quote_str = my_quote_str;
	body = my_body;
	msginfo = info;
	stacksize = 0;
	add_visibility(TRUE);
	if (buffer != NULL)
		*buffer = 0;
	bufsize = 0;
	error = 0;
}

void quote_fmterror(char *str)
{
	g_warning("Error: %s\n", str);
	error = 1;
}

int quote_fmtwrap(void)
{
	return 1;
}

static int isseparator(char ch)
{
	return isspace(ch) || ch == '.' || ch == '-';
}
%}

%union {
	char chr;
	char str[256];
}

%token SHOW_NEWSGROUPS
%token SHOW_DATE SHOW_FROM SHOW_FULLNAME SHOW_FIRST_NAME SHOW_LAST_NAME
%token SHOW_SENDER_INITIAL SHOW_SUBJECT SHOW_TO SHOW_MESSAGEID
%token SHOW_PERCENT SHOW_CC SHOW_REFERENCES SHOW_MESSAGE
%token SHOW_QUOTED_MESSAGE SHOW_BACKSLASH SHOW_TAB
%token SHOW_QUOTED_MESSAGE_NO_SIGNATURE SHOW_MESSAGE_NO_SIGNATURE
%token SHOW_EOL SHOW_QUESTION_MARK SHOW_PIPE SHOW_OPARENT SHOW_CPARENT
%token QUERY_DATE QUERY_FROM
%token QUERY_FULLNAME QUERY_SUBJECT QUERY_TO QUERY_NEWSGROUPS
%token QUERY_MESSAGEID QUERY_CC QUERY_REFERENCES
%token INSERT_FILE INSERT_PROGRAMOUTPUT
%token OPARENT CPARENT
%token CHARACTER
%token SHOW_DATE_EXPR

%start quote_fmt

%token <chr> CHARACTER
%type <chr> character
%type <str> string

%%

quote_fmt:
	character_or_special_or_insert_or_query_list;

character_or_special_or_insert_or_query_list:
	character_or_special_or_insert_or_query character_or_special_or_insert_or_query_list
	| character_or_special_or_insert_or_query ;

character_or_special_or_insert_or_query:
	special
	| character
	{
		INSERT_CHARACTER($1);
	}
	| query
	| insert ;

character:
	CHARACTER
	;

string:
	CHARACTER
	{
		$$[0] = $1;
		$$[1] = '\0';
	}
	| string CHARACTER
	{
		strcpy($$, $1);
		$$[strlen($$) + 1] = '\0';
		$$[strlen($$)] = $2;
	};

special:
	SHOW_NEWSGROUPS
	{
		if (msginfo->newsgroups)
			INSERT(msginfo->newsgroups);
	}
	| SHOW_DATE_EXPR OPARENT string CPARENT
	{
		/* 
		 * ALF - GNU C's strftime() has a nice format specifier 
		 * for time zone offset (%z). Non-standard however, so 
		 * emulate it.
		 */
		if (msginfo->date) {
			char  result[100];
			char *rptr;
			char  zone[6];
			struct tm lt;
			const char *fptr;
			const char *zptr;

#define RLEFT (sizeof result) - (rptr - result)	
#define STR_SIZE(x) (sizeof (x) - 1)

			zone[0] = 0;

			if (procheader_date_parse_to_tm(msginfo->date, &lt, zone)) {
				/*
				 * break up format string in tiny bits delimited by valid %z's and 
				 * feed it to strftime(). don't forget that '%%z' mean literal '%z'.
				 */
				for (rptr = result, fptr = $3; fptr && *fptr && rptr < &result[sizeof result - 1];) {
					int	    perc;
					const char *p;
					char	   *tmp;
					
					if (NULL != (zptr = strstr(fptr, "%z"))) {
						/*
						 * count nr. of prepended percent chars
						 */
						for (perc = 0, p = zptr; p && p >= $3 && *p == '%'; p--, perc++)
							;
						/*
						 * feed to strftime()
						 */
						tmp = g_strndup(fptr, zptr - fptr + (perc % 2 ? 0 : STR_SIZE("%z")));
						if (tmp) {
							rptr += strftime(rptr, RLEFT, tmp, &lt);
							g_free(tmp);
						}
						/*
						 * append time zone offset
						 */
						if (zone[0] && perc % 2) 
							rptr += g_snprintf(rptr, RLEFT, "%s", zone);
						fptr = zptr + STR_SIZE("%z");
					} else {
						rptr += strftime(rptr, RLEFT, fptr, &lt);
						fptr  = NULL;
					}
				}
				
				INSERT(result);
			}
#undef STR_SIZE			
#undef RLEFT			
		}
	}
	| SHOW_DATE
	{
		if (msginfo->date)
			INSERT(msginfo->date);
	}
	| SHOW_FROM
	{
		if (msginfo->from)
			INSERT(msginfo->from);
	}
	| SHOW_FULLNAME
	{
		if (msginfo->fromname)
			INSERT(msginfo->fromname);
	}
	| SHOW_FIRST_NAME
	{
		if (msginfo->fromname) {
			gchar *p;
			gchar *str;

			str = alloca(strlen(msginfo->fromname) + 1);
			if (str != NULL) {
				strcpy(str, msginfo->fromname);
				p = str;
				while (*p && !isspace(*p)) p++;
				*p = '\0';
				INSERT(str);
			}
		}
	}
	| SHOW_LAST_NAME
        {
                /* This probably won't work together very well with Middle
                 names and the like - thth */
		if (msginfo->fromname) {
			gchar *p;
			gchar *str;

			str = alloca(strlen(msginfo->fromname) + 1);
			if (str != NULL) {
				strcpy(str, msginfo->fromname);
                                p = str;
                                while (*p && !isspace(*p)) p++;
                                if (*p) {
				    /* We found a space. Get first none-space char and
				     insert rest of string from there. */
				    while (*p && isspace(*p)) p++;
                                    if (*p) {
	                    		INSERT(p);
				    } else {
    					/* If there is no none-space char, just insert
					 whole fromname. */
					INSERT(str);
				    }
				} else {
				    /* If there is no space, just insert whole fromname. */
				    INSERT(str);
				}
			}
		}
	}
	| SHOW_SENDER_INITIAL
	{
#define MAX_SENDER_INITIAL 20
		if (msginfo->fromname) {
			gchar tmp[MAX_SENDER_INITIAL];
			gchar *p;
			gchar *cur;
			gint len = 0;

			p = msginfo->fromname;
			cur = tmp;
			while (*p) {
				if (*p && isalnum(*p)) {
					*cur = toupper(*p);
						cur++;
					len++;
					if (len >= MAX_SENDER_INITIAL - 1)
						break;
				} else
					break;
				while (*p && !isseparator(*p)) p++;
				while (*p && isseparator(*p)) p++;
			}
			*cur = '\0';
			INSERT(tmp);
		}
	}
	| SHOW_SUBJECT
	{
		if (msginfo->subject)
			INSERT(msginfo->subject);
	}
	| SHOW_TO
	{
		if (msginfo->to)
			INSERT(msginfo->to);
	}
	| SHOW_MESSAGEID
	{
		if (msginfo->msgid)
			INSERT(msginfo->msgid);
	}
	| SHOW_PERCENT
	{
		INSERT("%");
	}
	| SHOW_CC
	{
		if (msginfo->cc)
			INSERT(msginfo->cc);
	}
	| SHOW_REFERENCES
	{
		/* if (msginfo->references)
			INSERT(msginfo->references); */
	}
	| SHOW_MESSAGE
	{
		if (msginfo->folder || body) {
			gchar buf[BUFFSIZE];
			FILE *fp;

			if (body)
				fp = str_open_as_stream(body);
			else
				fp = procmime_get_first_text_content(msginfo);

			if (fp == NULL)
				g_warning("Can't get text part\n");
			else {
				while (fgets(buf, sizeof(buf), fp) != NULL) {
					strcrchomp(buf);
					INSERT(buf);
				}
				fclose(fp);
			}
		}
	}
	| SHOW_QUOTED_MESSAGE
	{
		if (msginfo->folder || body) {
			gchar buf[BUFFSIZE];
			FILE *fp;

			if (body)
				fp = str_open_as_stream(body);
			else
				fp = procmime_get_first_text_content(msginfo);

			if (fp == NULL)
				g_warning("Can't get text part\n");
			else {
				while (fgets(buf, sizeof(buf), fp) != NULL) {
					strcrchomp(buf);
					if (quote_str)
						INSERT(quote_str);
					INSERT(buf);
				}
				fclose(fp);
			}
		}
	}
	| SHOW_MESSAGE_NO_SIGNATURE
	{
		if (msginfo->folder || body) {
			gchar buf[BUFFSIZE];
			FILE *fp;

			if (body)
				fp = str_open_as_stream(body);
			else
				fp = procmime_get_first_text_content(msginfo);

			if (fp == NULL)
				g_warning("Can't get text part\n");
			else {
				while (fgets(buf, sizeof(buf), fp) != NULL) {
					strcrchomp(buf);
					if (strncmp(buf, "-- ", 3) == 0)
						break;
					INSERT(buf);
				}
				fclose(fp);
			}
		}
	}
	| SHOW_QUOTED_MESSAGE_NO_SIGNATURE
	{
		if (msginfo->folder || body) {
			gchar buf[BUFFSIZE];
			FILE *fp;

			if (body)
				fp = str_open_as_stream(body);
			else
				fp = procmime_get_first_text_content(msginfo);

			if (fp == NULL)
				g_warning("Can't get text part\n");
			else {
				while (fgets(buf, sizeof(buf), fp) != NULL) {
					strcrchomp(buf);
					if (strncmp(buf, "-- ", 3) == 0)
						break;
					if (quote_str)
						INSERT(quote_str);
					INSERT(buf);
				}
				fclose(fp);
			}
		}
	}
	| SHOW_BACKSLASH
	{
		INSERT("\\");
	}
	| SHOW_TAB
	{
		INSERT("\t");
	}
	| SHOW_EOL
	{
		INSERT("\n");
	}
	| SHOW_QUESTION_MARK
	{
		INSERT("?");
	}
	| SHOW_PIPE
	{
		INSERT("|");
	}
	| SHOW_OPARENT
	{
		INSERT("{");
	}
	| SHOW_CPARENT
	{
		INSERT("}");
	};

query:
	QUERY_DATE
	{
		add_visibility(msginfo->date != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_FROM
	{
		add_visibility(msginfo->from != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_FULLNAME
	{
		add_visibility(msginfo->fromname != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_SUBJECT
	{
		add_visibility(msginfo->subject != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_TO
	{
		add_visibility(msginfo->to != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_NEWSGROUPS
	{
		add_visibility(msginfo->newsgroups != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_MESSAGEID
	{
		add_visibility(msginfo->msgid != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_CC
	{
		add_visibility(msginfo->cc != NULL);
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	}
	| QUERY_REFERENCES
	{
		/* add_visibility(msginfo->references != NULL); */
	}
	OPARENT quote_fmt CPARENT
	{
		remove_visibility();
	};

insert:
	INSERT_FILE OPARENT string CPARENT
	{
		{
			FILE *file;
			char buffer[256];
			
			if(file = fopen($3, "rb")) {
				while(fgets(buffer, sizeof(buffer), file)) {
					INSERT(buffer);
				}
				fclose(file);
			}
		}
	}
	| INSERT_PROGRAMOUTPUT OPARENT string CPARENT
	{
		{
			FILE *file;
			char buffer[256];
#ifdef WIN32
			gint retval;
			gchar *tmp = get_tmp_file();
			gchar *cmd = g_strdup_printf("%s > %s",$3,tmp);
			
			retval = system(cmd);
			if(!retval && (file = fopen(tmp, "r")))
#else
			if(file = popen($3, "r"))
#endif			/* bison ignores #ifdef'd "{" - cant follow k&r style */
			{
				while(fgets(buffer, sizeof(buffer), file)) {
					INSERT(buffer);
				}
				fclose(file);
			}
#ifdef WIN32
			unlink(tmp);
			g_free(tmp);
			g_free(cmd);
#endif
		}
	};
