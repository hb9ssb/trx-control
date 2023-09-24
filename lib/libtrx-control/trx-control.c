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

/* trx-control(7) support functions used by trxd(8) and other binaries */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "trx-control.h"

int
connect_trxd(const char *host, const char *port)
{
	struct addrinfo hints, *res, *res0;
	struct linger linger;
	int fd, error;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error) {
		fprintf(stderr, "%s: %s\n", host, gai_strerror(error));
		return -1;
	}
	fd = -1;
	for (res = res0; res; res = res->ai_next) {
		error = getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST |
		    NI_NUMERICSERV);
		if (error)
			continue;
		fd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (fd < 0)
			continue;
		if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
			close(fd);
			fd = -1;
			continue;
		}
		linger.l_onoff = 1;
		linger.l_linger = 10;	/* sec. */
		if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger,
		    sizeof(linger)))
			fprintf(stderr, "setsockopt SO_LINGER failed\n");
		break;
	}
	return fd;
}
