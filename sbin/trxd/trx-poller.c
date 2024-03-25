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

/* Handle transceivers that require polling for status updates */

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "trxd.h"

#define STATUS_REQUEST	"{\"request\": \"status-update\"}"
#define POLLING_INTERVAL	200000	/* microseconds */

static void
cleanup(void *arg)
{
	free(arg);
}

void *
trx_poller(void *arg)
{
	trx_controller_tag_t *t = (trx_controller_tag_t *)arg;

	if (pthread_detach(pthread_self()))
		err(1, "trx-poller: pthread_detach");

	pthread_cleanup_push(cleanup, NULL);

	if (pthread_setname_np(pthread_self(), "trx-poller"))
		err(1, "trx-poller: pthread_setname_np");

	for (;;) {
		if (pthread_mutex_lock(&t->mutex))
			err(1, "trx-poller: pthread_mutex_lock");

		t->handler = "pollHandler";
		t->response = NULL;
		t->data  = NULL;

		if (pthread_mutex_lock(&t->mutex2))
			err(1, "trx-poller: pthread_mutex_lock");

		if (pthread_cond_signal(&t->cond1))
			err(1, "trx-poller: pthread_cond_signal");

		if (pthread_mutex_unlock(&t->mutex2))
			err(1, "trx-poller: pthread_mutex_unlock");

		while (t->response == NULL) {
			if (pthread_cond_wait(&t->cond2, &t->mutex2))
				err(1, "trx-poller: pthread_cond_wait");
		}

		if (strlen(t->response))
			printf("trx-poller: unexpected response '%s'\n",
			    t->response);

		if (pthread_mutex_unlock(&t->mutex2))
			err(1, "trx-poller: pthread_mutex_unlock");

		if (pthread_mutex_unlock(&t->mutex))
			err(1, "trx-poller: pthread_mutex_unlock");

		usleep(POLLING_INTERVAL);
	}
	pthread_cleanup_pop(0);

	return NULL;
}
