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

/* Send data to networked clients over WebSockets */

#include <sys/socket.h>

#include <openssl/ssl.h>

#include <assert.h>
#include <err.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"
#include "websocket.h"

extern int verbose;

#define BUFSIZE		65535

static void
cleanup(void *arg)
{
	if (verbose > 1)
		printf("websocket-sender: cleanup\n");
	free(arg);
}

void *
websocket_sender(void *arg)
{
	sender_tag_t *s = (sender_tag_t *)arg;
	int status, nread, n, terminate;
	char *buf, *p;
	const char *command, *param;
	size_t datasize, framesize;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_detach(pthread_self()))
		err(1, "websocket-sender: pthread_detach");

	if (pthread_setname_np(pthread_self(), "wsock-sender"))
		err(1, "websocket-sender: pthread_setname_np");

	if (pthread_mutex_lock(&s->mutex))
		err(1, "websocket-sender: pthread_mutex_lock");
	if (verbose > 1)
		printf("websocket-sender: mutex locked\n");

	for (terminate = 0; !terminate ;) {
		if (verbose > 1)
			printf("websocket-sender: wait for cond\n");
		while (s->data == NULL) {
			if (pthread_cond_wait(&s->cond, &s->mutex))
				err(1, "websocket-sender: pthread_cond_wait");
			if (verbose > 1)
				printf("websocket-sender: cond changed\n");
		}

		if (!terminate) {
			if (verbose)
				printf("websocket-sender: -> %s\n", s->data);
			datasize = strlen(s->data);
			buf = malloc(BUFSIZE);
			framesize = datasize;
			if (buf == NULL)
				err(1, "websocket-sender: malloc\n");

			wsMakeFrame((const uint8_t *)s->data, datasize,
			    (unsigned char *)buf, &framesize, WS_TEXT_FRAME);

			if (s->ssl)
				SSL_write(s->ssl, buf, framesize);
			else
				send(s->socket, buf, framesize, 0);
			free(buf);
			s->data = NULL;
		}
	}
	pthread_cleanup_pop(0);
	return NULL;
}
