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

/* Signal incoming data from a file descriptor (usually a socket) */

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
	free(arg);
}

void *
signal_input(void *arg)
{
	signal_input_t *i = (signal_input_t *)arg;
	extension_tag_t *e;
	struct pollfd pfd;

	e = i->extension;

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "signal-input: pthread_detach");
		exit(1);
	}

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "signal-input")) {
		syslog(LOG_ERR, "signal-input: pthread_setname_np");
		exit(1);
	}

	pfd.fd = i->fd;
	pfd.events = POLLIN;

	for (;;) {
		if (poll(&pfd, 1, -1) == -1) {
			syslog(LOG_ERR, "signal-input: poll");
			exit(1);
		}
		if (pfd.revents) {

			if (pthread_mutex_lock(&e->mutex2)) {
				syslog(LOG_ERR,
				    "signal-input: pthread_mutex_lock");
				exit(1);
			}

			e->done = 0;
			lua_getglobal(e->L, i->func);
			lua_pushinteger(e->L, i->fd);

			e->call = 1;
			pthread_cond_signal(&e->cond1);

			while (!e->done)
				pthread_cond_wait(&e->cond2, &e->mutex2);

			e->done = 0;

			pthread_mutex_unlock(&e->mutex2);
		}
	};
	pthread_cleanup_pop(0);
	return NULL;
}
