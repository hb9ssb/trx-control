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

void *
dispatcher(void *arg)
{
	sender_tag_t *s = (sender_tag_t *)arg;
	int status, nread, n, terminate;
	char *buf, *p;
	const char *command, *param;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_detach(pthread_self()))
		err(1, "dispatcher: pthread_detach");

	if (pthread_mutex_lock(&s->mutex))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: mutex locked\n");

	for (terminate = 0; !terminate ;) {
		printf("dispatcher: wait for condition to change\n");
		while (s->data == NULL) {
			if (pthread_cond_wait(&s->cond, &s->mutex))
				err(1, "dispatcher: pthread_cond_wait");
			if (verbose > 1)
				printf("dispatcher: cond changed\n");
		}
	}
	pthread_cleanup_pop(0);
	return NULL;
}
