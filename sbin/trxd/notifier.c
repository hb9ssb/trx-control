/*
 * Copyright (c) 2024 Marc Balmer HB9SSB
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

/* Notify listeners (connected clients or extensions) of events */

#include <err.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"

extern int verbose;


static void
cleanup(void *arg)
{
	free(arg);
}

void *
notifier(void *arg)
{
	dispatcher_tag_t *d = (dispatcher_tag_t *)arg;
	trx_controller_tag_t *t;
	destination_t *to, *dst;
	lua_State *L;
	int status, request;
	const char *dest, *req;

	if (pthread_detach(pthread_self()))
		err(1, "notifier: pthread_detach");

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "notifier"))
		err(1, "notifier: pthread_setname_np");

	if (pthread_mutex_lock(&d->mutex))
		err(1, "notifier: pthread_mutex_lock");

	if (verbose)
		printf("notifier: ready\n");

	d->data = NULL;
	if (pthread_cond_signal(&d->cond2))
		err(1, "c: pthread_cond_signal");

	for (;;) {
		while (d->data == NULL) {
			if (pthread_cond_wait(&d->cond, &d->mutex))
				err(1, "notifier: pthread_cond_wait");
		}
	}
	pthread_cleanup_pop(0);
	return NULL;
}
