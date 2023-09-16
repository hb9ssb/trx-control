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
	char buf[128], out[128], *p;
	const char *command, *param;
	t = command_tag;

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

		command = buf;
		param = NULL;
		p = strchr(buf, ' ');
		if (p) {
			*p++ = '\0';
			while (*p == ' ')
				p++;
			if (*p)
				param = p;
		}

		if (!strcmp(command, "use")) {
			command_tag_t *n;

			for (n = command_tag; n; n = n->next) {
				if (!strcmp(n->name, param)) {
					t = n;
					break;
				}
			}
			if (n != NULL) {
				snprintf(out, sizeof(out), "using trx '%s'\n",
				    t->name);
				write(fd, out, strlen(out));
			} else {
				snprintf(out, sizeof(out), "no trx '%s'\n",
				    param);
				write(fd, out, strlen(out));
			}
		} else if (!strcmp(command, "list-trx")) {
			command_tag_t *n;
			snprintf(out, sizeof(out), "list-trx-start\n");
			write(fd, out, strlen(out));

			for (n = command_tag; n; n = n->next) {
				snprintf(out, sizeof(out), "%s\n", n->name);
				write(fd, out, strlen(out));
			}
			snprintf(out, sizeof(out), "list-trx-end\n");
			write(fd, out, strlen(out));
		} else {
			pthread_mutex_lock(&t->mutex);
			pthread_mutex_lock(&t->rmutex);

			t->command = command;
			t->param = param;
			t->client_fd = fd;

			pthread_cond_signal(&t->cond);
			pthread_mutex_unlock(&t->mutex);


			pthread_cond_wait(&command_tag->rcond, &t->rmutex);

			write(fd, command_tag->reply, strlen(t->reply));
			buf[0] = 0x0a;
			buf[1] = 0x0d;
			write(fd, buf, 2);

			pthread_mutex_unlock(&t->rmutex);
		}
	} while (nread > 0);
	close(fd);
	free(arg);
	return NULL;
}
