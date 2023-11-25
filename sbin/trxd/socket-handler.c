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

/* Handle network clients over TCP/IP sockets */

#include <err.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
	printf("socket-handler: cleanup\n");
	close(fd);
}

static void
cleanup_sender(void *arg)
{
	sender_tag_t *s = (sender_tag_t *)arg;

	printf("socket-handler: cleanup sender\n");

	pthread_cancel(s->sender);
}

static void
cleanup_dispatcher(void *arg)
{
	dispatcher_tag_t *d = (dispatcher_tag_t *)arg;

	printf("socket-handler: cleanup dispatcher\n");

	pthread_cancel(d->dispatcher);
}

void *
socket_handler(void *arg)
{
	sender_tag_t *s;
	dispatcher_tag_t *d;
	int fd = *(int *)arg;
	char *buf;

	if (pthread_detach(pthread_self()))
		err(1, "socket-handler: pthread_detach");

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "trxd-sock"))
		err(1, "socket-handler: pthread_setname_np");

	s = malloc(sizeof(sender_tag_t));
	if (s == NULL)
		err(1, "socket-handler: malloc");
	s->data = NULL;
	s->socket = fd;
	if (pthread_mutex_init(&s->mutex, NULL))
		err(1, "socket-handler: pthread_mutex_init");

	if (pthread_cond_init(&s->cond, NULL))
		err(1, "socket-handler: pthread_cond_init");

	if (pthread_cond_init(&s->cond2, NULL))
		err(1, "socket-handler: pthread_cond_init");

	if (pthread_create(&s->sender, NULL, socket_sender, s))
		err(1, "socket-handler: pthread_create");

	pthread_cleanup_push(cleanup_sender, s);
	/* Create a dispatcher thread to dispatch incoming data */
	d = malloc(sizeof(dispatcher_tag_t));
	if (d == NULL)
		err(1, "socket-handler: malloc");
	d->sender = s;

	if (pthread_mutex_init(&d->mutex, NULL))
		err(1, "socket-handler: pthread_mutex_init");

	if (pthread_cond_init(&d->cond, NULL))
		err(1, "socket-handler: pthread_cond_init");

	if (pthread_create(&d->dispatcher, NULL, dispatcher, d))
		err(1, "socket-handler: pthread_create");

	pthread_cleanup_push(cleanup_dispatcher, d);

	for (;;) {
		/* buf will later be freed by the dispatcher */
		buf = trxd_readln(fd);

		if (buf == NULL)
			pthread_exit(NULL);
		else if (verbose)
			printf("socket-handler: <- %s\n", buf);

		if (pthread_mutex_lock(&d->mutex))
			err(1, "socket-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("socket-handler: dispatcher mutex locked\n");

		d->data = buf;

		pthread_cond_signal(&d->cond);

		if (pthread_cond_signal(&d->cond))
			err(1, "socket-handler: pthread_cond_signal");
		if (verbose > 1)
			printf("socket-handler: dispatcher cond signaled\n");

		if (pthread_mutex_unlock(&d->mutex))
			err(1, "socket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("socket-handler: dispatcher mutex unlocked\n");
	}
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);

	return NULL;
}
