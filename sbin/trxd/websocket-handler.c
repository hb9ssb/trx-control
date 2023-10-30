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

extern trx_controller_tag_t *trx_controller_tag;
extern int verbose;

#define BUFSIZE		65535

static int
websocket_recv(int fd, char **dest)
{
	unsigned char *data;
	char *buf;
	size_t nread, len, datasize;
	int type;

	printf("websocket_recv\n");
	buf = malloc(BUFSIZE);

	nread = len = 0;
	type = WS_INCOMPLETE_FRAME;
	do {
#if 0
		if (websock->ssl)
			nread = SSL_read(websock->ssl, buf + len,
			    BUFSIZE - len);
		else
#endif
			nread = recv(fd, buf + len,
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
#if 0
			if (websock->ssl) {
				SSL_write(websock->ssl, buf, datasize);
				SSL_shutdown(websock->ssl);
				SSL_free(websock->ssl);
				websock->ssl = NULL;
			} else {
#endif
				send(fd, buf, datasize, 0);
				close(fd);
				fd = -1;
#if 0
			}
#endif
			return -1;
			break;
		case WS_PING_FRAME:
			wsMakeFrame(NULL, 0, (unsigned char *)buf, &datasize,
			    WS_PONG_FRAME);
#if 0
			if (websock->ssl)
				SSL_write(websock->ssl, buf, datasize);
			else
#endif
				send(fd, buf, datasize, 0);
			len = 0;
			type = WS_INCOMPLETE_FRAME;
			break;
		case WS_TEXT_FRAME:
			printf("recv %d bytes,  %s\n", datasize, data);
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
	trx_controller_tag_t *t;
	sender_tag_t *s;
	int status, nread, n, terminate;
	char *buf, *p;
	const char *command, *param;

	/* Check if have a default transceiver */
	for (t = trx_controller_tag; t != NULL; t = t->next)
		if (t->is_default)
			break;

	/* If there is no default transceiver, use the first one */
	if (t == NULL)
		t = trx_controller_tag;

	if (pthread_detach(pthread_self()))
		err(1, "websocket-handler: pthread_detach");

	s = malloc(sizeof(sender_tag_t));
	if (s == NULL)
		err(1, "websocket-handler: malloc");
	t->sender = s;
	s->data = NULL;
	s->socket = w->socket;
	s->ssl = NULL;
	s->ctx = NULL;

	w->sender = s;

	if (pthread_mutex_init(&s->mutex, NULL))
		err(1, "websocket-handler: pthread_mutex_init");

	if (pthread_cond_init(&s->cond, NULL))
		err(1, "websocket-handler: pthread_cond_init");

	if (pthread_create(&s->sender, NULL, websocket_sender, s))
		err(1, "websocket-handler: pthread_create");

	for (terminate = 0; !terminate ;) {
		printf("wait for data\n");
		websocket_recv(w->socket, &buf);
		printf("got buf %s\n", buf);
		if (buf == NULL) {
			terminate = 1;
			buf = strdup("{\"request\": \"stop-status-updates\"}");
		}

		if (pthread_mutex_lock(&t->mutex))
			err(1, "websocket-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("websocket-handler: mutex locked\n");

		if (pthread_mutex_lock(&t->sender->mutex))
			err(1, "websocket-handler: pthread_mutex_lock");

		t->handler = "requestHandler";
		t->reply = NULL;
		t->data = buf;
		t->client_fd = w->socket;
		t->new_tag = t;

		if (pthread_mutex_lock(&t->mutex2))
			err(1, "websocket-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("websocket-handler: mutex2 locked\n");

		/* We signal cond, and mutex gets owned by trx-controller */
		if (pthread_cond_signal(&t->cond1))
			err(1, "websocket-handler: pthread_cond_signal");
		if (verbose > 1)
			printf("websocket-handler: cond1 signaled\n");

		if (pthread_mutex_unlock(&t->mutex2))
			err(1, "websocket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("websocket-handler: mutex unlocked\n");

		while (t->reply == NULL) {
			if (pthread_cond_wait(&t->cond2, &t->mutex2))
				err(1, "websocket-handler: pthread_cond_wait");
			if (verbose > 1)
				printf("websocket-handler: cond2 changed\n");
		}

		free(buf);
		if (strlen(t->reply) > 0 && !terminate) {
			printf("calling the sender thread\n");
			t->sender->data = t->reply;
			if (pthread_cond_signal(&t->sender->cond))
				err(1, "websocket-handler: pthread_cond_signal");
			pthread_mutex_unlock(&t->sender->mutex);
		} else {
			printf("not calling the sender thread\n");
			pthread_mutex_unlock(&t->sender->mutex);
		}
		/* Check if we changed the transceiver */
		if (t->new_tag != t)
			t = t->new_tag;

		if (pthread_mutex_unlock(&t->mutex2))
			err(1, "websocket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("websocket-handler: mutex2 unlocked\n");

		if (pthread_mutex_unlock(&t->mutex))
			err(1, "websocket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("websocket-handler: mutex unlocked\n");
	}
terminate:
	close(w->socket);
	free(arg);
	return NULL;
}
