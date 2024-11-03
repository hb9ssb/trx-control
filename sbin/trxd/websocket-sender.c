/*
 * Copyright (c) 2023 - 2024 Marc Balmer HB9SSB
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

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>

#include "trxd.h"
#include "trx-control.h"
#include "websocket.h"

extern int verbose;

#define MAX_WS_HEADER	10

static void
cleanup(void *arg)
{
	free(arg);
}

void *
websocket_sender(void *arg)
{
	sender_tag_t *s = (sender_tag_t *)arg;
	unsigned char *buf;
	size_t datasize, framesize;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "websocket-sender: pthread_detach");
		exit(1);
	}

	if (pthread_setname_np(pthread_self(), "ws-sender")) {
		syslog(LOG_ERR, "websocket-sender: pthread_setname_np");
		exit(1);
	}

	if (pthread_mutex_lock(&s->mutex)) {
		syslog(LOG_ERR, "websocket-sender: pthread_mutex_lock");
		exit(1);
	}

	s->data = NULL;
	if (pthread_cond_signal(&s->cond2)) {
		syslog(LOG_ERR, "websocket-sender: pthread_cond_signal");
		exit(1);
	}

	for (;;) {
		while (s->data == NULL) {
			if (pthread_cond_wait(&s->cond, &s->mutex)) {
				syslog(LOG_ERR, "websocket-sender: pthread_cond_wait");
				exit(1);
			}
		}

		if (verbose)
			printf("websocket-sender: -> %s\n", s->data);
		datasize = strlen(s->data);
		buf = malloc(datasize + MAX_WS_HEADER);
		framesize = datasize;
		if (buf == NULL) {
			syslog(LOG_ERR, "websocket-sender: malloc\n");
			exit(1);
		}

		wsMakeFrame((const uint8_t *)s->data, datasize,
		    (unsigned char *)buf, &framesize, WS_TEXT_FRAME);

		if (s->ssl)
			SSL_write(s->ssl, buf, framesize);
		else
			send(s->socket, buf, framesize, 0);
		free(buf);
		s->data = NULL;
		if (pthread_cond_signal(&s->cond2)) {
			syslog(LOG_ERR, "websocket-sender: pthread_cond_signal");
			exit(1);
		}
	}
	pthread_cleanup_pop(0);
	return NULL;
}
