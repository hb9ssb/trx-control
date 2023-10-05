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

/* Handle network clients */

#include <err.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"

extern command_tag_t *command_tag;
extern int verbose;

void *
client_handler(void *arg)
{
	command_tag_t *t;
	int fd = *(int *)arg;
	int status, nread, n, terminate;
	char *buf, *p;
	const char *command, *param;

	/* Check if have a default transceiver */
	for (t = command_tag; t != NULL; t = t->next)
		if (t->is_default)
			break;

	/* If there is no default transceiver, use the firs one */
	if (t == NULL)
		t = command_tag;

	if (pthread_detach(pthread_self()))
		err(1, "client-handler: pthread_detach");

	for (terminate = 0; !terminate ;) {
		buf = trxd_readln(fd);

		if (buf == NULL) {
			terminate = 1;
			buf = strdup("{\"request\": \"stop-status-updates\"}");
		}

		if (pthread_mutex_lock(&t->mutex))
			err(1, "client-handler: pthread_mutex_lock (mutex)");
		if (verbose > 1)
			printf("client-handler: mutex locked\n");
		if (pthread_mutex_lock(&t->rmutex))
			err(1, "client-handler: pthread_mutex_lock (rmutex)");
		if (verbose > 1)
			printf("client-handler: rmutex locked\n");

		t->handler = "requestHandler";
		t->data = buf;
		t->client_fd = fd;
		t->new_tag = t;

		if (pthread_mutex_lock(&t->qmutex))
			err(1, "client-handler: pthread_mutex_lock (qmutex)");

		if (pthread_cond_signal(&t->qcond))
			err(1, "client-handler: pthread_cond_signal");
		if (pthread_mutex_unlock(&t->qmutex))
			err(1, "client-handler: pthread_mutex_unlock (qmutex)");

		if (verbose > 1)
			printf("client-handler: qcond signaled\n");


		if (pthread_cond_wait(&t->rcond, &t->rmutex))
			err(1, "client-handler: pthread_cond_signal");

		if (pthread_mutex_unlock(&t->rmutex))
			err(1, "client-handler: pthread_mutex_unlock (rmutex)");
		if (verbose > 1)
			printf("client-handler:rmutex unlocked\n");

		if (pthread_mutex_unlock(&t->mutex))
			err(1, "client-handler: pthread_mutex unlock (mutex)");
		if (verbose > 1)
			printf("client-handler: mutex unlocked\n");

		free(buf);
		if (t->reply && !terminate)
			trxd_writeln(fd, t->reply);

		/* Check if we changed the transceiver */
		if (t->new_tag != t)
			t = t->new_tag;
	}
	close(fd);
	free(arg);
	return NULL;
}
