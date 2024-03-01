/*
 * Copyright (c) 2023 - 2024 Marc Balmer HB9SSB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <netdb.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>


#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"
#include "websocket.h"

#define MAXLISTEN	16

#define BIND_ADDR	"localhost"
#define LISTEN_PORT	"14290"

extern void *websocket_handler(void *);
extern int log_connections;

#define BUFSIZE		65535

static int
websocket_handshake(websocket_t *websock, char *path)
{
	struct handshake hs;
	size_t nread;
	char *buf;
	int rv = -1;

	nullHandshake(&hs);

	buf = malloc(BUFSIZE);
	if (websock->ssl)
		nread = SSL_read(websock->ssl, buf, BUFSIZE);
	else
		nread = recv(websock->socket, buf, BUFSIZE, 0);
	buf[nread] = '\0';

	if (wsParseHandshake((unsigned char *)buf, nread, &hs) ==
	    WS_OPENING_FRAME) {
		if (!strcmp(hs.resource, path)) {
			wsGetHandshakeAnswer(&hs, (unsigned char *)buf, &nread);
			freeHandshake(&hs);
			if (websock->ssl)
				SSL_write(websock->ssl, buf, nread);
			else
				send(websock->socket, buf, nread, 0);
			buf[nread] = '\0';
			rv = 0;
		} else {
			nread = sprintf(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
			if (websock->ssl)
				SSL_write(websock->ssl, buf, nread);
			else
				send(websock->socket, buf, nread, 0);
		}
	} else {
		nread = sprintf(buf,
			"HTTP/1.1 400 Bad Request\r\n"
			"%s%s\r\n\r\n",
			versionField,
			version);
		if (websock->ssl)
			SSL_write(websock->ssl, buf, nread);
		else
			send(websock->socket, buf, nread, 0);
	}
	free(buf);
	return rv;
}

void *
websocket_listener(void *arg)
{
	websocket_listener_t *t = (websocket_listener_t *)arg;
	struct addrinfo hints, *res, *res0;
	int listen_fd[MAXLISTEN], i, error, val, ret;

	if (pthread_detach(pthread_self()))
		err(1, "websocket-listener: pthread_detach");

	if (pthread_setname_np(pthread_self(), "ws-listener"))
		err(1, "websocket-listener: pthread_setname_np");

	/* Setup network listening */
	for (i = 0; i < MAXLISTEN; i++)
		listen_fd[i] = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(t->bind_addr, t->listen_port, &hints, &res0);
	if (error) {
		syslog(LOG_ERR, "getaddrinfo: %s:%s: %s",
		    t->bind_addr, t->listen_port, gai_strerror(error));
		exit(1);
	}

	i = 0;
	for (res = res0; res != NULL && i < MAXLISTEN;
	    res = res->ai_next) {
		listen_fd[i] = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (listen_fd[i] < 0)
			continue;
		if (fcntl(listen_fd[i], F_SETFL, fcntl(listen_fd[i],
		    F_GETFL) | O_NONBLOCK)) {
			syslog(LOG_ERR, "fcntl: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		val = 1;
		if (setsockopt(listen_fd[i], SOL_SOCKET, SO_REUSEADDR,
		    (const char *)&val,
		    sizeof(val))) {
			syslog(LOG_ERR, "setsockopt: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		if (bind(listen_fd[i], res->ai_addr,
		    res->ai_addrlen)) {
			syslog(LOG_ERR, "bind: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		if (listen(listen_fd[i], 5)) {
			syslog(LOG_ERR, "listen: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		i++;
	}

	if (t->certificate != NULL) {
		SSL_library_init();
		SSL_load_error_strings();
		if ((t->ctx = SSL_CTX_new(TLSv1_2_method())) == NULL)
			err(1, "websocket-listener: can't create SSL context");

		if (SSL_CTX_use_certificate_chain_file(t->ctx, t->certificate)
		    != 1)
			err(1, "websocket-listener: can't load certificate");
		if (SSL_CTX_use_PrivateKey_file(t->ctx, t->certificate,
		    SSL_FILETYPE_PEM) != 1)
			err(1, "websocket-listener: error loading private key");
	}

	/* Wait for connections as long as websocket_listener runs */
	for (;;) {
		struct timeval	 tv;
		fd_set		 readfds;
		int		 r, maxfd = -1;

		FD_ZERO(&readfds);
		for (i = 0; i < MAXLISTEN; ++i) {
			if (listen_fd[i] != -1) {
				FD_SET(listen_fd[i], &readfds);
				if (listen_fd[i] > maxfd)
					maxfd = listen_fd[i];
			}
		}
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		r = select(maxfd + 1, &readfds, NULL, NULL, &tv);
		if (r < 0) {
			if (errno != EINTR) {
				syslog(LOG_ERR, "select: %s", strerror(errno));
				break;
			}
		} else if (r == 0)
			continue;

		for (i = 0; i < MAXLISTEN; ++i) {
			struct sockaddr_storage	 sa;
			socklen_t		 len;
			int			*client_fd;
			char			 hbuf[NI_MAXHOST];
			websocket_t		*w;

			client_fd = malloc(sizeof(int));

			if (listen_fd[i] == -1 ||
			    !FD_ISSET(listen_fd[i], &readfds))
				continue;
			memset(&sa, 0, sizeof(sa));
			len = sizeof(sa);
			*client_fd = accept(listen_fd[i],
			    (struct sockaddr *)&sa, &len);
			if (*client_fd < 0) {
				syslog(LOG_ERR, "accept: %s", strerror(errno));
				free(client_fd);
				break;
			}
			error = getnameinfo((struct sockaddr *)&sa, len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
			if (error)
				syslog(LOG_ERR, "getnameinfo: %s",
				    gai_strerror(error));

			if (log_connections)
				syslog(LOG_INFO, "websocket connection from %s",
				    hbuf);
			w = malloc(sizeof(websocket_t));
			w->socket = *client_fd;
			w->ssl = NULL;
			w->ctx = NULL;

			if (t->ctx != NULL) {
				w->ctx = t->ctx;
				if ((w->ssl = SSL_new(w->ctx)) == NULL)
					warnx("websocket-listener: can't "
					    "create SSL context");

				if (!SSL_set_fd(w->ssl, w->socket))
					warnx("can't set SSL socket");
				if ((ret = SSL_accept(w->ssl)) <= 0) {
					warnx("can't accept SSL "
					    "connection: SSL error code %d",
					    SSL_get_error(w->ssl, ret));
					close(w->socket);
					free(w->ssl);
					free(w);
					continue;
				}
			}

			if (!websocket_handshake(w, t->path)) {
				pthread_create(&w->listen_thread, NULL,
				    websocket_handler, w);
			} else {
				close(w->socket);
				free(w->ssl);
				free(w);
			}
		}
	}
	closelog();
	return 0;
}
