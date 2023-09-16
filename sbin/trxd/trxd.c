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
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <netdb.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"

#define MAXLISTEN	16

#define BIND_ADDR	"localhost"
#define LISTEN_PORT	"14285"

extern int luaopen_yaml(lua_State *);
extern int luaopen_trxd(lua_State *);

extern void *client_handler(void *);
extern void *trx_control(void *);

extern int trx_control_running;

command_tag_t *command_tag = NULL;

static void
usage(void)
{
	(void)fprintf(stderr, "usage: trxd [-dl] [-b address] [-c config-file] "
	    "[-g group] [-p port] [-u user] [-P path]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct passwd *passwd;
	struct group *grp;
	uid_t uid;
	gid_t gid;
	struct stat sb;
	struct addrinfo hints, *res, *res0;
	lua_State *L;
	pthread_t trx_control_thread, thread;
	int fd, listen_fd[MAXLISTEN], i, ch, nodaemon = 0, log = 0, err, val;
	int top;
	const char *bind_addr, *listen_port, *user, *group, *homedir, *pidfile;
	const char *cfg_file;

	bind_addr = listen_port = user = group = pidfile = cfg_file = NULL;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "config-file",	required_argument, 0, 'c' },
			{ "no-daemon",		no_argument, 0, 'd' },
			{ "log-connections",	no_argument, 0, 'l' },
			{ "group",		required_argument, 0, 'g' },
			{ "bind-address",	required_argument, 0, 'b' },
			{ "listen-port",	required_argument, 0, 'l' },
			{ "pid-file",		required_argument, 0, 'P' },
			{ "user",		required_argument, 0, 'u' },
			{ 0, 0, 0, 0 }
		};

		ch = getopt_long(argc, argv, "c:dlb:g:p:u:P:", long_options,
		    &option_index);

		if (ch == -1)
			break;

		switch (ch) {
		case 0:
			break;
		case 'c':
			cfg_file = optarg;
			break;
		case 'd':
			nodaemon = 1;
			break;
		case 'g':
			group = optarg;
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
		case 'u':
			user = optarg;
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

	if (argc)
		usage();

	if (cfg_file == NULL)
		cfg_file = _PATH_CFG;

	openlog("trxd", nodaemon == 1 ?
		LOG_PERROR | LOG_CONS | LOG_PID | LOG_NDELAY
		: LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL) {
		syslog(LOG_ERR, "cannot initialize Lua state");
		goto terminate;
	}
	luaL_openlibs(L);

	/* Provide the yaml and trxd modules as if they were part of std Lua */
	luaopen_yaml(L);
	lua_setglobal(L, "yaml");
	luaopen_trxd(L);
	lua_setglobal(L, "trxd");

	/* Read the configuration file an extract parameters */
	if (!stat(cfg_file, &sb)) {
		printf("parse config file %s\n", cfg_file);

		lua_getglobal(L, "yaml");
		lua_getfield(L, -1, "parsefile");
		lua_pushstring(L, cfg_file);

		switch (lua_pcall(L, 1, 1, 0)) {
		case LUA_OK:
			if (lua_type(L, -1) != LUA_TTABLE) {
				syslog(LOG_ERR, "invalid configuration file");
				lua_close(L);
				exit(1);
			}
			if (bind_addr == NULL) {
				lua_getfield(L, -1, "bind-address");
				bind_addr = lua_tostring(L, -1);
				lua_pop(L, 1);
			}
			if (listen_port == NULL) {
				lua_getfield(L, -1, "listen-port");
				listen_port = lua_tostring(L, -1);
				lua_pop(L, 1);
			}
			if (user == NULL) {
				lua_getfield(L, -1, "user");
				user = lua_tostring(L, -1);
				lua_pop(L, 1);
			}
			if (group == NULL) {
				lua_getfield(L, -1, "group");
				group = lua_tostring(L, -1);
				lua_pop(L, 1);
			}
			if (pidfile == NULL) {
				lua_getfield(L, -1, "pid-file");
				pidfile = lua_tostring(L, -1);
				lua_pop(L, 1);
			}
			if (nodaemon == 0) {
				lua_getfield(L, -1, "no-daemon");
				nodaemon = lua_toboolean(L, -1);
				lua_pop(L, 1);
			}
			if (log == 0) {
				lua_getfield(L, -1, "log-connections");
				log = lua_toboolean(L, -1);
				lua_pop(L, 1);
			}
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			syslog(LOG_ERR, "Configuration error: %s",
			    lua_tostring(L, -1));
			break;
		}
	} else if (strcmp(cfg_file, _PATH_CFG)) {
		syslog(LOG_ERR, "configuration file '%s' not accessible",
		    cfg_file);
		lua_close(L);
		exit(1);
	}

	/*
	 * Use default values for required parameters that are neither set on
	 * the command line nor in the configuration file.
	 */
	if (bind_addr == NULL)
		bind_addr = BIND_ADDR;
	if (listen_port == NULL)
		listen_port = LISTEN_PORT;
	if (user == NULL)
		user = TRXD_USER;
	if (group == NULL)
		group = TRXD_GROUP;

	uid = getuid();
	gid = getgid();

	if ((passwd = getpwnam(user)) != NULL) {
		uid = passwd->pw_uid;
		gid = passwd->pw_gid;
		homedir = passwd->pw_dir;
	} else {
		syslog(LOG_ERR, "no such user '%s'", user);
		exit(1);
	}

	if (group != NULL) {
		if ((grp = getgrnam(group)) != NULL)
			gid = grp->gr_gid;
		else {
			syslog(LOG_ERR, "no such group '%s'", group);
			exit(1);
		}
	}

	if (chdir(homedir)) {
		syslog(LOG_ERR, "can't chdir to %s", homedir);
		exit(1);
	}

	if (setgid(gid)) {
		syslog(LOG_ERR, "can't set group");
		exit(1);
	}
	if (setuid(uid)) {
		syslog(LOG_ERR, "can't set user id");
		exit(1);
	}

	if (!getuid() || !getgid()) {
		syslog(LOG_ERR, "must not be run as root, exiting");
		exit(1);
	}

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

	lua_getfield(L, -1, "trx");
	top = lua_gettop(L);
	lua_pushnil(L);
	while (lua_next(L, top)) {
		command_tag_t *t;

		t = malloc(sizeof(command_tag_t));
		t->next = NULL;
		t->is_running = 0;

		lua_getfield(L, -1, "name");
		t->name = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "device");
		t->device = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "driver");
		t->driver = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		if (command_tag == NULL)
			command_tag = t;
		else {
			command_tag_t *n;
			n = command_tag;
			while (n->next != NULL)
				n = n->next;
			n->next = t;
		}

		if (pthread_mutex_init(&t->mutex, NULL))
			goto terminate;

		if (pthread_cond_init(&t->cond, NULL))
			goto terminate;

		if (pthread_mutex_init(&t->rmutex, NULL))
			goto terminate;

		if (pthread_cond_init(&t->rcond, NULL))
			goto terminate;

		if (pthread_mutex_init(&t->ai_mutex, NULL))
			goto terminate;

		/* Create the trx-control thread */
		pthread_create(&t->trx_control, NULL, trx_control, t);
		lua_pop(L, 1);
	}

	/* The Lua state is no longer needed below this point */
	lua_close(L);

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

	/* Wait for connections as long as trx_control runs */
	while (1) {
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
			int			 *client_fd, err;
			char			 hbuf[NI_MAXHOST];

			client_fd = malloc(sizeof(int));

			if (listen_fd[i] == -1 ||
			    !FD_ISSET(listen_fd[i], &readfds))
				continue;
			memset(&sa, 0, sizeof(sa));
			len = sizeof(sa);
			*client_fd = accept(listen_fd[i],
			    (struct sockaddr *)&sa, &len);
			if (*client_fd < 0) {
				syslog(LOG_ERR, "accept: %s", strerror(errno));
				free(client_fd);
				break;
			}
			err = getnameinfo((struct sockaddr *)&sa, len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
			if (err)
				syslog(LOG_ERR, "getnameinfo: %s",
				    gai_strerror(err));
			if (log)
				syslog(LOG_INFO, "connection from %s", hbuf);

			pthread_create(&thread, NULL, client_handler,
			    client_fd);
		}
	}

terminate:
	closelog();
	return 0;
}
