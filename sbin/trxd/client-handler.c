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
#include <unistd.h>

#include "trxd.h"

extern command_tag_t *command_tag;

void *
client_handler(void *arg)
{
	int fd = *(int *)arg;
	int status, nread, n;
	char buf[128], *p;

	status = pthread_detach(pthread_self());
	if (status)
		err(1, "can't detach");

	do {
		nread = read(fd, buf, sizeof(buf));
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

		command_tag->command = buf;
		command_tag->param = NULL;
		p = strchr(buf, ' ');
		if (p) {
			*p++ = '\0';
			while (*p == ' ')
				p++;
			if (*p)
				command_tag->param = p;
		}
		command_tag->client_fd = fd;

		pthread_cond_signal(&command_tag->cond);
		pthread_mutex_unlock(&command_tag->mutex);


		pthread_cond_wait(&command_tag->rcond, &command_tag->rmutex);

		write(fd, command_tag->reply, strlen(command_tag->reply));
		buf[0] = 0x0a;
		buf[1] = 0x0d;
		write(fd, buf, 2);

		pthread_mutex_unlock(&command_tag->rmutex);

	} while (nread > 0);
	close(fd);
	return NULL;
}