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
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"

#define STATUS_REQUEST	"{\"request\": \"get-frequency\"}"
#define POLLING_INTERVAL	200000	/* microseconds */

void *
trx_poller(void *arg)
{
	command_tag_t *t = (command_tag_t *)arg;
	struct timeval tv;
	int fd = *(int *)arg;
	int status, nread, n;
	char buf[256], out[1024], *p;
	const char *command, *param;


	status = pthread_detach(pthread_self());
	if (status)
		err(1, "can't detach");

	while (t->poller_running) {
		pthread_mutex_lock(&t->mutex);
		pthread_mutex_lock(&t->rmutex);

		/* XXX Maybe create a specific handler for the poller */
		t->handler = "requestHandler";
		t->data = STATUS_REQUEST;
		t->client_fd = 0;

		pthread_cond_signal(&t->cond);
		pthread_mutex_unlock(&t->mutex);

		pthread_cond_wait(&t->rcond, &t->rmutex);

		pthread_mutex_unlock(&t->rmutex);
		usleep(POLLING_INTERVAL);
	}
	return NULL;
}
