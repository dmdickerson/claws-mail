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

#include <glib.h>
#ifdef WIN32
# include <w32lib.h>
# include <sys/stat.h>
#else
# include <sys/time.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <unistd.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#include "socket.h"
#include "utils.h"
#if USE_SSL
#  include "ssl.h"
#endif

#if USE_GIO
#error USE_GIO is currently not supported
#endif

#ifdef WIN32 /* from Winsock2.h */
# define SD_RECEIVE      0x00
# define SD_SEND         0x01
# define SD_BOTH         0x02
#endif

#define BUFFSIZE	8192
#define IO_TIMEOUT	60

static gint sock_connect_with_timeout	(gint			 sock,
					 const struct sockaddr	*serv_addr,
					 gint			 addrlen,
					 guint			 timeout_secs);

#ifndef INET6
static gint sock_connect_by_hostname	(gint		 sock,
					 const gchar	*hostname,
					 gushort	 port);
#else
static gint sock_connect_by_getaddrinfo	(const gchar	*hostname,
					 gushort	 port);
#endif

static SockInfo *sockinfo_from_fd(const gchar *hostname,
				  gushort port,
				  gint sock);

gint fd_connect_unix(const gchar *path)
{
#ifdef WIN32
	perror("unixsocket not on win32");
	return -1;
#else
	gint sock;
	struct sockaddr_un addr;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("sock_connect_unix(): socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	return sock;
#endif
}

gint fd_open_unix(const gchar *path)
{
#ifdef WIN32
	return -1;
#else
	gint sock;
	struct sockaddr_un addr;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	if (sock < 0) {
		perror("sock_open_unix(): socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0) {
		perror("listen");
		close(sock);
		return -1;		
	}

	return sock;
#endif
}

#ifdef WIN32
gint fd_open_lock_service(const gushort port)
{
	gint sock;
	struct sockaddr_in sin;

	sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock < 0) {
		perror("sock_open_lock_service(): socket");
		return -1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port   = htons(port);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("bind");
		close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0) {
		perror("listen");
		close(sock);
		return -1;
	}

	return sock;
}
#endif

gint fd_accept(gint sock)
{
	struct sockaddr_in caddr;
	gint caddr_len;

	caddr_len = sizeof(caddr);
	return accept(sock, (struct sockaddr *)&caddr, &caddr_len);
}


static gint set_nonblocking_mode(gint fd, gboolean nonblock)
{
#ifdef WIN32
	return -1;
#else
	gint flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl");
		return -1;
	}

	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	return fcntl(fd, F_SETFL, flags);
#endif
}

gint sock_set_nonblocking_mode(SockInfo *sock, gboolean nonblock)
{
	g_return_val_if_fail(sock != NULL, -1);

	return set_nonblocking_mode(sock->sock, nonblock);
}


static gboolean is_nonblocking_mode(gint fd)
{
#ifdef WIN32
	return 0;
#else
	gint flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl");
		return FALSE;
	}

	return ((flags & O_NONBLOCK) != 0);
#endif
}

gboolean sock_is_nonblocking_mode(SockInfo *sock)
{
	g_return_val_if_fail(sock != NULL, FALSE);

	return is_nonblocking_mode(sock->sock);
}

static gint fd_check_io(gint fd, GIOCondition cond)
{
	struct timeval timeout;
	fd_set fds;

	timeout.tv_sec  = IO_TIMEOUT;
	timeout.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (cond == G_IO_IN) {
		select(fd + 1, &fds, NULL, NULL, &timeout);
	} else {
		select(fd + 1, NULL, &fds, NULL, &timeout);
	}

	if (FD_ISSET(fd, &fds)) {
		return 0;
	} else {
		g_warning("Socket IO timeout\n");
		return -1;
	}
}

#ifndef WIN32
static sigjmp_buf jmpenv;
#endif

static void timeout_handler(gint sig)
{
#ifndef WIN32
	siglongjmp(jmpenv, 1);
#endif
}

static gint sock_connect_with_timeout(gint sock,
				      const struct sockaddr *serv_addr,
				      gint addrlen,
				      guint timeout_secs)
{
	gint ret;
	void (*prev_handler)(gint);

#ifndef WIN32
	alarm(0);
	prev_handler = signal(SIGALRM, timeout_handler);
	if (sigsetjmp(jmpenv, 1)) {
		alarm(0);
		signal(SIGALRM, prev_handler);
		errno = ETIMEDOUT;
		return -1;
	}
	alarm(timeout_secs);
#endif
	
	ret = connect(sock, serv_addr, addrlen);
	
#ifndef WIN32
	alarm(0);
	signal(SIGALRM, prev_handler);
#endif
	
	return ret;
}

struct hostent *my_gethostbyname(const gchar *hostname)
{
	struct hostent *hp;
	void (*prev_handler)(gint);
	guint timeout_secs = IO_TIMEOUT;

#ifndef WIN32
	alarm(0);
	prev_handler = signal(SIGALRM, timeout_handler);
	if (sigsetjmp(jmpenv, 1)) {
		alarm(0);
		signal(SIGALRM, prev_handler);
		fprintf(stderr, "%s: host lookup timed out.\n", hostname);
		errno = 0;
		return NULL;
	}
	alarm(timeout_secs);
#endif

	if ((hp = gethostbyname(hostname)) == NULL) {
#ifndef WIN32
		alarm(0);
		signal(SIGALRM, prev_handler);
#endif
		fprintf(stderr, "%s: unknown host.\n", hostname);
		errno = 0;
		return NULL;
	}

#ifndef WIN32
	alarm(0);
	signal(SIGALRM, prev_handler);
#endif

	return hp;
}

#ifndef INET6
static gint my_inet_aton(const gchar *hostname, struct in_addr *inp)
{
#if HAVE_INET_ATON
	return inet_aton(hostname, inp);
#else
#if HAVE_INET_ADDR
	guint32 inaddr;

	inaddr = inet_addr(hostname);
	if (inaddr != -1) {
		memcpy(inp, &inaddr, sizeof(inaddr));
		return 1;
	} else
		return 0;
#else
	return 0;
#endif
#endif /* HAVE_INET_ATON */
}

static gint sock_connect_by_hostname(gint sock, const gchar *hostname,
				     gushort port)
{
	struct hostent *hp;
	struct sockaddr_in ad;
	guint timeout_secs = IO_TIMEOUT;

	memset(&ad, 0, sizeof(ad));
	ad.sin_family = AF_INET;
	ad.sin_port = htons(port);

	if (!my_inet_aton(hostname, &ad.sin_addr)) {
		if ((hp = my_gethostbyname(hostname)) == NULL) {
			fprintf(stderr, "%s: unknown host.\n", hostname);
			errno = 0;
			return -1;
		}

		if (hp->h_length != 4 && hp->h_length != 8) {
			fprintf(stderr, "illegal address length received for host %s\n", hostname);
			errno = 0;
			return -1;
		}

		memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
	}

	return sock_connect_with_timeout(sock, (struct sockaddr *)&ad,
					 sizeof(ad), timeout_secs);
}

#else /* INET6 */
static gint sock_connect_by_getaddrinfo(const gchar *hostname, gushort	port)
{
	gint sock = -1, gai_error;
	struct addrinfo hints, *res, *ai;
	guint timeout_secs = IO_TIMEOUT;
	gchar port_str[6];

	memset(&hints, 0, sizeof(hints));
	/* hints.ai_flags = AI_CANONNAME; */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* convert port from integer to string. */
	g_snprintf(port_str, sizeof(port_str), "%d", port);

	if ((gai_error = getaddrinfo(hostname, port_str, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo for %s:%s failed: %s\n",
			hostname, port_str, gai_strerror(gai_error));
		return -1;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0)
			continue;

		if (sock_connect_with_timeout
			(sock, ai->ai_addr, ai->ai_addrlen, timeout_secs) == 0)
			break;

		close(sock);
	}

	if (res != NULL)
		freeaddrinfo(res);

	if (ai == NULL)
		return -1;

	return sock;
}
#endif /* !INET6 */


/* Open a connection using an external program.  May be useful when
 * you need to tunnel through a SOCKS or other firewall, or to
 * establish an IMAP-over-SSH connection. */
/* TODO: Recreate this for sock_connect_thread() */
SockInfo *sock_connect_cmd(const gchar *hostname, const gchar *tunnelcmd)
{
	gint fd[2];
	int r;
#ifdef WIN32
	perror("socketpair not under WIN32");
	return NULL;
#else
	if ((r = socketpair(AF_UNIX, SOCK_STREAM, 0, fd)) == -1) {
		perror("socketpair");
		return NULL;
	}
	log_message("launching tunnel command \"%s\"\n", tunnelcmd);
	if (fork() == 0) {
		close(fd[0]);
		close(0);
		close(1);
		dup(fd[1]);	/* set onto stdin */
		dup(fd[1]);
		execlp("/bin/sh", "/bin/sh", "-c", tunnelcmd, NULL);
	}

	close(fd[1]);
	return sockinfo_from_fd(hostname, 0, fd[0]);
#endif
}


SockInfo *sock_connect(const gchar *hostname, gushort port)
{
	gint sock;

#ifdef INET6
	if ((sock = sock_connect_by_getaddrinfo(hostname, port)) < 0)
		return NULL;
#else
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return NULL;
	}

	if (sock_connect_by_hostname(sock, hostname, port) < 0) {
		if (errno != 0) perror("connect");
		close(sock);
		return NULL;
	}
#endif /* INET6 */

	return sockinfo_from_fd(hostname, port, sock);
}


static SockInfo *sockinfo_from_fd(const gchar *hostname,
				  gushort port,
				  gint sock)
{
	SockInfo *sockinfo;
	
	sockinfo = g_new0(SockInfo, 1);
	sockinfo->sock = sock;
	sockinfo->hostname = g_strdup(hostname);
	sockinfo->port = port;
	sockinfo->state = CONN_ESTABLISHED;

	usleep(100000);

	return sockinfo;
}

gint sock_printf(SockInfo *sock, const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	return sock_write(sock, buf, strlen(buf));
}

gint sock_read(SockInfo *sock, gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

#if USE_SSL
	if (sock->ssl)
		return ssl_read(sock->ssl, buf, len);
#endif
	return fd_read(sock->sock, buf, len);
}

gint fd_read(gint fd, gchar *buf, gint len)
{
	if (fd_check_io(fd, G_IO_IN) < 0)
		return -1;

#ifdef WIN32
	return recv(fd, buf, len, 0);
#else
	return read(fd, buf, len);
#endif
}

#if USE_SSL
gint ssl_read(SSL *ssl, gchar *buf, gint len)
{
	return SSL_read(ssl, buf, len);
}
#endif

gint sock_write(SockInfo *sock, const gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

#if USE_SSL
	if (sock->ssl)
		return ssl_write(sock->ssl, buf, len);
#endif
	return fd_write(sock->sock, buf, len);
}

gint fd_write(gint fd, const gchar *buf, gint len)
{
	gint n, wrlen = 0;

	while (len) {
		if (fd_check_io(fd, G_IO_OUT) < 0)
			return -1;
		signal(SIGPIPE, SIG_IGN);
#ifdef WIN32
		n = send(fd, buf, len, 0);
#else
		n = write(fd, buf, len);
#endif
		if (n <= 0) {
			log_error("write on fd%d: %s\n", fd, strerror(errno));
			return -1;
		}
		len -= n;
		wrlen += n;
		buf += n;
	}

	return wrlen;
}

#if USE_SSL
gint ssl_write(SSL *ssl, const gchar *buf, gint len)
{
	gint n, wrlen = 0;

	while (len) {
		n = SSL_write(ssl, buf, len);
		if (n <= 0)
			return -1;
		len -= n;
		wrlen += n;
		buf += n;
	}

	return wrlen;
}
#endif

gint fd_recv(gint fd, gchar *buf, gint len, gint flags)
{
	if (fd_check_io(fd, G_IO_IN) < 0)
		return -1;

	return recv(fd, buf, len, flags);
}

gint fd_gets(gint fd, gchar *buf, gint len)
{
	gchar *newline, *bp = buf;
	gint n;

	if (--len < 1)
		return -1;
#ifdef WIN32
	do {
/*
XXX:tm try nonblock
MSKB Article ID: Q147714 
Windows Sockets 2 Service Provider Interface Limitations
Polling with recv(MSG_PEEK) to determine when a complete message 
has arrived.
    Reason and Workaround not available.

Single-byte send() and recv(). 
    Reason: Couple one-byte sends with Nagle disabled.
    Workaround: Send modest amounts and receive as much as possible.
(still unused)
*/
		if (recv(fd, bp, 1, 0) <= 0)
			return -1;
		if (*bp == '\n')
			break;
		bp++;
		len--;
	} while (0 < len);
#else
	do {
		if ((n = fd_recv(fd, bp, len, MSG_PEEK)) <= 0)
			return -1;
		if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = fd_read(fd, bp, n)) < 0)
			return -1;
		bp += n;
		len -= n;
	} while (!newline && len);
#endif

	*bp = '\0';
	return bp - buf;
}

#if USE_SSL
gint ssl_gets(SSL *ssl, gchar *buf, gint len)
{
	gchar *newline, *bp = buf;
	gint n;

	if (--len < 1)
		return -1;
	do {
		if ((n = SSL_peek(ssl, bp, len)) <= 0)
			return -1;
		if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = SSL_read(ssl, bp, n)) < 0)
			return -1;
		bp += n;
		len -= n;
	} while (!newline && len);

	*bp = '\0';
	return bp - buf;
}
#endif

gint sock_gets(SockInfo *sock, gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

#if USE_SSL
	if (sock->ssl)
		return ssl_gets(sock->ssl, buf, len);
#endif
	return fd_gets(sock->sock, buf, len);
}

gchar *fd_getline(gint fd)
{
	gchar buf[BUFFSIZE];
	gchar *str = NULL;
	gint len;
	gulong size = 1;

	while ((len = fd_gets(fd, buf, sizeof(buf))) > 0) {
		size += len;
		if (!str)
			str = g_strdup(buf);
		else {
			str = g_realloc(str, size);
			strcat(str, buf);
		}
		if ((buf[len - 1] == '\n')
#ifdef WIN32
			|| (buf[len - 1] == '\r')
#endif
			)
			break;
	}
	if (len == -1) {
		log_error("Read from socket fd%d failed: %s\n",
			  fd, strerror(errno));
		if (str)
			g_free(str);
		return NULL;
	}

	return str;
}

#if USE_SSL
gchar *ssl_getline(SSL *ssl)
{
	gchar buf[BUFFSIZE];
	gchar *str = NULL;
	gint len;
	gulong size = 1;

	while ((len = ssl_gets(ssl, buf, sizeof(buf))) > 0) {
		size += len;
		if (!str)
			str = g_strdup(buf);
		else {
			str = g_realloc(str, size);
			strcat(str, buf);
		}
		if (buf[len - 1] == '\n')
			break;
	}

	return str;
}
#endif

gchar *sock_getline(SockInfo *sock)
{
	g_return_val_if_fail(sock != NULL, NULL);

#if USE_SSL
	if (sock->ssl)
		return ssl_getline(sock->ssl);
#endif
	return fd_getline(sock->sock);
}

gint sock_puts(SockInfo *sock, const gchar *buf)
{
	gint ret;

	if ((ret = sock_write(sock, buf, strlen(buf))) < 0)
		return ret;
	return sock_write(sock, "\r\n", 2);
}

/* peek at the next socket character without actually reading it */
gint sock_peek(SockInfo *sock)
{
	gint n;
	gchar ch;

	g_return_val_if_fail(sock != NULL, -1);

#if USE_SSL
	if (sock->ssl) {
		if ((n = SSL_peek(sock->ssl, &ch, 1)) < 0)
			return -1;
		else
			return ch;
	}
#endif
	if ((n = recv(sock->sock, &ch, 1, MSG_PEEK)) < 0)
		return -1;
	else
		return ch;
}

gint sock_close(SockInfo *sock)
{
	gint ret;

	if (!sock)
		return 0;

#if USE_SSL
	if (sock->ssl)
		ssl_done_socket(sock);
#endif
#ifdef WIN32
	shutdown(sock->sock,SD_SEND); /* complete transfer before close */
#endif 
	ret = fd_close(sock->sock); 
	g_free(sock->hostname);
	g_free(sock);

	return ret;
}

gint fd_close(gint fd)
{
	return close(fd);
}

gint sock_gdk_input_add(SockInfo *sock,
			GdkInputCondition condition,
			GdkInputFunction function,
			gpointer data)
{
	g_return_val_if_fail(sock != NULL, -1);

	/* :WK: We have to change some things here becuse most likey
	   function() does take SockInfo * and not an gint */
	return gdk_input_add(sock->sock, condition, function, data);
}
