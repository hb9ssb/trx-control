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

extern trx_controller_tag_t *trx_controller_tag;
extern int verbose;

void *
socket_handler(void *arg)
{
	trx_controller_tag_t *t;
	int fd = *(int *)arg;
	int status, nread, n, terminate;
	char *buf, *p;
	const char *command, *param;

	/* Check if have a default transceiver */
	for (t = trx_controller_tag; t != NULL; t = t->next)
		if (t->is_default)
			break;

	/* If there is no default transceiver, use the firs one */
	if (t == NULL)
		t = trx_controller_tag;

	if (pthread_detach(pthread_self()))
		err(1, "socket-handler: pthread_detach");

	for (terminate = 0; !terminate ;) {
		buf = trxd_readln(fd);

		if (buf == NULL) {
			terminate = 1;
			buf = strdup("{\"request\": \"stop-status-updates\"}");
		}

		if (pthread_mutex_lock(&t->mutex))
			err(1, "socket-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("socket-handler: mutex locked\n");

		t->handler = "requestHandler";
		t->reply = NULL;
		t->data = buf;
		t->client_fd = fd;
		t->new_tag = t;

		if (pthread_mutex_lock(&t->mutex2))
			err(1, "socket-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("socket-handler: mutex2 locked\n");

		/* We signal cond, and mutex gets owned by trx-controller */
		if (pthread_cond_signal(&t->cond1))
			err(1, "socket-handler: pthread_cond_signal");
		if (verbose > 1)
			printf("socket-handler: cond1 signaled\n");

		if (pthread_mutex_unlock(&t->mutex2))
			err(1, "socket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("socket-handler: mutex unlocked\n");

		while (t->reply == NULL) {
			if (pthread_cond_wait(&t->cond2, &t->mutex2))
				err(1, "socket-handler: pthread_cond_wait");
			if (verbose > 1)
				printf("socket-handler: cond2 changed\n");
		}

		free(buf);
		if (strlen(t->reply) > 0 && !terminate)
			trxd_writeln(fd, t->reply);

		/* Check if we changed the transceiver */
		if (t->new_tag != t)
			t = t->new_tag;

		if (pthread_mutex_unlock(&t->mutex2))
			err(1, "socket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("socket-handler: mutex2 unlocked\n");

		if (pthread_mutex_unlock(&t->mutex))
			err(1, "socket-handler: pthread_mutex_unlock");
		if (verbose > 1)
			printf("socket-handler: mutex unlocked\n");
	}
	close(fd);
	free(arg);
	return NULL;
}
