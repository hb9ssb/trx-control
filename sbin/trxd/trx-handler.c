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

/* Handle incoming data from the trx */

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "trxd.h"

extern int verbose;

static void
cleanup(void *arg)
{
	printf("trx-handler: cleanup\n");
	free(arg);
}

void *
trx_handler(void *arg)
{
	trx_controller_tag_t *t = (trx_controller_tag_t *)arg;
	struct pollfd pfd;
	int n, fd;
	char buf[128];

	if (pthread_detach(pthread_self()))
		err(1, "trx-handler: pthread_detach");

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "trxd-trx"))
		err(1, "trx-handler: pthread_setname_np");

	fd = t->cat_device;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (;;) {
		if (pthread_mutex_lock(&t->mutex))
			err(1, "trx-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("trx-handler: mutex locked\n");

		if (poll(&pfd, 1, 0) == -1)
			err(1, "trx-handler: poll");
		if (pfd.revents) {
			for (n = 0; n < sizeof(buf) - 1; n++) {
				read(fd, &buf[n], 1);
				if (buf[n] == t->handler_eol)
					break;
			}
			buf[++n] = '\0';

			t->handler = "dataHandler";
			t->reply = NULL;
			t->data = buf;
			t->client_fd = 0;

			if (pthread_mutex_lock(&t->mutex2))
				err(1, "trx-handler: pthread_mutex_lock");
			if (verbose > 1)
				printf("trx-handler: mutex2 locked\n");

			if (pthread_cond_signal(&t->cond1))
				err(1, "trx-handler: pthread_cond_signal");
			if (verbose > 1)
				printf("trx-handler: cond1 signaled\n");

			if (pthread_mutex_unlock(&t->mutex2))
				err(1, "trx-handler: pthread_mutex_unlock");
			if (verbose > 1)
				printf("trx-handler: mutex2 unlocked\n");

			while (t->reply == NULL) {
				if (pthread_cond_wait(&t->cond2, &t->mutex2))
					err(1, "trx-handler: "
					    "pthread_cond_wait");
			}
			if (pthread_mutex_unlock(&t->mutex))
				err(1, "trx-handler: pthread_mutex_unlock");
			if (verbose > 1)
				printf("trx-handler: mutex unlocked\n");

		}
	};
	pthread_cleanup_pop(0);
	return NULL;
}
