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
int cmdhandler_ref = LUA_REFNIL;
lua_State *L;

static struct {
	const char *command;
	const char *func;
} command_map[] = {
	"use",			"useTrx",
	"list-trx",		"listTrx",
	"set-frequency",	"setFrequency",
	"get-frequency",	"getFrequency",
	"set-mode",		"setMode",
	"get-mode",		"getMode",
	"start-status-updates",	"startStatusUpdates",
	"stop-status-updates",	"stopStatusUpdates",
	"lock",			"lockTrx",
	"unlock",		"unlockTrx",
	NULL,			NULL
};

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
			errx(1, "trxd disconnected\n");
		} else {
			lua_geti(L, LUA_REGISTRYINDEX, cmdhandler_ref);
			lua_getfield(L, -1, "handleStatusUpdate");
			if (lua_type(L, -1) != LUA_TFUNCTION) {
				fprintf(stderr, "status updates not supported,"
				    " please submit a bug report\n");
			} else {

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
			}
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
	wordexp_t p;
	int c, n, terminate;
	char *host, *port, buf[128], *line, *param;

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
		fprintf(stderr, "Lua error: %s", lua_tostring(L, -1));
		exit(1);
	}
	if (lua_type(L, -1) != LUA_TTABLE) {
		fprintf(stderr, "trxctl did not return a table\n");
		exit(1);
	} else
		cmdhandler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	fd = trxd_connect(host, port);
	if (fd < 0) {
		fprintf(stderr, "connection to %s:%s failed\n", host, port);
		exit(1);
	}

	wordexp(HISTORY, &p, 0);
	read_history(p.we_wordv[0]);
	rl_event_hook = handle_status_updates;

	for (terminate = 0; !terminate; ) {
		line = rl_gets();

		if (line == NULL) {
			terminate = 1;
			break;
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
		for (n = 0; command_map[n].command != NULL; n++)
			if (!strcmp(command_map[n].command, line))
				break;

		if (command_map[n].command != NULL) {
			lua_geti(L, LUA_REGISTRYINDEX, cmdhandler_ref);
			lua_getfield(L, -1, command_map[n].func);
			if (lua_type(L, -1) != LUA_TFUNCTION) {
				fprintf(stderr, "command not supported,"
				    " please submit a bug report\n");
			} else {
				if (param)
					lua_pushstring(L, param);
				switch (lua_pcall(L, param ? 1 : 0, 1,
				    0)) {
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
	write_history(p.we_wordv[0]);
	wordfree(&p);
	lua_close(L);
	close(fd);
	return 0;
}
