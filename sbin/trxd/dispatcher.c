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

/* Dispatch incoming data from (web)socket-handlers */

#include <err.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"

extern trx_controller_tag_t *trx_controller_tag;
extern int verbose;

static void
cleanup(void *arg)
{
	if (verbose > 1)
		printf("dispatcher: cleanup\n");
	free(arg);
}

void
call_trx_controller(dispatcher_tag_t *d)
{
	trx_controller_tag_t *t = d->trx_controller;

	printf("call_trx_controller\n");

	if (pthread_mutex_lock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: mutex locked\n");

	if (pthread_mutex_lock(&t->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: sender mutex locked\n");

	t->handler = "requestHandler";
	t->reply = NULL;
	t->data = d->data;
	t->new_tag = t;

	if (pthread_mutex_lock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: mutex2 locked\n");

	/* We signal cond, and mutex gets owned by trx-controller */
	if (pthread_cond_signal(&t->cond1))
		err(1, "dispatcher: pthread_cond_signal");
	if (verbose > 1)
		printf("dispatcher: cond1 signaled\n");

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");
	if (verbose > 1)
		printf("dispatcher: mutex unlocked\n");

	while (t->reply == NULL) {
		if (pthread_cond_wait(&t->cond2, &t->mutex2))
			err(1, "dispatcher: pthread_cond_wait");
		if (verbose > 1)
			printf("dispatcher: cond2 changed\n");
	}

	free(d->data);
	if (strlen(t->reply) > 0 && !d->terminate) {
		printf("calling the sender thread\n");
		t->sender->data = t->reply;
		if (pthread_cond_signal(&t->sender->cond))
			err(1, "socket-handler: pthread_cond_signal");
		pthread_mutex_unlock(&t->sender->mutex);
	} else {
		printf("not calling the sender thread\n");
		pthread_mutex_unlock(&t->sender->mutex);
	}

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");
	if (verbose > 1)
		printf("dispatcher: mutex2 unlocked\n");

	if (pthread_mutex_unlock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_unlock");
	if (verbose > 1)
		printf("dispatcher: mutex unlocked\n");

	/* Check if we changed the transceiver, keep the sender! */
	if (t->new_tag != t) {
		t->new_tag->sender = t->sender;
		d->trx_controller = t->new_tag;
	}
}

void
dispatch(dispatcher_tag_t *d)
{
	call_trx_controller(d);
}

void *
dispatcher(void *arg)
{
	dispatcher_tag_t *d = (dispatcher_tag_t *)arg;
	trx_controller_tag_t *t;
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

	t->sender = d->sender;
	d->trx_controller = t;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_detach(pthread_self()))
		err(1, "dispatcher: pthread_detach");

	if (pthread_setname_np(pthread_self(), "dispatcher"))
		err(1, "dispatcher: pthread_setname_np");

	if (pthread_mutex_lock(&d->mutex))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: mutex locked\n");

	for (terminate = 0; !terminate ;) {
		printf("dispatcher: wait for condition to change\n");
		while (d->data == NULL) {
			if (pthread_cond_wait(&d->cond, &d->mutex))
				err(1, "dispatcher: pthread_cond_wait");
			if (verbose > 1)
				printf("dispatcher: cond changed\n");
		}
		dispatch(d);
		d->data = NULL;
	}
	pthread_cleanup_pop(0);
	return NULL;
}
