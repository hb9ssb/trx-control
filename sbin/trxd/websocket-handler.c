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

/* Handle network clients over WebSockets */

#include <sys/socket.h>

#include <openssl/ssl.h>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"
#include "websocket.h"

extern void *websocket_sender(void *);
extern void *dispatcher(void *);

extern int verbose;

static void
cleanup(void *arg)
{
	websocket_t *w = (websocket_t *)arg;

	if (verbose)
		syslog(LOG_NOTICE, "websocket-handler: terminating\n");

	if (w->ssl) {
		SSL_shutdown(w->ssl);
		SSL_free(w->ssl);
	} else
		close(w->socket);

	free(arg);
}

static void
cleanup_sender(void *arg)
{
	sender_tag_t *s = (sender_tag_t *)arg;

	pthread_cancel(s->sender);
}

static void
cleanup_dispatcher(void *arg)
{
	dispatcher_tag_t *d = (dispatcher_tag_t *)arg;

	pthread_cancel(d->dispatcher);
}

static int
websocket_read(void *data, unsigned char *dest, size_t len)
{
	websocket_t *websock = (websocket_t *)data;

	if (websock->ssl)
		return SSL_read(websock->ssl, dest, len);
	else
		return recv(websock->socket, dest, len, 0);
}

static int
websocket_write(void *data, unsigned char *dest, size_t len)
{
	websocket_t *websock = (websocket_t *)data;

	if (websock->ssl)
		return SSL_write(websock->ssl, dest, len);
	else
		return send(websock->socket, dest, len, 0);
}

void
sigpipe_handler(int unused)
{
	pthread_exit(NULL);
}

void *
websocket_handler(void *arg)
{
	websocket_t *w = (websocket_t *)arg;
	sender_tag_t *s;
	dispatcher_tag_t *d;
	char *buf;

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "websocket-handler: pthread_detach");
		exit(1);
	}

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "websocket")) {
		syslog(LOG_ERR, "websocket-handler: pthread_setname_np");
		exit(1);
	}

	/* Create a websocket-sender thread to send data to the client */
	s = malloc(sizeof(sender_tag_t));
	if (s == NULL) {
		syslog(LOG_ERR, "websocket-handler: malloc");
		exit(1);
	}
	s->data = (char *)1;
	s->socket = w->socket;
	s->ssl = w->ssl;
	s->ctx = w->ctx;

	w->sender = s;

	if (pthread_mutex_init(&s->mutex, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_mutex_init(&s->mutex2, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_cond_init(&s->cond, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_cond_init(&s->cond2, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_create(&s->sender, NULL, websocket_sender, s)) {
		syslog(LOG_ERR, "websocket-handler: pthread_create");
		exit(1);
	}

	pthread_cleanup_push(cleanup_sender, s);

	/* Create a dispatcher thread to dispatch incoming data */
	d = malloc(sizeof(dispatcher_tag_t));
	if (d == NULL) {
		syslog(LOG_ERR, "websocket-handler: malloc");
		exit(1);
	}
	d->data = (char *)1;
	d->sender = s;

	if (pthread_mutex_init(&d->mutex, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_mutex_init(&d->mutex2, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_cond_init(&d->cond, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_cond_init(&d->cond2, NULL)) {
		syslog(LOG_ERR, "websocket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_create(&d->dispatcher, NULL, dispatcher, d)) {
		syslog(LOG_ERR, "websocket-handler: pthread_create");
		exit(1);
	}

	pthread_cleanup_push(cleanup_dispatcher, d);

	if (pthread_mutex_lock(&d->mutex2)) {
		syslog(LOG_ERR, "websocket-handler: pthread_mutex_lock");
		exit(1);
	}

	if (verbose)
		printf("websocket-handler: wait for dispatcher\n");

	while (d->data != NULL)
		if (pthread_cond_wait(&d->cond2, &d->mutex2)) {
			syslog(LOG_ERR, "wbsocket-handler: pthread_cond_wait");
			exit(1);
		}

	if (pthread_mutex_lock(&s->mutex2)) {
		syslog(LOG_ERR, "websocket-handler: pthread_mutex_lock");
		exit(1);
	}

	if (verbose)
		printf("websocket-handler: wait for sender\n");

	while (s->data != NULL)
		if (pthread_cond_wait(&s->cond2, &s->mutex2)) {
			syslog(LOG_ERR, "websocket-handler: pthread_cond_wait");
			exit(1);
		}

	sigaction(SIGPIPE, &(struct sigaction){sigpipe_handler}, NULL);

	if (verbose)
		printf("websocket-handler: sender is ready\n");

	for (;;) {
		/* buf will later be freed by the dispatcher */
		if (wsRead(&buf, NULL, websocket_read, websocket_write, w)) {
			if (verbose)
				printf("websocket-handler: short read: %s\n",
					strerror(errno));
			pthread_exit(NULL);
		} else if (verbose && buf != NULL)
			printf("websocket-handler: <- %s\n", buf);

		if (buf != NULL) {
			d->data = buf;

			if (pthread_cond_signal(&d->cond)) {
				syslog(LOG_ERR,
				    "websocket-handler: pthread_cond_signal");
				exit(1);
			}

			while (d->data != NULL)
				if (pthread_cond_wait(&d->cond2, &d->mutex2)) {
					syslog(LOG_ERR,
					    "websocket-handler: "
					    "pthread_cond_wait");
					exit(1);
				}
		} else
			syslog(LOG_ERR, "websocket-handler: received empty "
			    "text frame, skipping it.");
	}
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	return NULL;
}
