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

#include <sys/select.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "trxd.h"

void *
trx_handler(void *arg)
{
	command_tag_t *command_tag = (command_tag_t *)arg;
	int status, nread, n, r, fd;
	char buf[128], *p;
	fd_set fds;
	struct timeval tv;

	status = pthread_detach(pthread_self());
	if (status)
		err(1, "can't detach");

	fd = command_tag->cat_device;

	do {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		pthread_mutex_lock(&command_tag->ai_mutex);
		r = select(fd + 1, &fds, NULL, NULL, &tv);
		if (r < 0) {
			if (errno != EINTR) {
				syslog(LOG_ERR, "select: %s", strerror(errno));
				break;
			}
		}
		if (r > 0 && FD_ISSET(fd, &fds)) {
			printf("trx data available\n");
			nread = read(fd, buf, sizeof(buf));
		} else {
			printf("no trx data available\n");
		}
		pthread_mutex_unlock(&command_tag->ai_mutex);

		if (nread <= 0)
			break;

		for (n = 0; n < nread; n++) {
			if (buf[n] == 0x0a || buf[n] == 0x0d) {
				buf[n] = '\0';
				break;
			}
		}
		pthread_mutex_lock(&command_tag->mutex);
		pthread_mutex_lock(&command_tag->rmutex);

		command_tag->handler = "dataHandler";
		command_tag->data = buf;
		command_tag->client_fd = 0;

		pthread_cond_signal(&command_tag->cond);
		pthread_mutex_unlock(&command_tag->mutex);

		pthread_cond_wait(&command_tag->rcond, &command_tag->rmutex);
		pthread_mutex_unlock(&command_tag->rmutex);

	} while (nread >= 0);
	close(fd);
	return NULL;
}
