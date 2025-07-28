/*
 * Copyright (c) 2023 - 2025 Marc Balmer HB9SSB
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
#include <termios.h>
#include <unistd.h>
#include <zmq.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

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
extern int luaopen_trx(lua_State *);
extern int luaopen_trxd(lua_State *);
extern int luaopen_trx_controller(lua_State *);

extern void proxy_map(lua_State *, lua_State *, int);
extern void *nmea_handler(void *);
extern void *sd_event_handler(void *);
extern void *socket_handler(void *);
extern void *trx_controller(void *);
extern void *sdr_controller(void *);
extern void *gpio_controller(void *);
extern void *relay_controller(void *);
extern void *websocket_listener(void *);
extern void *extension(void *);

extern int trx_control_running;

destination_t *destination = NULL;
pthread_mutex_t destination_mutex;

void *zmq_ctx;

/*
 * private,, i.e. per connection extensions configuration.  This is in
 * JSON format, if private extensions have been defined.
 */

char *private_extensions = NULL;

static void
usage(void)
{
#ifdef USE_SDDM
	(void)fprintf(stderr, "usage: trxd [-adlmvV] [-b address] [-c path] "
	    "[-g group] [-p port] [-u user] [-P path]\n");
#else
	(void)fprintf(stderr, "usage: trxd [-adlvV] [-b address] [-c path] "
	    "[-g group] [-p port] [-u user] [-P path]\n");
#endif
	exit(1);
}

int
add_destination(const char *name, enum DestinationType type, void *arg)
{
	destination_t *d;

	if (pthread_mutex_lock(&destination_mutex)) {
		syslog(LOG_ERR, "pthread_mutex_lock");
		exit(1);
	}

	/* Destination names must be unique */
	for (d = destination; d != NULL; d = d->next)
		if (!strcmp(d->name, name)) {
			pthread_mutex_unlock(&destination_mutex);
			return -1;
	}

	d = malloc(sizeof(destination_t));
	if (d == NULL) {
		syslog(LOG_ERR, "memory allocation error");
		exit(1);
	}

	d->next = d->previous = NULL;
	d->name = strdup(name);
	d->type = type;
	switch (type) {
	case DEST_TRX:
		d->tag.trx = arg;
		break;
	case DEST_SDR:
		d->tag.sdr = arg;
		break;
	case DEST_RELAY:
		d->tag.relay = arg;
		break;
	case DEST_GPIO:
		d->tag.gpio = arg;
		break;
	case DEST_INTERNAL:
		if (!strcmp(name, "nmea"))
			d->tag.nmea = arg;
		else {
			syslog(LOG_ERR, "unknown internal name '%s'", name);
			exit(1);
		}
		break;
	case DEST_EXTENSION:
		d->tag.extension = arg;
		break;
	case DEST_ROTOR:
		syslog(LOG_ERR, "rotors are not yet supported");
		exit(1);
	}

	if (destination == NULL)
		destination = d;
	else {
		destination_t *n;
		n = destination;
		while (n->next != NULL)
			n = n->next;
		n->next = d;
		d->previous = n;
	}
	pthread_mutex_unlock(&destination_mutex);
	return 0;
}

int
remove_destination(const char *name)
{
	destination_t *d;
	int rv = -1;

	if (destination == NULL)
		return -1;

	if (pthread_mutex_lock(&destination_mutex)) {
		syslog(LOG_ERR, "pthread_mutex_lock");
		exit(1);
	}

	for (d = destination; d != NULL; d = d->next) {
		if (!strcmp(d->name, name)) {
			rv = 0;
			if (d == destination) {
				d->previous = NULL;
				destination = d->next;
				free(d->name);
				free(d);
				break;
			} else {
				if (d->next)
					d->next->previous = d->previous;
				d->previous->next = d->next;
				free(d->name);
				free(d);
				break;
			}
		}
	}
	pthread_mutex_unlock(&destination_mutex);
	return rv;
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
	pthread_t thread;
	int fd, listen_fd[MAXLISTEN], i, ch, noannounce = 0, nodaemon = 0;
	int error, val, top;
#ifdef USE_SDDM
	pthread_t sd_event_handler_thread;
	int monitor_systemd = 0;
#endif

	const char *bind_addr, *listen_port, *user, *group, *homedir, *pidfile;
	char *cfg_file;

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
#ifdef USE_SDDM
			{ "monitor-systemd",	no_argument, 0, 'm' },
#endif
			{ "pid-file",		required_argument, 0, 'P' },
			{ "user",		required_argument, 0, 'u' },
			{ "verbose",		no_argument, 0, 'v' },
			{ "version",		no_argument, 0, 'V' },
			{ 0, 0, 0, 0 }
		};

#ifdef USE_SDDM
		ch = getopt_long(argc, argv, "ac:dlb:g:mp:u:vVP:", long_options,
		    &option_index);
#else
		ch = getopt_long(argc, argv, "ac:dlb:g:p:u:vVP:", long_options,
		    &option_index);
#endif
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
#ifdef USE_SDDM
		case 'm':
			monitor_systemd = 1;
			break;
#endif
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

	openlog("trxd", nodaemon == 1 ?
		LOG_PERROR | LOG_CONS | LOG_PID | LOG_NDELAY
		: LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);

	if (cfg_file == NULL) {
		cfg_file = _PATH_CFG;
		if (stat(cfg_file, &sb)) {
			if (asprintf(&cfg_file, "%s/.local/%s",
			    getenv("HOME"), _PATH_CFG_LOCAL) == -1) {
				syslog(LOG_ERR,
				    "can not create local config path");
				exit(1);
			}
		}
	}

	if (stat(cfg_file, &sb)) {
		syslog(LOG_ERR, "can't stat config file %s: %s", cfg_file,
		    strerror(errno));
		exit(1);
	}

	if (pthread_mutex_init(&destination_mutex, NULL)) {
		syslog(LOG_ERR, "pthread_mutex_init");
		exit(1);
	}

	/* Setup ZeroMQ */
	if ((zmq_ctx = zmq_ctx_new()) == NULL) {
		syslog(LOG_ERR, "cannot create ZeroMQ context");
		exit(1);
	}

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL) {
		syslog(LOG_ERR, "cannot initialize Lua state");
		goto terminate;
	}
	luaL_openlibs(L);

	/*
	 * Provide the yaml, trxd, and, json modules as if they were part of
	 * std Lua
	 */
	luaopen_yaml(L);
	lua_setglobal(L, "yaml");
	luaopen_trxd(L);
	lua_setglobal(L, "trxd");
	luaopen_json(L);
	lua_setglobal(L, "json");

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
			syslog(LOG_ERR, "%s: %s", cfg_file,
			    lua_tostring(L, -1));
			exit(1);
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
	lua_getfield(L, -1, "transceivers");
	if (lua_istable(L, -1)) {
		top = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, top)) {
			trx_controller_tag_t *t;
			const char *p;
			char trx_path[PATH_MAX];
			const char *protocol = NULL;
			char proto_path[PATH_MAX];

			t = malloc(sizeof(trx_controller_tag_t));
			t->name = strdup(lua_tostring(L, -2));
			t->handler = t->response = NULL;
			t->is_running = 0;
			t->speed = 9600;
			t->channel = 0;
			t->audio_input = t->audio_output = NULL;
			t->poller_required = 0;
			t->poller_running = 0;
			t->handler_running = 0;
			t->senders = NULL;

			lua_getfield(L, -1, "device");
			if (!lua_isstring(L, -1)) {
				syslog(LOG_ERR, "missing trx device path");
				exit(1);
			}
			t->device = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			if (*t->device == '/' && stat(t->device, &sb)) {
				syslog(LOG_WARNING,
				    "trxd: file not found %s\n", t->device);
				free((void *)t->device);
				free(t->name);
				free(t);
				lua_pop(L, 1);
				continue;
			}

			lua_getfield(L, -1, "speed");
			if (lua_isinteger(L, -1))
				t->speed = lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "channel");
			if (lua_isinteger(L, -1))
				t->channel =lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "audio");
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "input");
				if (lua_isstring(L, -1))
					t->audio_input =
					    strdup(lua_tostring(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, -1, "output");
				if (lua_isstring(L, -1))
					t->audio_output =
					    strdup(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
			lua_pop(L, 1);

			lua_getfield(L, -1, "trx");
			if (!lua_isstring(L, -1)) {
				syslog(LOG_ERR, "missing trx name");
				exit(1);
			}
			t->trx = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			lua_getfield(L, -1, "default");
			t->is_default = lua_toboolean(L, -1);
			lua_pop(L, 1);

			/* Setup Lua */
			t->L = luaL_newstate();
			if (t->L == NULL) {
				syslog(LOG_ERR, "cannot create Lua state");
				exit(1);
			}

			luaL_openlibs(t->L);

			luaopen_trx(t->L);
			lua_setglobal(t->L, "trx");
			luaopen_trx_controller(t->L);
			lua_setglobal(t->L, "trxController");
			luaopen_trxd(t->L);
			lua_setglobal(t->L, "trxd");
			luaopen_json(t->L);
			lua_setglobal(t->L, "json");
			luaopen_yaml(t->L);
			lua_setglobal(t->L, "yaml");

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

			/* Load trx description and protocol driver */
			snprintf(trx_path, sizeof(trx_path), "%s/%s.yaml",
			    _PATH_TRX, t->trx);

			if (stat(trx_path, &sb)) {
				syslog(LOG_ERR, "%s: file not found", trx_path);
				exit(1);
			}

			lua_getglobal(t->L, "yaml");
			lua_getfield(t->L, -1, "parsefile");
			lua_pushstring(t->L, trx_path);

			switch (lua_pcall(t->L, 1, 1, 0)) {
			case LUA_OK:
				lua_getfield(t->L, -1, "protocol");
				protocol = lua_tostring(t->L, -1);
				if (protocol == NULL) {
					syslog(LOG_ERR,
					    "%s: no protocol specified",
					    trx_path);
					exit(1);
				}
				lua_pop(t->L, 1);
				lua_setglobal(t->L, "_trx");
				break;
			case LUA_ERRRUN:
			case LUA_ERRMEM:
			case LUA_ERRERR:
				syslog(LOG_ERR, "%s: %s", trx_path,
				    lua_tostring(t->L, -1));
				exit(1);
				break;
			}

			if (protocol == NULL) {
				syslog(LOG_ERR, "%s: no protocol defined",
				    trx_path);
				exit(1);
			}

			snprintf(proto_path, sizeof(proto_path), "%s/%s.lua",
			    _PATH_PROTOCOL, protocol);

			if (stat(proto_path, &sb)) {
				syslog(LOG_ERR, "protocol not found: %s",
				    protocol);
				exit(1);
			}
			if (luaL_dofile(t->L, proto_path)) {
				syslog(LOG_ERR, "%s", lua_tostring(t->L, -1));
				exit(1);
			}

			lua_setglobal(t->L, "_protocol");

			if (luaL_dostring(t->L, "for k, v in pairs(_trx) do "
			    "_protocol[k] = v end")) {
				syslog(LOG_ERR, "%s", lua_tostring(t->L, -1));
				exit(1);
			}

			if (luaL_dofile(t->L, _PATH_TRX_CONTROLLER)) {
				syslog(LOG_ERR, "%s", lua_tostring(t->L, -1));
				exit(1);
			}
			if (lua_type(t->L, -1) != LUA_TTABLE) {
				syslog(LOG_ERR, "table expected");
				exit(1);
			} else
				t->ref = luaL_ref(t->L, LUA_REGISTRYINDEX);

			/*
			 * Setup the registerDriver function, but don't call
			 * it yet as the CAT device is not yet open and it
			 * might be needed by the initialize function.
			 */
			lua_geti(t->L, LUA_REGISTRYINDEX, t->ref);
			lua_getfield(t->L, -1, "registerDriver");
			lua_pushstring(t->L, t->name);
			lua_pushstring(t->L, t->device);

			lua_getglobal(t->L, "_protocol");
			lua_getfield(t->L, -1,
			    "statusUpdatesRequirePolling");
			t->poller_required = lua_toboolean(t->L, -1);
			lua_pop(t->L, 1);

			lua_getfield(L, -1, "configuration");
			if (lua_istable(L, -1)) {
				proxy_map(L, t->L, lua_gettop(t->L));
				lua_setglobal(t->L, "_config");
				if (luaL_dostring(t->L, "for k, v in "
				    "pairs(_config) do _G[k] = v end "
				    "_config = nil")) {
					syslog(LOG_ERR, "%s",
					    lua_tostring(t->L, -1));
					exit(1);
				}
			}
			lua_pop(L, 1);

			lua_getfield(L, -1, "audio");
			if (lua_istable(L, -1))
				proxy_map(L, t->L, lua_gettop(t->L));
			else
				lua_newtable(t->L);
			lua_setfield(t->L, -2, "audio");
			lua_pop(L, 1);

			if (add_destination(t->name, DEST_TRX, t)) {
				syslog(LOG_ERR,
				    "transceivers: names must be unique");
				exit(1);
			}

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
		syslog(LOG_NOTICE, "no transceivers defined\n");
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
			if (!lua_isstring(L, -1)) {
				syslog(LOG_ERR, "missing gpio device path");
				exit(1);
			}
			t->device = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			lua_getfield(L, -1, "speed");
			if (lua_isinteger(L, -1))
				t->speed =lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "driver");
			if (!lua_isstring(L, -1)) {
				syslog(LOG_ERR, "missing gpio driver name");
				exit(1);
			}
			t->driver = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			if (add_destination(t->name, DEST_GPIO, t)) {
				syslog(LOG_ERR, "gpio: names must be unique");
				exit(1);
			}

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
		syslog(LOG_NOTICE, "no gpio defined\n");
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
			if (!lua_isstring(L, -1)) {
				syslog(LOG_ERR, "missing relay driver name");
				exit(1);
			}

			t->driver = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);

			if (add_destination(t->name, DEST_RELAY, t)) {
				syslog(LOG_ERR, "relays: names must be unique");
				exit(1);
			}

			if (pthread_mutex_init(&t->mutex, NULL))
				goto terminate;

			if (pthread_mutex_init(&t->mutex2, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond1, NULL))
				goto terminate;

			if (pthread_cond_init(&t->cond2, NULL))
				goto terminate;

			/* Create the relay-controller thread */
			pthread_create(&t->relay_controller, NULL,
			    relay_controller, t);
			lua_pop(L, 1);
		}
	} else if (verbose)
		syslog(LOG_NOTICE, "no relays defined\n");
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
			if (t == NULL) {
				syslog(LOG_ERR, "memory allocation failure");
				exit(1);
			}
			t->has_config = 0;
			t->listeners = NULL;
			t->L = luaL_newstate();
			if (t->L == NULL) {
				syslog(LOG_ERR, "cannot create Lua state");
				exit(1);
			}
			luaL_openlibs(t->L);
			luaopen_trxd(t->L);
			lua_setglobal(t->L, "trxd");
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
			if (!lua_isstring(L, -1)) {
				syslog(LOG_ERR,
				    "missing extension script name");
				exit(1);
			}
			p = lua_tostring(L, -1);

			if (strchr(p, '/')) {
				syslog(LOG_ERR,
				    "script name must not contain slashes");
				exit(1);
			}
			snprintf(script, sizeof(script), "%s/%s.lua",
			    _PATH_EXTENSION, p);

			lua_pop(L, 1);

			if (luaL_loadfile(t->L, script)) {
				syslog(LOG_ERR, "%s", lua_tostring(t->L, -1));
				exit(1);
			}

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

			if (add_destination(name, DEST_EXTENSION, t)) {
				syslog(LOG_ERR, "names must be unique");
				exit(1);
			}

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
		syslog(LOG_NOTICE, "no extensions defined\n");
	lua_pop(L, 1);

#if 0
	/* Setup private extensions */
	lua_getglobal(L, "json");
	lua_getfield(L, -1, "encode");
	lua_getfield(L, -3, "private-extensions");
	if (lua_istable(L, -1)) {
		switch (lua_pcall(L, 1, 1, 0)) {
		case LUA_OK:
			private_extensions = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);
			if (private_extensions == NULL) {
				syslog(LOG_ERR, "memory error");
				exit(1);
			}
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			syslog(LOG_ERR, "%s", lua_tostring(L, -1));
			exit(1);
			break;
		}
	}
	lua_pop(L, 2);
#endif

	/* Setup WebSocket listening */
	lua_getfield(L, -1, "websocket");
	if (lua_istable(L, -1)) {
		websocket_listener_t *t;

		t = malloc(sizeof(websocket_listener_t));
		if (t == NULL) {
			syslog(LOG_ERR, "memory allocation error");
			exit(1);
		}
		t->ssl = NULL;
		t->ctx = NULL;
		t->root = NULL;
		t->certificate = NULL;
		t->key = NULL;
		t->announce = noannounce ? 0 : 1;

		lua_getfield(L, -1, "bind-address");
		if (!lua_isstring(L, -1)) {
			syslog(LOG_ERR, "missing websocket bind-address");
			exit(1);
		}
		t->bind_addr = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "listen-port");
		if (!lua_isstring(L, -1)) {
			syslog(LOG_ERR, "missing websocket listen-port");
			exit(1);
		}
		t->listen_port = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "path");
		if (!lua_isstring(L, -1)) {
			syslog(LOG_ERR, "missing websocket path");
			exit(1);
		}
		t->path = strdup(lua_tostring(L, -1));
		lua_pop(L, 1);

		if (!noannounce) {
			lua_getfield(L, -1, "announce");
			if (lua_isboolean(L, -1))
				t->announce = lua_toboolean(L, -1);
			lua_pop(L, 1);
		}

		lua_getfield(L, -1, "ssl");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "root");
			if (lua_isstring(L, -1))
				t->root = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);
			lua_getfield(L, -1, "certificate");
			if (lua_isstring(L, -1))
				t->certificate = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);
			lua_getfield(L, -1, "key");
			if (lua_isstring(L, -1))
				t->key = strdup(lua_tostring(L, -1));
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		/* Create the websocket-listener thread */
		pthread_create(&t->listener, NULL, websocket_listener, t);
	}
	lua_pop(L, 1);

	/* Setup NMEA listening */
	lua_getfield(L, -1, "nmea");
	if (lua_istable(L, -1)) {
		nmea_tag_t *t;
		struct termios tty;
		const char *device;
		int speed = 9600;
		int channel = 0;

		t = malloc(sizeof(nmea_tag_t));
		if (t == NULL) {
			syslog(LOG_ERR, "memory allocation error");
			exit(1);
		}
		t->year = t->month = t->day = 0;
		t->hour = t->minute = t->second = 0;
		t->status = 0;
		t->latitude = t->longitude = t->altitude = 0.0;
		t->speed = t->course = t->variation = 0.0;
		t->mode = 'I';
		t->locator[0] = '\0';

		lua_getfield(L, -1, "device");
		if (!lua_isstring(L, -1)) {
			syslog(LOG_ERR, "missing nmea device");
			exit(1);
		}
		device = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "speed");
		if (lua_isinteger(L, -1))
			speed = lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "channel");
		if (lua_isinteger(L, -1))
			channel = lua_tointeger(L, -1);
		lua_pop(L, 1);

		if (*device == '/') {
			/* Assume device under /dev */
			t->fd = open(device, O_RDWR);
			if (t->fd == -1) {
				syslog(LOG_ERR, "cannot open nmea device");
				exit(1);
			}

			if (isatty(t->fd)) {
				if (tcgetattr(t->fd, &tty) < 0) {
					syslog(LOG_ERR, "tcgetattr");
					exit(1);
				} else {
					cfmakeraw(&tty);
					tty.c_cflag |= CLOCAL;
					cfsetspeed(&tty, speed);

					if (tcsetattr(t->fd, TCSADRAIN, &tty)
					   < 0) {
						syslog(LOG_ERR,
						    "tcsetattr");
						exit(1);
					}
				}
			}
		} else if (strlen(device) == 17) {
			/* Assume Bluetooth RFCOMM */
			struct sockaddr_rc addr = { 0 };

			fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

			addr.rc_family = AF_BLUETOOTH;
			addr.rc_channel = (uint8_t) channel;
			str2ba(device, &addr.rc_bdaddr);
			if (connect(fd,
			    (struct sockaddr *)&addr, sizeof(addr))) {
				syslog(LOG_ERR, "can't connect to %s",
				    device);
				exit(1);
			}
		} else {
			syslog(LOG_ERR, "unknown device %s", device);
			exit(1);
		}

		if (add_destination("nmea", DEST_INTERNAL, t)) {
			syslog(LOG_ERR, "nmea: names must be unique");
			exit(1);
		}

		if (pthread_mutex_init(&t->mutex, NULL))
			goto terminate;

		/* Create the nmea-handler thread */
		pthread_create(&t->nmea_handler, NULL, nmea_handler, t);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

#ifdef USE_SDDM
	/* Setup systemd events handler */
	if (monitor_systemd)
		pthread_create(&sd_event_handler_thread, NULL,
		    sd_event_handler, NULL);
#endif

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
