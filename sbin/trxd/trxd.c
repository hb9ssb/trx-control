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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
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
#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"

#define MAXLISTEN	16

#define BIND_ADDR	"localhost"
#define LISTEN_PORT	"14285"

int verbose = 0;
int log_connections = 0;

extern int luaopen_yaml(lua_State *);
extern int luaopen_trxd(lua_State *);
extern int luaopen_json(lua_State *);

extern void proxy_map(lua_State *, lua_State *, int);
extern void *nmea_handler(void *);
extern void *socket_handler(void *);
extern void *trx_controller(void *);
extern void *gpio_controller(void *);
extern void *relay_controller(void *);
extern void *websocket_listener(void *);
extern void *extension(void *);

extern int trx_control_running;

destination_t *destination = NULL;

static void
usage(void)
{
	(void)fprintf(stderr, "usage: trxd [-adlvV] [-b address] [-c path] "
	    "[-g group] [-p port] [-u user] [-P path]\n");
	exit(1);
}

int
add_destination(const char *name, enum DestinationType type, void *arg)
{
	destination_t *d;

	/* Destination names must be unique */
	for (d = destination; d != NULL; d = d->next)
		if (d->name == name)
			return -1;

	d = malloc(sizeof(destination_t));
	if (d == NULL)
		err(1, "malloc");

	d->next = NULL;
	d->name = strdup(name);
	d->type = type;
	switch (type) {
	case DEST_TRX:
		d->tag.trx = arg;
		break;
	case DEST_RELAY:
		d->tag.relay = arg;
		break;
	case DEST_GPIO:
		d->tag.gpio = arg;
		break;
	case DEST_EXTENSION:
		d->tag.extension = arg;
		break;
	}


	if (destination == NULL)
		destination = d;
	else {
		destination_t *n;
		n = destination;
		while (n->next != NULL)
			n = n->next;
		n->next = d;
	}
	return 0;
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
	int fd, listen_fd[MAXLISTEN], i, ch, noannounce = 0, nodaemon = 0;
	int error, val, top;
	const char *bind_addr, *listen_port, *user, *group, *homedir, *pidfile;
	const char *cfg_file;

	bind_addr = listen_port = user = group = pidfile = cfg_file = NULL;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "config-file",	required_argument, 0, 'c' },
			{ "no-daemon",		no_argument, 0, 'd' },
			{ "no-announce",	no_argument, 0, 'a' },
			{ "log-connections",	no_argument, 0, 'l' },
			{ "group",		required_argument, 0, 'g' },
			{ "bind-address",	required_argument, 0, 'b' },
			{ "listen-port",	required_argument, 0, 'l' },
			{ "pid-file",		required_argument, 0, 'P' },
			{ "user",		required_argument, 0, 'u' },
			{ "verbose",		no_argument, 0, 'v' },
			{ "version",		no_argument, 0, 'V' },
			{ 0, 0, 0, 0 }
		};

		ch = getopt_long(argc, argv, "ac:dlb:g:p:u:vVP:", long_options,
		    &option_index);

		if (ch == -1)
			break;

		switch (ch) {
		case 0:
			break;
		case 'a':
			noannounce = 1;
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
			log_connections = 1;
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
		case 'v':
			verbose++;
			break;
		case 'V':
			printf("trxd %s\n", TRXD_VERSION);
			exit(0);
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

	/* Read the configuration file and extract parameters */
	if (!stat(cfg_file, &sb)) {
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
			if (log_connections == 0) {
				lua_getfield(L, -1, "log-connections");
				log_connections = lua_toboolean(L, -1);
				lua_pop(L, 1);
			}
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			errx(1, "%s: %s", cfg_file, lua_tostring(L, -1));
			break;
		}
	} else {
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

	/*
	 * If trxd(8) is started by the root user, weh change the user id to
	 * an unprivileged user.
	 */
	if (uid == 0) {
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

	/* Setup the trx-controllers */
	lua_getfield(L, -1, "trx");
	if (lua_istable(L, -1)) {
		top = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, top)) {
			trx_controller_tag_t *t;

			t = malloc(sizeof(trx_controller_tag_t));
			t->name = strdup(lua_tostring(L, -2));
			t->handler = t->response = NULL;
			t->is_running = 0;
			t->speed = 9600;
			t->channel = 0;
			t->poller_required = 0;
			t->poller_running = 0;
			t->handler_running = 0;
			t->senders = NULL;

			lua_getfield(L, -1, "device");
			if (!lua_isstring(L, -1))
				errx(1, "missing trx device path");
			t->device = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			lua_getfield(L, -1, "speed");
			if (lua_isinteger(L, -1))
				t->speed =lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "channel");
			if (lua_isinteger(L, -1))
				t->channel =lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "driver");
			if (!lua_isstring(L, -1))
				errx(1, "missing trx driver name");
			t->driver = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			lua_getfield(L, -1, "default");
			t->is_default = lua_toboolean(L, -1);
			lua_pop(L, 1);

			if (add_destination(t->name, DEST_TRX, t))
				errx(1, "names must be unique");

			if (pthread_mutex_init(&t->mutex, NULL))
				goto terminate;

			if (pthread_mutex_init(&t->mutex2, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond1, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond2, NULL))
				goto terminate;

			/* Create the trx-controller thread */
			pthread_create(&t->trx_controller, NULL, trx_controller,
			    t);
			lua_pop(L, 1);
		}
	} else if (verbose)
		printf("trxd: no trx defined\n");
	lua_pop(L, 1);

	/* Setup the gpio-controllers */
	lua_getfield(L, -1, "gpio");
	if (lua_istable(L, -1)) {
		top = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, top)) {
			gpio_controller_tag_t *t;

			t = malloc(sizeof(gpio_controller_tag_t));
			t->name = strdup(lua_tostring(L, -2));
			t->handler = t->response = NULL;
			t->is_running = 0;
			t->speed = 9600;
			t->poller_required = 0;
			t->poller_running = 0;
			t->senders = NULL;

			lua_getfield(L, -1, "device");
			if (!lua_isstring(L, -1))
				errx(1, "missing gpio device path");
			t->device = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			lua_getfield(L, -1, "speed");
			if (lua_isinteger(L, -1))
				t->speed =lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "driver");
			if (!lua_isstring(L, -1))
				errx(1, "missing gpio driver name");
			t->driver = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			if (add_destination(t->name, DEST_GPIO, t))
				errx(1, "names must be unique");

			if (pthread_mutex_init(&t->mutex, NULL))
				goto terminate;

			if (pthread_mutex_init(&t->mutex2, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond1, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond2, NULL))
				goto terminate;

			/* Create the gpio-controller thread */
			pthread_create(&t->gpio_controller, NULL,
			    gpio_controller, t);
			lua_pop(L, 1);
		}
	} else if (verbose)
		printf("trxd: no gpio defined\n");
	lua_pop(L, 1);

	/* Setup the relay-controllers */
	lua_getfield(L, -1, "relays");
	if (lua_istable(L, -1)) {
		top = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, top)) {
			relay_controller_tag_t *t;

			t = malloc(sizeof(relay_controller_tag_t));
			t->name = strdup(lua_tostring(L, -2));
			t->handler = t->response = NULL;
			t->is_running = 0;
			t->poller_running = 0;

			lua_getfield(L, -1, "driver");
			if (!lua_isstring(L, -1))
				errx(1, "missing relay driver name");

			t->driver = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			if (add_destination(t->name, DEST_RELAY, t))
				errx(1, "names must be unique");

			if (pthread_mutex_init(&t->mutex, NULL))
				goto terminate;

			if (pthread_mutex_init(&t->mutex2, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond1, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond2, NULL))
				goto terminate;

			/* Create the relay-controller thread */
			pthread_create(&t->relay_controller, NULL, relay_controller, t);
			lua_pop(L, 1);
		}
	} else if (verbose)
		printf("trxd: no relays defined\n");
	lua_pop(L, 1);

	/* Setup the extensions */
	lua_getfield(L, -1, "extensions");
	if (lua_istable(L, -1)) {
		top = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, top)) {
			extension_tag_t *t;
			const char *p;
			char script[PATH_MAX], *name;

			t = malloc(sizeof(extension_tag_t));
			if (t == NULL)
				err(1, "malloc");
			t->has_config = 0;
			t->listeners = NULL;
			t->L = luaL_newstate();
			if (t->L == NULL)
				err(1, "luaL_newstate");

			luaL_openlibs(t->L);
			luaopen_trxd(t->L);
			lua_setglobal(t->L, "trxd");

			luaL_openlibs(t->L);
			luaopen_json(t->L);
			lua_setglobal(t->L, "json");

			t->call = t->done = 0;
			name = (char *)lua_tostring(L, -2);

			lua_getglobal(t->L, "package");
			lua_getfield(t->L, -1, "cpath");
			lua_pushstring(t->L, ";");
			lua_pushstring(t->L, _PATH_LUA_CPATH);
			lua_concat(t->L, 3);
			lua_setfield(t->L, -2, "cpath");
			lua_pop(t->L, 1);

			lua_getglobal(t->L, "package");
			lua_getfield(t->L, -1, "path");
			lua_pushstring(t->L, ";");
			lua_pushstring(t->L, _PATH_LUA_PATH);
			lua_concat(t->L, 3);
			lua_setfield(t->L, -2, "path");
			lua_pop(t->L, 1);

			lua_getfield(L, -1, "path");
			if (lua_isstring(L, -1)) {
				p = lua_tostring(L, -1);
				lua_getglobal(t->L, "package");
				lua_getfield(t->L, -1, "path");
				lua_pushstring(t->L, ";");
				lua_pushstring(t->L, p);
				lua_concat(t->L, 3);
				lua_setfield(t->L, -2, "path");
				lua_pop(t->L, 1);
			}
			lua_pop(L, 1);

			lua_getfield(L, -1, "cpath");
			if (lua_isstring(L, -1)) {
				p = lua_tostring(L, -1);
				lua_getglobal(t->L, "package");
				lua_getfield(t->L, -1, "cpath");
				lua_pushstring(t->L, ";");
				lua_pushstring(t->L, p);
				lua_concat(t->L, 3);
				lua_setfield(t->L, -2, "cpath");
				lua_pop(t->L, 1);
			}
			lua_pop(L, 1);

			lua_getfield(L, -1, "script");
			if (!lua_isstring(L, -1))
				errx(1, "missing extension script name");

			p = lua_tostring(L, -1);

			if (strchr(p, '/'))
				err(1, "script name must not contain slashes");

			snprintf(script, sizeof(script), "%s/%s.lua",
			    _PATH_EXTENSION, p);

			lua_pop(L, 1);

			if (luaL_loadfile(t->L, script))
				err(1, "%s", lua_tostring(t->L, -1));

			lua_getfield(L, -1, "configuration");
			if (lua_istable(L, -1)) {
				proxy_map(L, t->L, lua_gettop(t->L));
				t->has_config = 1;
			}
			lua_pop(L, 1);

			lua_getfield(L, -1, "callable");
			if (lua_isboolean(L, -1))
				t->is_callable = lua_toboolean(L, -1);
			else
				t->is_callable = 1;
			lua_pop(L, 1);

			if (add_destination(name, DEST_EXTENSION, t))
				errx(1, "names must be unique");

			if (pthread_mutex_init(&t->mutex, NULL))
				goto terminate;

			if (pthread_mutex_init(&t->mutex2, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond1, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond2, NULL))
				goto terminate;

			/* Create the extension thread */
			pthread_create(&t->extension, NULL, extension, t);
			lua_pop(L, 1);
		}
	} else if (verbose)
		printf("trxd: no extensions defined\n");
	lua_pop(L, 1);

	/* Setup WebSocket listening */
	lua_getfield(L, -1, "websocket");
	if (lua_istable(L, -1)) {
		websocket_listener_t *t;

		t = malloc(sizeof(websocket_listener_t));
		if (t == NULL)
			err(1, "trxd: malloc");
		t->ssl = NULL;
		t->ctx = NULL;
		t->certificate = NULL;
		t->announce = noannounce ? 0 : 1;

		lua_getfield(L, -1, "bind-address");
		if (!lua_isstring(L, -1))
			errx(1, "missing websocket bind-address");
		t->bind_addr = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "listen-port");
		if (!lua_isstring(L, -1))
			errx(1, "missing websocket listen-port");
		t->listen_port = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "path");
		if (!lua_isstring(L, -1))
			errx(1, "missing websocket path");
		t->path = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		if (!noannounce) {
			lua_getfield(L, -1, "announce");
			if (lua_isboolean(L, -1))
				t->announce = lua_toboolean(L, -1);
			lua_pop(L, 1);
		}

		lua_getfield(L, -1, "certificate");
		if (lua_isstring(L, -1))
			t->certificate = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		/* Create the websocket-listener thread */
		pthread_create(&t->listener, NULL, websocket_listener, t);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	/* Setup NMEA listening */
	lua_getfield(L, -1, "nmea");
	if (lua_istable(L, -1)) {
		nmea_tag_t *t;
		struct termios tty;
		const char *device;
		int speed = 9600;

		t = malloc(sizeof(nmea_tag_t));
		if (t == NULL)
			err(1, "trxd: malloc");

		lua_getfield(L, -1, "device");
		if (!lua_isstring(L, -1))
			errx(1, "missing nmea device");
		device = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "speed");
		if (lua_isinteger(L, -1))
			speed = lua_tointeger(L, -1);
		lua_pop(L, 1);


		t->fd = open(device, O_RDWR);
		if (t->fd == -1)
			err(1, "trxd: open");

		if (isatty(t->fd)) {
			if (tcgetattr(t->fd, &tty) < 0)
				err(1, "nmea-handler: tcgetattr");
			else {
				cfmakeraw(&tty);
				tty.c_cflag |= CLOCAL;
				cfsetspeed(&tty, speed);

				if (tcsetattr(t->fd, TCSADRAIN, &tty) < 0)
					err(1, "nmea-handler: tcsetattr");
			}
		}

		/* Create the nmea-handler thread */
		pthread_create(&t->nmea_handler, NULL, nmea_handler, t);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	/* Setup network listening */
	for (i = 0; i < MAXLISTEN; i++)
		listen_fd[i] = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(bind_addr, listen_port, &hints, &res0);
	if (error) {
		syslog(LOG_ERR, "getaddrinfo: %s:%s: %s",
		    bind_addr, listen_port, gai_strerror(error));
		exit(1);
	}

	/* The Lua state is no longer needed below this point */
	lua_close(L);

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
			int			 *client_fd, error;
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
			error = getnameinfo((struct sockaddr *)&sa, len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
			if (error)
				syslog(LOG_ERR, "getnameinfo: %s",
				    gai_strerror(error));
			if (log_connections)
				syslog(LOG_INFO, "socket connection from %s",
				    hbuf);

			pthread_create(&thread, NULL, socket_handler,
			    client_fd);
		}
	}
terminate:
	closelog();
	return 0;
}
