/*
 * Copyright (c) 2023 Marc Balmer HB9SSB
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

/* Handle network clients WebSockets */

#include <sys/socket.h>

#include <openssl/ssl.h>

#include <err.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"
#include "websocket.h"

extern void *websocket_sender(void *);
extern void *dispatcher(void *);

extern int verbose;

#define BUFSIZE		65535

static int
websocket_recv(websocket_t *websock, char **dest)
{
	unsigned char *data;
	char *buf;
	size_t nread, len, datasize;
	int type;

	buf = malloc(BUFSIZE);

	nread = len = 0;
	type = WS_INCOMPLETE_FRAME;
	do {
		if (websock->ssl)
			nread = SSL_read(websock->ssl, buf + len,
			    BUFSIZE - len);
		else
			nread = recv(websock->socket, buf + len,
			    BUFSIZE - len, 0);
		if (nread <= 0) {
			type = WS_EMPTY_FRAME;
		} else {
			len += nread;
			type = wsParseInputFrame((unsigned char *)buf, len,
			    &data, &datasize);
		}
		switch (type) {
		case WS_CLOSING_FRAME:
			wsMakeFrame(NULL, 0, (unsigned char *)buf, &datasize,
			    WS_CLOSING_FRAME);
			if (websock->ssl) {
				SSL_write(websock->ssl, buf, datasize);
				SSL_shutdown(websock->ssl);
				SSL_free(websock->ssl);
				websock->ssl = NULL;
			} else {
				send(websock->socket, buf, datasize, 0);
				close(websock->socket);
				websock->socket = -1;
			}
			return -1;
			break;
		case WS_PING_FRAME:
			wsMakeFrame(NULL, 0, (unsigned char *)buf, &datasize,
			    WS_PONG_FRAME);
			if (websock->ssl)
				SSL_write(websock->ssl, buf, datasize);
			else
				send(websock->socket, buf, datasize, 0);
			len = 0;
			type = WS_INCOMPLETE_FRAME;
			break;
		case WS_TEXT_FRAME:
			data[datasize] = '\0';
			*dest = strdup(data);
			break;
		case WS_INCOMPLETE_FRAME:
			break;
		default:
			return -1;
		}
	} while (type == WS_INCOMPLETE_FRAME);
	free(buf);
	return 0;
}

void *
websocket_handler(void *arg)
{
	websocket_t *w = (websocket_t *)arg;
	sender_tag_t *s;
	dispatcher_tag_t *d;
	int status, nread, n, terminate;
	char *buf, *p;
	const char *command, *param;

	if (pthread_detach(pthread_self()))
		err(1, "websocket-handler: pthread_detach");

	if (pthread_setname_np(pthread_self(), "wsock-handler"))
		err(1, "websocket-handler: pthread_setname_np");

	/* Create a websocket-sender thread to send data to the client */
	s = malloc(sizeof(sender_tag_t));
	if (s == NULL)
		err(1, "websocket-handler: malloc");
	s->data = NULL;
	s->socket = w->socket;
	s->ssl = w->ssl;
	s->ctx = w->ctx;

	w->sender = s;

	if (pthread_mutex_init(&s->mutex, NULL))
		err(1, "websocket-handler: pthread_mutex_init");

	if (pthread_cond_init(&s->cond, NULL))
		err(1, "websocket-handler: pthread_cond_init");

	if (pthread_create(&s->sender, NULL, websocket_sender, s))
		err(1, "websocket-handler: pthread_create");

	/* Create a dispatcher thread to dispatch incoming data */
	d = malloc(sizeof(dispatcher_tag_t));
	if (d == NULL)
		err(1, "websocket-handler: malloc");
	d->data = NULL;
	d->sender = s;

	if (pthread_mutex_init(&d->mutex, NULL))
		err(1, "websocket-handler: pthread_mutex_init");

	if (pthread_cond_init(&d->cond, NULL))
		err(1, "websocket-handler: pthread_cond_init");

	if (pthread_create(&d->dispatcher, NULL, dispatcher, d))
		err(1, "websocket-handler: pthread_create");

	for (terminate = 0; !terminate ;) {
		if (websocket_recv(w, &buf) == -1) {
			pthread_cancel(s->sender);
			pthread_join(s->sender, NULL);
			terminate = 1;
			buf = strdup("{\"request\": \"stop-status-updates\"}");
		} else if (verbose)
			printf("websocket-handler: <- %s\n", buf);

		if (pthread_mutex_lock(&d->mutex))
			err(1, "socket-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("socket-handler: dispatcher mutex locked\n");

		d->data = buf;
		d->terminate = terminate;

		pthread_cond_signal(&d->cond);

		if (pthread_cond_signal(&d->cond))
			err(1, "websocket-handler: pthread_cond_signal");
		if (verbose > 1)
			printf("websocket-handler: dispatcher cond signaled\n");

		if (pthread_mutex_unlock(&d->mutex))
			err(1, "websocket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("websocket-handler: dispatcher mutex unlocked\n");
	}
terminate:
	if (verbose)
		printf("websocket-handler: terminating\n");
	close(w->socket);
	free(arg);
	return NULL;
}
