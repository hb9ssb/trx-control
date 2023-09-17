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
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"

extern int luaopen_trx(lua_State *);
extern int luaopen_trxd(lua_State *);
extern int luaopen_json(lua_State *);
extern void *trx_handler(void *);

extern command_tag_t *command_tag;

void *
trx_control(void *arg)
{
	command_tag_t *tag = (command_tag_t *)arg;
	struct termios tty;
	lua_State *L;
	int fd, n, driver_ref;
	struct stat sb;
	char trx_driver[PATH_MAX];
	pthread_t trx_handler_thread;

	L = NULL;
	pthread_detach(pthread_self());

	if (strchr(tag->driver, '/')) {
		syslog(LOG_ERR, "driver must not contain slashes");
		goto terminate;
	}

	fd = open(tag->device, O_RDWR);
	if (fd == -1) {
		syslog(LOG_ERR, "Can't open CAT device %s: %s",
		    tag->device, strerror(errno));
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

	tag->cat_device = fd;

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL) {
		syslog(LOG_ERR, "cannot initialize Lua state");
		goto terminate;
	}
	luaL_openlibs(L);

	lua_pushinteger(L, fd);
	lua_setglobal(L, "_CAT_DEVICE");

	luaopen_trx(L);
	lua_setglobal(L, "trx");
	luaopen_trxd(L);
	lua_setglobal(L, "trxd");
	luaopen_json(L);
	lua_setglobal(L, "json");

	lua_getglobal(L, "package");
	lua_getfield(L, -1, "path");
	lua_pushstring(L, ";" _PATH_TRX "/?.lua");
	lua_concat(L, 2);
	lua_setfield(L, -2, "path");
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
	    tag->driver);

	if (stat(trx_driver, &sb)) {
		syslog(LOG_ERR, "driver for trx-type %s not found",
		    tag->driver);
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
		    tag->driver);
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
	lua_pop(L, 1);

#if 0
	/* handle incoming data from the trx */
	pthread_create(&trx_handler_thread, NULL, trx_handler, tag);
#endif

	tag->is_running = 1;
	while (1) {
		int nargs = 1;

		if (pthread_mutex_lock(&tag->mutex))
			goto terminate;

		if (pthread_cond_wait(&tag->cond, &tag->mutex))
			goto terminate;

#if 0
		if (pthread_mutex_lock(&tag->ai_mutex))
			goto terminate;
#endif

		lua_geti(L, LUA_REGISTRYINDEX, driver_ref);
		lua_getfield(L, -1, tag->handler);
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			tag->reply = "command not supported, "
			    "please submit a bug report";
		} else {
			lua_pushstring(L, tag->data);
			lua_pushinteger(L, tag->client_fd);

			switch (lua_pcall(L, 2, 1, 0)) {
			case LUA_OK:
				break;
			case LUA_ERRRUN:
			case LUA_ERRMEM:
			case LUA_ERRERR:
				syslog(LOG_ERR, "Lua error: %s",
				    lua_tostring(L, -1));
				break;
			}
			if (lua_type(L, -1) == LUA_TSTRING) {
				const char *reply = lua_tostring(L, -1);
				if (!strncmp(reply, SWITCH_TAG,
				    strlen(SWITCH_TAG))) {
					char *name;
					command_tag_t *t;

					name = strchr(reply, ':');
					name++;
					for (t = command_tag; t != NULL;
					   t = t->next) {
						if (!strcmp(t->name, name)) {
							tag->new_tag = t;
							break;
						}
					}

					tag->reply= "{ status: \"Ok\"}";
				} else
					tag->reply = reply;
			} else
				tag->reply = "{result: \"no value\"}";
		}
		lua_pop(L, 2);

		pthread_mutex_lock(&tag->rmutex);
		pthread_cond_signal(&tag->rcond);
		pthread_mutex_unlock(&tag->rmutex);

#if 0
		pthread_mutex_unlock(&tag->ai_mutex);
#endif
		pthread_mutex_unlock(&tag->mutex);
	}

terminate:
	if (L)
		lua_close(L);
	return NULL;
}
