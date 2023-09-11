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

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "pathnames.h"
#include "trxd.h"

#define MAXLISTEN	16

#define BIND_ADDR	"localhost"
#define LISTEN_PORT	"14285"

extern void *client_handler(void *);
extern void *trx_control(void *);

extern int trx_control_running;

static void
usage(void)
{
	(void)fprintf(stderr, "usage: trxd <device> <trx-type>\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int fd;
	struct addrinfo hints, *res, *res0;
	pthread_t trx_control_thread, thread;
	controller_t controller;
	int listen_fd[MAXLISTEN], i, ch, nodaemon = 0, log = 0, err, val;
	char *bind_addr, *listen_port;
	char *pidfile = NULL;

	bind_addr = listen_port = NULL;

	while ((ch = getopt(argc, argv, "dlb:p:P:")) != -1) {
		switch (ch) {
		case 'd':
			nodaemon = 1;
			break;
		case 'l':
			log = 1;
			break;
		case 'b':
			bind_addr = optarg;
			break;
		case 'p':
			listen_port = optarg;
			break;
		case 'P':
			pidfile = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();


	if (bind_addr == NULL)
		bind_addr = BIND_ADDR;
	if (listen_port == NULL)
		listen_port = LISTEN_PORT;

	openlog("trxd", nodaemon == 1 ?
		LOG_PERROR | LOG_CONS | LOG_PID | LOG_NDELAY
		: LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);

	if (!nodaemon) {
		if (daemon(0, 0))
			syslog(LOG_ERR, "cannot daemonize: %s",
			    strerror(errno));
		if (pidfile != NULL) {
			FILE *fp;

			fp = fopen(pidfile, "w");
			if (fp != NULL) {
				fprintf(fp, "%ld\n", (long)getpid());
				fclose(fp);
			}
		}
	}

	/* Create the trx-control thread */
	controller.device = argv[0];
	controller.trx_type = argv[1];
	pthread_create(&trx_control_thread, NULL, trx_control, &controller);

	/* Setup network listening */
	for (i = 0; i < MAXLISTEN; i++)
		listen_fd[i] = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	err = getaddrinfo(bind_addr, listen_port, &hints, &res0);
	if (err) {
		syslog(LOG_ERR, "getaddrinfo: %s:%s: %s",
		    bind_addr, listen_port, gai_strerror(err));
		exit(1);
	}
	i = 0;
	for (res = res0; res != NULL && i < MAXLISTEN;
	    res = res->ai_next) {
		listen_fd[i] = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (listen_fd[i] < 0)
			continue;
		if (fcntl(listen_fd[i], F_SETFL, fcntl(listen_fd[i],
		    F_GETFL) | O_NONBLOCK)) {
			syslog(LOG_ERR, "fcntl: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		val = 1;
		if (setsockopt(listen_fd[i], SOL_SOCKET, SO_REUSEADDR,
		    (const char *)&val,
		    sizeof(val))) {
			syslog(LOG_ERR, "setsockopt: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		if (bind(listen_fd[i], res->ai_addr,
		    res->ai_addrlen)) {
			syslog(LOG_ERR, "bind: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		if (listen(listen_fd[i], 5)) {
			syslog(LOG_ERR, "listen: %s", strerror(errno));
			close(listen_fd[i]);
			continue;
		}
		i++;
	}

	/* Wait for connections as long as rig_control runs */
	while (trx_control_running) {
		struct timeval	 tv;
		fd_set		 readfds;
		int		 maxfd = -1;
		int		 r;

		FD_ZERO(&readfds);
		for (i = 0; i < MAXLISTEN; ++i) {
			if (listen_fd[i] != -1) {
				FD_SET(listen_fd[i], &readfds);
				if (listen_fd[i] > maxfd)
					maxfd = listen_fd[i];
			}
		}
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		r = select(maxfd + 1, &readfds, NULL, NULL, &tv);
		if (r < 0) {
			if (errno != EINTR) {
				syslog(LOG_ERR, "select: %s", strerror(errno));
				break;
			}
		} else if (r == 0)
			continue;

		for (i = 0; i < MAXLISTEN; ++i) {
			struct sockaddr_storage	 sa;
			socklen_t		 len;
			int			 client_fd, err;
			char			 hbuf[NI_MAXHOST];

			if (listen_fd[i] == -1 ||
			    !FD_ISSET(listen_fd[i], &readfds))
				continue;
			memset(&sa, 0, sizeof(sa));
			len = sizeof(sa);
			client_fd = accept(listen_fd[i],
			    (struct sockaddr *)&sa, &len);
			if (client_fd < 0) {
				syslog(LOG_ERR, "accept: %s", strerror(errno));
				break;
			}
			err = getnameinfo((struct sockaddr *)&sa, len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
			if (err)
				syslog(LOG_ERR, "getnameinfo: %s",
				    gai_strerror(err));
			if (log)
				syslog(LOG_INFO, "connection from %s", hbuf);
			if (fcntl(client_fd, F_SETFL,
			    fcntl(client_fd, F_GETFL) | O_NONBLOCK)) {
				syslog(LOG_ERR, "fcntl: %s", strerror(errno));
				close(client_fd);
				break;
			}
			pthread_create(&thread, NULL, client_handler,
			    &client_fd);
		}
	}
	pthread_join(trx_control_thread, NULL);

	closelog();
	printf("trxd terminates\n");
	return 0;
}
