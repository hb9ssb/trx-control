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

#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "trx-control.h"

#define DEFAULT_HOST	"localhost"
#define DEFAULT_PORT	"14285"
#define _PATH_TRXCTL	"/usr/share/trxctl/trxctl.lua"
#define HISTORY		"~/.trxctl_history"

extern void luaopen_trxctl(lua_State *);
extern void luaopen_json(lua_State *);

int fd = 0;
int verbose = 0;
lua_State *L;
wordexp_t p;

static void
usage(void)
{
	(void)fprintf(stderr, "usage: trxctl [-lv] [-p port]\n");
	exit(1);
}

int
handle_status_updates()
{
	struct pollfd pfd;
	char *line;

	pfd.fd = fd;
	pfd.events = POLLIN;

	if (poll(&pfd, 1, -0) == -1)
		err(1, "poll");

	if (pfd.revents) {
		line = trxd_readln(fd);
		if (line == NULL) {
			/* Terminate prompt */
			printf("\n");

			/* Make sure the terminal can be used */
			rl_deprep_terminal();

			/* Write out command history and close the Lua state */
			write_history(p.we_wordv[0]);
			wordfree(&p);
			lua_close(L);
			errx(1, "trxd disconnected\n");
		} else {
			lua_getglobal(L, "handleStatusUpdate");
			lua_pushstring(L, line);
			switch (lua_pcall(L, 1, 1, 0)) {
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
				printf("\n%s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
			free(line);
			rl_forced_update_display();
		}
	}
}

char *
rl_gets()
{
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("trxctl > ");

	if (line_read && *line_read)
		add_history(line_read);
	return line_read;
}

int
main(int argc, char *argv[])
{
	int c, n, terminate;
	char *host, *port, buf[128], *line, *param, *to;

	verbose = 0;
	host = DEFAULT_HOST;
	port = DEFAULT_PORT;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "host",		required_argument, 0, 'h' },
			{ "verbose",		no_argument, 0, 'v' },
			{ "port",		required_argument, 0, 'p' },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "h:vp:", long_options,
		    &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			break;
		case 'h':
			host = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

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
		fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
		exit(1);
	}

	fd = trxd_connect(host, port);
	if (fd < 0) {
		fprintf(stderr, "connection to %s:%s failed\n", host, port);
		exit(1);
	}

	wordexp(HISTORY, &p, 0);
	read_history(p.we_wordv[0]);
	rl_event_hook = handle_status_updates;

	for (terminate = 0; !terminate; ) {
		to = NULL;

		line = rl_gets();

		if (line == NULL) {
			terminate = 1;
			break;
		}

		if (*line == '!') {
			char *p;

			to = ++line;
			p = strchr(to, ' ');
			if (p) {
				*p++ = '\0';
				line = p;
			} else
				while (*line++)
					;
		}

		param = strchr(line, ' ');
		if (param) {
			*param++ = '\0';
			while (*param == ' ')
				++param;
		}

		if (!strcmp(line, "quit")) {
			terminate = 1;
			break;
		}

		if (to || strlen(line)) {
			lua_getglobal(L, "call");
			if (lua_type(L, -1) != LUA_TFUNCTION)
				errx(1, "call function not found");

			if (to)
				lua_pushstring(L, to);
			else
				lua_pushnil(L);
			if (*line)
				lua_pushstring(L, line);
			else
				lua_pushnil(L);
			if (param)
				lua_pushstring(L, param);
			else
				lua_pushnil(L);
			switch (lua_pcall(L, 3, 1, 0)) {
			case LUA_OK:
				break;
			case LUA_ERRRUN:
			case LUA_ERRMEM:
			case LUA_ERRERR:
				syslog(LOG_ERR, "Lua error: %s",
				    lua_tostring(L, -1));
				break;
			}
		}
	}
	write_history(p.we_wordv[0]);
	wordfree(&p);
	lua_close(L);
	close(fd);
	return 0;
}
