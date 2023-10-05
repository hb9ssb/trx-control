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

/* Handle transceivers that need polling for status updates */

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"

#define STATUS_REQUEST	"{\"request\": \"status-update\"}"
#define POLLING_INTERVAL	200000	/* microseconds */

void *
trx_poller(void *arg)
{
	command_tag_t *t = (command_tag_t *)arg;

	if (pthread_detach(pthread_self()))
		err(1, "trx-poller: pthread_detach");

	while (t->poller_running) {
		if (pthread_mutex_lock(&t->mutex))
			err(1, "trx-poller: pthread_mutex_lock (mutex)");

		if (pthread_mutex_lock(&t->rmutex))
			err(1, "trx-poller: pthread_mutex_lock (rmutex)");

		t->handler = "pollHandler";

		if (pthread_cond_signal(&t->qcond))
			err(1, "trx-poller: pthread_cond_signal");

		if (pthread_cond_wait(&t->rcond, &t->rmutex))
			err(1, "trx-poller: pthread_cond_wait");

		if (pthread_mutex_unlock(&t->rmutex))
			err(1, "trx-poller: pthread_mutex_unlock (rmutex)");
		if (pthread_mutex_unlock(&t->mutex))
			err(1, "trx-poller: pthread_mutex_unlock (mutex)");
		usleep(POLLING_INTERVAL);
	}
	return NULL;
}
