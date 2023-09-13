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

/* Control a transceiver using a driver written in Lua */

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"

extern int luaopen_trx(lua_State *);
extern int luaopen_trxd(lua_State *);
extern void *trx_handler(void *);
int trx_control_running = 0;
int fd = -1;
extern command_tag_t *command_tag;

static struct {
	const char *command;
	const char *func;
} command_map[] = {
	"load-driver",		"loadDriver",
	"get-transceiver",	"getTransceiver",
	"set-frequency",	"setFrequency",
	"get-frequency",	"getFrequency",
	"listen-frequency",	"listenFrequency",
	"unlisten-frequency",	"unlistenFrequency",
	NULL,			NULL
};

void *
trx_control(void *arg)
{
	controller_t controller = *(controller_t *)arg;
	struct termios tty;
	lua_State *L;
	int n, driver_ref;
	struct stat sb;
	char trx_driver[PATH_MAX];
	pthread_t trx_handler_thread;

	L = NULL;
	trx_control_running = 1;
	pthread_detach(pthread_self());

	if (strchr(controller.trx_type, '/')) {
		syslog(LOG_ERR, "trx-type must not contain slashes");
		goto terminate;
	}

	fd = open(controller.device, O_RDWR);
	if (fd == -1) {
		syslog(LOG_ERR, "Can't open CAT device %s: %s",
		    controller.device, strerror(errno));
		goto terminate;
	}
	if (isatty(fd)) {
		if (tcgetattr(fd, &tty) < 0)
			syslog(LOG_ERR, "Can't get CAT device tty attributes");
		else {
			cfmakeraw(&tty);
			tty.c_cflag |= CLOCAL;
			if (tcsetattr(fd, TCSADRAIN, &tty) < 0)
				syslog(LOG_ERR,
				    "Can't set CAT device tty attributes");
		}
	}

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL) {
		syslog(LOG_ERR, "cannot initialize Lua state");
		goto terminate;
	}
	luaL_openlibs(L);
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "preload");
	lua_pushcfunction(L, luaopen_trx);
	lua_setfield(L, -2, "trx");
	lua_pushcfunction(L, luaopen_trxd);
	lua_setfield(L, -2, "trxd");
	lua_pop(L, 1);

	if (luaL_dofile(L, _PATH_TRX_CONTROL)) {
		syslog(LOG_ERR, "Lua error: %s", lua_tostring(L, -1));
		goto terminate;
	}
	if (lua_type(L, -1) != LUA_TTABLE) {
		syslog(LOG_ERR, "trx driver did not return a table");
		goto terminate;
	} else
		driver_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	snprintf(trx_driver, sizeof(trx_driver), "%s/%s.lua", _PATH_TRX,
	    controller.trx_type);

	if (stat(trx_driver, &sb)) {
		syslog(LOG_ERR, "driver for trx-type %s not found",
		    controller.trx_type);
		goto terminate;
	}

	lua_geti(L, LUA_REGISTRYINDEX, driver_ref);
	lua_getfield(L, -1, "registerDriver");

	if (luaL_dofile(L, trx_driver)) {
		syslog(LOG_ERR, "Lua error: %s", lua_tostring(L, -1));
		goto terminate;
	}
	if (lua_type(L, -1) != LUA_TTABLE) {
		syslog(LOG_ERR, "trx driver %s did not return a table",
		    controller.trx_type);
		goto terminate;
	} else {
		switch (lua_pcall(L, 1, 0, 0)) {
		case LUA_OK:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			syslog(LOG_ERR, "Lua error: %s", lua_tostring(L, -1));
			break;
		}
	}
	lua_pop(L, 2);

	/* handle incoming data from the trx */
	pthread_create(&trx_handler_thread, NULL, trx_handler, &fd);

	while (1) {
		int nargs = 1;

		if (pthread_mutex_lock(&command_tag->mutex))
			goto terminate;

		if (pthread_cond_wait(&command_tag->cond, &command_tag->mutex))
			goto terminate;

		if (pthread_mutex_lock(&command_tag->ai_mutex))
			goto terminate;

		for (n = 0; command_map[n].command != NULL; n++)
			if (!strcmp(command_map[n].command,
			    command_tag->command))
				break;

		if (command_map[n].command != NULL) {
			lua_geti(L, LUA_REGISTRYINDEX, driver_ref);
			lua_getfield(L, -1, command_map[n].func);
			if (lua_type(L, -1) != LUA_TFUNCTION) {
				command_tag->reply = "command not supported, "
				    "please submit a bug report";
			} else {
				if (command_tag->param) {
					lua_pushstring(L, command_tag->param);
					nargs++;
				}
				lua_pushinteger(L, command_tag->client_fd);
				switch (lua_pcall(L, nargs, 1, 0)) {
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
					command_tag->reply =
					    (char *)lua_tostring(L, -1);
				else
					command_tag->reply = "no result";
			}
			lua_pop(L, 2);
		} else
			command_tag->reply = "no such command";

		pthread_mutex_lock(&command_tag->rmutex);
		pthread_cond_signal(&command_tag->rcond);
		pthread_mutex_unlock(&command_tag->rmutex);


		pthread_mutex_unlock(&command_tag->ai_mutex);
		pthread_mutex_unlock(&command_tag->mutex);
	}

terminate:
	if (L)
		lua_close(L);
	trx_control_running = 0;
	return NULL;
}
