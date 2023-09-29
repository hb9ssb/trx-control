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

#include <sys/epoll.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "trxd.h"

#define MAX_EVENTS	2
void *
trx_handler(void *arg)
{
	command_tag_t *command_tag = (command_tag_t *)arg;
	struct epoll_event ev[MAX_EVENTS], events[MAX_EVENTS];
	int status, nread, n, r, fd, epfd, nfds, terminate;
	char buf[128], *p;
	struct timeval tv;
	sigset_t mask;

	if (pthread_detach(pthread_self()))
		err(1, "trx-handler: pthread_detach failed");

	fd = command_tag->cat_device;

	epfd = epoll_create(1);
	if (epfd == -1)
		err(1, "trx-handler: epoll_create failed");

	ev[0].events = EPOLLIN;
	ev[0].data.fd = fd;
	ev[1].events = EPOLLIN;
	ev[1].data.fd = command_tag->handler_pipefd[0];

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev[0]) == -1)
		err(1, "epoll_ctl");
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, command_tag->handler_pipefd[0],
	    &ev[1]) == -1)
		err(1, "epoll_ctl");

	for (terminate = 0; terminate == 0; ) {
		nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			else
				err(1, "trx-handler: epoll_wait failed");
		}
		for (n = 0; n < nfds; n++) {
			if (events[n].data.fd == fd) {

				for (n = 0; n < sizeof(buf) - 1; n++) {
					read(fd, &buf[n], 1);
					if (buf[n] == command_tag->handler_eol)
						break;
				}
				buf[++n] = '\0';

				pthread_mutex_lock(&command_tag->mutex);
				pthread_mutex_lock(&command_tag->rmutex);

				command_tag->handler = "dataHandler";
				command_tag->data = buf;
				command_tag->client_fd = 0;

				pthread_cond_signal(&command_tag->qcond);

				pthread_cond_wait(&command_tag->rcond,
				    &command_tag->rmutex);
				pthread_mutex_unlock(&command_tag->rmutex);
				pthread_mutex_unlock(&command_tag->mutex);

			} else
				terminate = 1;
		}
	};
	return NULL;
}
