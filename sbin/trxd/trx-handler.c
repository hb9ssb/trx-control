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
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "trxd.h"

extern int verbose;

void *
trx_handler(void *arg)
{
	command_tag_t *command_tag = (command_tag_t *)arg;
	struct pollfd pfds[2];
	int status, nread, n, r, fd, epfd, nfds, terminate;
	char buf[128], *p;
	struct timeval tv;

	if (pthread_detach(pthread_self()))
		err(1, "trx-handler: pthread_detach");

	fd = command_tag->cat_device;

	pfds[0].fd = fd;
	pfds[0].events = POLLIN;
	pfds[1].fd = command_tag->handler_pipefd[0];
	pfds[1].events = POLLIN;

	for (terminate = 0; terminate == 0; ) {
		if (pthread_mutex_lock(&command_tag->mutex))
			err(1, "trx-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("trx-handler: mutex locked\n");

		if (poll(pfds, 2, 0) == -1)
			err(1, "trx-handler: poll");
		if (pfds[0].revents) {
			for (n = 0; n < sizeof(buf) - 1; n++) {
				read(fd, &buf[n], 1);
				if (buf[n] == command_tag->handler_eol)
					break;
			}
			buf[++n] = '\0';

			command_tag->handler = "dataHandler";
			command_tag->data = buf;
			command_tag->client_fd = 0;

			if (pthread_cond_signal(&command_tag->cond))
				err(1, "trx-handler: pthread_cond_signal");
			if (verbose > 1)
				printf("trx-handler: cond signaled\n");
			if (pthread_mutex_unlock(&command_tag->mutex))
				err(1, "trx-handler: pthread_mutex_lock");

			if (pthread_cond_wait(&command_tag->cond,
			    &command_tag->mutex))
				err(1, "trx-handler: pthread_cond_wait");
		}
		if (pfds[1].revents)
			terminate = 1;

		if (pthread_mutex_unlock(&command_tag->mutex))
			err(1, "trx-handler: pthread_mutex_lock");
		if (verbose > 1)
			printf("trx-handler: mutex unlocked\n");
	};
	return NULL;
}
