/*
 * Copyright (c) 2023 - 2026 Marc Balmer HB9SSB
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

/* Handle network clients over TCP/IP sockets */

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"

extern void *socket_sender(void *);
extern void *dispatcher(void *);

extern trx_controller_tag_t *trx_controller_tag;
extern int verbose;

static void
cleanup(void *arg)
{
	int fd = *(int *)arg;
	close(fd);
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

void *
socket_handler(void *arg)
{
	sender_tag_t *s;
	dispatcher_tag_t *d;
	int fd = *(int *)arg;
	char *buf;

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "socket-handler: pthread_detach");
		exit(1);
	}

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "socket")) {
		syslog(LOG_ERR, "socket-handler: pthread_setname_np");
		exit(1);
	}

	/* Create a socket-sender thread to send data to the client */
	s = malloc(sizeof(sender_tag_t));
	if (s == NULL) {
		syslog(LOG_ERR, "socket-handler: malloc");
		exit(1);
	}
	s->data = (char *)1;
	s->socket = fd;

	if (pthread_mutex_init(&s->mutex, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_mutex_init(&s->mutex2, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_cond_init(&s->cond, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_cond_init(&s->cond2, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_create(&s->sender, NULL, socket_sender, s)) {
		syslog(LOG_ERR, "socket-handler: pthread_create");
		exit(1);
	}

	pthread_cleanup_push(cleanup_sender, s);

	/* Create a dispatcher thread to dispatch incoming data */
	d = malloc(sizeof(dispatcher_tag_t));
	if (d == NULL) {
		syslog(LOG_ERR, "socket-handler: malloc");
		exit(1);
	}
	d->data = (char *)1;
	d->sender = s;

	if (pthread_mutex_init(&d->mutex, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_mutex_init(&d->mutex2, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_mutex_init");
		exit(1);
	}

	if (pthread_cond_init(&d->cond, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_cond_init(&d->cond2, NULL)) {
		syslog(LOG_ERR, "socket-handler: pthread_cond_init");
		exit(1);
	}

	if (pthread_create(&d->dispatcher, NULL, dispatcher, d)) {
		syslog(LOG_ERR, "socket-handler: pthread_create");
		exit(1);
	}

	pthread_cleanup_push(cleanup_dispatcher, d);

	if (pthread_mutex_lock(&d->mutex2)) {
		syslog(LOG_ERR, "socket-handler: pthread_mutex_lock");
		exit(1);
	}

	if (verbose)
		printf("socket-handler: wait for dispatcher\n");

	while (d->data != NULL)
		if (pthread_cond_wait(&d->cond2, &d->mutex2)) {
			syslog(LOG_ERR, "socket-handler: pthread_cond_wait");
			exit(1);
		}

	if (pthread_mutex_lock(&s->mutex2)) {
		syslog(LOG_ERR, "socket-handler: pthread_mutex_lock");
		exit(1);
	}

	if (verbose)
		printf("socket-handler: wait for sender\n");

	while (s->data != NULL)
		if (pthread_cond_wait(&s->cond2, &s->mutex2)) {
			syslog(LOG_ERR, "socket-handler: pthread_cond_wait");
			exit(1);
		}

	if (verbose)
		printf("socket-handler: sender is ready\n");

	for (;;) {
		/* buf will later be freed by the dispatcher */
		buf = trxd_readln(fd);

		if (buf == NULL)
			pthread_exit(NULL);
		else if (verbose)
			printf("socket-handler: <- %s\n", buf);

		d->data = buf;
		d->len = strlen(buf);

		if (pthread_cond_signal(&d->cond)) {
			syslog(LOG_ERR, "socket-handler: pthread_cond_signal");
			exit(1);
		}

		while (d->data != NULL)
			if (pthread_cond_wait(&d->cond2, &d->mutex2)) {
				syslog(LOG_ERR, "socket-handler: "
				    "pthread_cond_wait");
			exit(1);
		}
	}
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);

	return NULL;
}
