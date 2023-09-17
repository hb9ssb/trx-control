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

#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define DEFAULT_HOST	"localhost"
#define DEFAULT_PORT	"14285"
#define _PATH_TRXCTL	"/usr/share/trxctl/trxctl.lua"

extern void luaopen_trxctl(lua_State *);
extern void luaopen_json(lua_State *);

int fd = 0;

static struct {
	const char *command;
	const char *func;
} command_map[] = {
	"use",			"useTrx",
	"list-trx",		"listTrx",
	"set-frequency",	"setFrequency",
	"get-frequency",	"getFrequency",
	"listen-frequency",	"listenFrequency",
	"unlisten-frequency",	"unlistenFrequency",
	NULL,			NULL
};

static void
usage(void)
{
	(void)fprintf(stderr, "usage: trxctl [-lv] [-p port] [-u name] command "
	    "[args ...]\n");
	exit(1);
}

int
connect_trxd(const char *host, const char *port)
{
	struct addrinfo hints, *res, *res0;
	struct linger linger;
	int error;
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

static void
list_trx(int fd)
{
}

static void
use_trx(int fd, char *trx)
{
}


char *
rl_gets()
{
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("trxctl> ");

	if (line_read && *line_read)
		add_history(line_read);
	return line_read;
}

int
main(int argc, char *argv[])
{
	lua_State *L;
	int fd, c, n, list, verbosity, cmdhandler_ref;
	char *host, *port, *trx, buf[128], *line;

	verbosity = 0;
	host = DEFAULT_HOST;
	port = DEFAULT_PORT;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "list",		no_argument, 0, 'l' },
			{ "use",		required_argument, 0, 'u' },
			{ "host",		required_argument, 0, 'h' },
			{ "verbose",		no_argument, 0, 'v' },
			{ "port",		required_argument, 0, 'p' },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "h:lu:vp:", long_options,
		    &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			break;
		case 'h':
			host = optarg;
			break;
		case 'l':
			list = 1;
			break;
		case 'p':
			port = optarg;
			break;
		case 'u':
			trx = optarg;
			break;
		case 'v':
			verbosity++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	L = luaL_newstate();
	if (L == NULL) {
		fprintf(stderr, "cannot initialize Lua state\n");
		exit(1);
	}
	luaL_openlibs(L);
	luaopen_trxctl(L);
	lua_setglobal(L, "trxctl");
	luaopen_json(L);
	lua_setglobal(L, "json");
	if (luaL_dofile(L, _PATH_TRXCTL)) {
		fprintf(stderr, "Lua error: %s", lua_tostring(L, -1));
		exit(1);
	}
	if (lua_type(L, -1) != LUA_TTABLE) {
		fprintf(stderr, "trxctl did not return a table\n");
		exit(1);
	} else
		cmdhandler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	fd = connect_trxd(host, port);
	if (fd < 0) {
		fprintf(stderr, "connection failed\n");
		exit(1);
	}

	while ((line = rl_gets())) {
		char *param;

		param = strchr(line, ' ');
		if (param) {
			*param++ = '\0';
			while (*param == ' ')
				++param;
		}

		if (!strcmp(line, "quit"))
			break;

		for (n = 0; command_map[n].command != NULL; n++)
			if (!strcmp(command_map[n].command, line))
				break;

		if (command_map[n].command != NULL) {
			lua_geti(L, LUA_REGISTRYINDEX, cmdhandler_ref);
			lua_getfield(L, -1, command_map[n].func);
			if (lua_type(L, -1) != LUA_TFUNCTION) {
				fprintf(stderr, "command not supported, "
				    "please submit a bug report\n");
			} else {
				if (param)
					lua_pushstring(L, param);
				switch (lua_pcall(L, param ? 1 : 0, 1, 0)) {
				case LUA_OK:
					break;
				case LUA_ERRRUN:
				case LUA_ERRMEM:
				case LUA_ERRERR:
					syslog(LOG_ERR, "Lua error: %s",
					    lua_tostring(L, -1));
					break;
				}
				if (lua_type(L, -1) == LUA_TSTRING)
					printf("trxctl > %s\n",
					    lua_tostring(L, -1));
			}
			lua_pop(L, 1);
		} else if (*line)
			printf("no such command\n");

	}

	lua_close(L);
	close(fd);
	return 0;
}
