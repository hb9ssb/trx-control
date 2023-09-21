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

extern command_tag_t *command_tag;

void *
client_handler(void *arg)
{
	command_tag_t *t;
	int fd = *(int *)arg;
	int status, nread, n;
	char buf[256], out[1024], *p;
	const char *command, *param;

	t = command_tag;

	status = pthread_detach(pthread_self());
	if (status)
		err(1, "can't detach");

	do {
		nread = read(fd, buf, sizeof(buf));
		if (nread <= 0)
			break;
		/* don't write past buf if buf is full */
		if (nread == sizeof(buf))
			nread--;
		buf[nread] = '\0';
		pthread_mutex_lock(&t->mutex);
		pthread_mutex_lock(&t->rmutex);

		t->handler = "requestHandler";
		t->data = buf;
		t->client_fd = fd;
		t->new_tag = t;

		pthread_cond_signal(&t->cond);
		pthread_mutex_unlock(&t->mutex);

		pthread_cond_wait(&t->rcond, &t->rmutex);

		snprintf(out, sizeof(out), "%s\0x0a\0x0d", t->reply);
		write(fd, out, strlen(out));

		pthread_mutex_unlock(&t->rmutex);

		/* Check if we changed the transceiver */
		if (t->new_tag != t)
			t = t->new_tag;
	} while (nread > 0);
	close(fd);
	free(arg);
	return NULL;
}
