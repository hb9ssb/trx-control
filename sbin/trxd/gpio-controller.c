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

/* Control GPIO pins */

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

extern int luaopen_trxd(lua_State *);
extern int luaopen_gpio_controller(lua_State *);
extern int luaopen_gpio(lua_State *);
extern int luaopen_json(lua_State *);

extern int verbose;

__thread gpio_controller_tag_t	*gpio_controller_tag;
__thread int gpio_device;

static void
cleanup(void *arg)
{
	gpio_controller_tag_t *t = (gpio_controller_tag_t *)arg;
	if (t->L)
		lua_close(t->L);
	free(t->name);
	free(arg);
}

void *
gpio_controller(void *arg)
{
	gpio_controller_tag_t *t = (gpio_controller_tag_t *)arg;
	struct termios tty;
	int fd;
	struct stat sb;
	char gpio_driver[PATH_MAX];

	t->L = NULL;
	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "gpio-controller: pthread_detach");
		exit(1);
	}
	if (verbose)
		printf("gpio-controller: initializing gpio %s\n", t->name);

	gpio_controller_tag = t;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "gpio")) {
		syslog(LOG_ERR, "gpio-controller: pthread_setname_np");
		exit(1);
	}

	/*
	 * Lock this gpios mutex, so that no other thread accesses
	 * while we are initializing.
	 */
	if (pthread_mutex_lock(&t->mutex)) {
		syslog(LOG_ERR, "gpio-controller: pthread_mutex_lock");
		exit(1);
	}

	if (pthread_mutex_lock(&t->mutex2)) {
		syslog(LOG_ERR, "gpio-controller: pthread_mutex_lock");
		exit(1);
	}

	if (strchr(t->driver, '/')) {
		syslog(LOG_ERR, "gpio-controller: driver name must not "
		    "contain slashes");
		exit(1);
	}

	fd = open(t->device, O_RDWR);
	if (fd == -1) {
		syslog(LOG_ERR, "gpio-controller: %s", t->device);
		exit(1);
	}

	if (isatty(fd)) {
		if (tcgetattr(fd, &tty) < 0) {
			syslog(LOG_ERR, "gpio-controller: tcgetattr");
			exit(1);
		} else {
			cfmakeraw(&tty);
			tty.c_cflag |= CLOCAL;
			cfsetspeed(&tty, t->speed);

			if (tcsetattr(fd, TCSADRAIN, &tty) < 0) {
				syslog(LOG_ERR, "gpio-controller: tcsetattr");
				exit(1);
			}
		}
	}

	gpio_device = fd;
	t->gpio_device = fd;

	/* Setup Lua */
	t->L = luaL_newstate();
	if (t->L == NULL) {
		syslog(LOG_ERR, "gpio-controller: luaL_newstate");
		exit(1);
	}

	luaL_openlibs(t->L);

	luaopen_gpio(t->L);
	lua_setglobal(t->L, "gpio");
	luaopen_trxd(t->L);
	lua_setglobal(t->L, "trxd");
	luaopen_gpio_controller(t->L);
	lua_setglobal(t->L, "gpioController");
	luaopen_json(t->L);
	lua_setglobal(t->L, "json");

	if (luaL_dofile(t->L, _PATH_GPIO_CONTROLLER)) {
		syslog(LOG_ERR, "gpio-controller: %s", lua_tostring(t->L, -1));
		exit(1);
	}
	if (lua_type(t->L, -1) != LUA_TTABLE) {
		syslog(LOG_ERR, "gpio-controller: table expected");
		exit(1);
	} else
		t->ref = luaL_ref(t->L, LUA_REGISTRYINDEX);

	snprintf(gpio_driver, sizeof(gpio_driver), "%s/%s.lua", _PATH_GPIO,
	    t->driver);

	if (stat(gpio_driver, &sb)) {
		syslog(LOG_ERR, "gpio-controller: %s", t->driver);
		exit(1);
	}

	lua_geti(t->L, LUA_REGISTRYINDEX, t->ref);
	lua_getfield(t->L, -1, "registerDriver");
	lua_pushstring(t->L, t->name);
	lua_pushstring(t->L, t->device);
	if (luaL_dofile(t->L, gpio_driver)) {
		syslog(LOG_ERR, "gpio-controller: %s", lua_tostring(t->L, -1));
		exit(1);
	}
	if (lua_type(t->L, -1) != LUA_TTABLE) {
		syslog(LOG_ERR, "gpio-controller: %s: table expected",
		    t->driver);
		exit(1);
	} else {
		lua_getfield(t->L, -1, "statusUpdatesRequirePolling");
		t->poller_required = lua_toboolean(t->L, -1);
		lua_pop(t->L, 1);
		switch (lua_pcall(t->L, 3, 0, 0)) {
		case LUA_OK:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			syslog(LOG_ERR, "trx-controller: %s",
			    lua_tostring(t->L, -1));
			exit(1);
			break;
		}
	}
	lua_pop(t->L, 1);

	t->is_running = 1;

	/*
	 * We are ready to go, unlock the mutex, so that client-handlers,
	 * gpio-handlers, and, gpio-pollers can access it.
	 */
	if (pthread_mutex_unlock(&t->mutex)) {
		syslog(LOG_ERR, "gpio-controller: pthread_mutex_unlock");
		exit(1);
	}

	while (1) {
		/* Wait on cond, this releases the mutex */
		while (t->handler == NULL) {
			if (pthread_cond_wait(&t->cond1, &t->mutex2)) {
				syslog(LOG_ERR, "gpio-controller: "
				    "pthread_cond_wait");
				exit(1);
			}
		}
		lua_geti(t->L, LUA_REGISTRYINDEX, t->ref);
		lua_getfield(t->L, -1, t->handler);
		if (lua_type(t->L, -1) != LUA_TFUNCTION) {
			t->response = "command not supported, "
			    "please submit a bug report";
		} else {
			lua_pushstring(t->L, t->data);
			t->response = NULL;

			switch (lua_pcall(t->L, 1, 1, 0)) {
			case LUA_OK:
				if (lua_type(t->L, -1) == LUA_TSTRING)
					t->response =
					    (char *)lua_tostring(t->L, -1);
				else
					t->response = "";
				break;
			case LUA_ERRRUN:
			case LUA_ERRMEM:
			case LUA_ERRERR:
				t->response = "{\"status\":\"Error\","
				    "\"reason\":\"Lua error\"}";

				syslog(LOG_ERR, "Lua error: %s",
				    lua_tostring(t->L, -1));
				break;
			}
		}
		lua_pop(t->L, 2);
		t->handler = NULL;

		if (pthread_cond_signal(&t->cond2)) {
			syslog(LOG_ERR, "gpio-controller: pthread_cond_signal");
			exit(1);
		}
	}
	pthread_cleanup_pop(0);
	return NULL;
}
