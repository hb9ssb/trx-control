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

/* Control a transceiver using a driver written in Lua */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>

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

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"

extern int luaopen_trx(lua_State *);
extern int luaopen_trxd(lua_State *);
extern int luaopen_trx_controller(lua_State *);
extern int luaopen_json(lua_State *);
extern void *trx_handler(void *);

extern int verbose;

__thread trx_controller_tag_t	*trx_controller_tag;
__thread int cat_device;

static void
cleanup(void *arg)
{
	trx_controller_tag_t *t = (trx_controller_tag_t *)arg;
	if (t->L)
		lua_close(t->L);
	free(t->name);
	free(arg);
}

void *
trx_controller(void *arg)
{
	trx_controller_tag_t *t = (trx_controller_tag_t *)arg;
	struct termios tty;
	int fd, n;
	struct stat sb;
	pthread_t trx_handler_thread;

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "trx-controller: pthread_detach");
		exit(1);
	}
	if (verbose)
		printf("trx-controller: initializing trx %s\n", t->name);

	trx_controller_tag = t;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "trx")) {
		syslog(LOG_ERR, "trx-controller: pthread_setname_np");
		exit(1);
	}

	/*
	 * Lock this transceivers mutex, so that no other thread accesses
	 * while we are initializing.
	 */
	if (pthread_mutex_lock(&t->mutex)) {
		syslog(LOG_ERR, "trx-controller: pthread_mutex_lock");
		exit(1);
	}

	if (pthread_mutex_lock(&t->mutex2)) {
		syslog(LOG_ERR, "trx-controller: pthread_mutex_lock");
		exit(1);
	}

	if (*t->device == '/') {	/* Assume device under /dev */
		fd = open(t->device, O_RDWR);
		if (fd == -1) {
			syslog(LOG_ERR, "trx-controller: can't open %s",
			    t->device);
			exit(1);
		}

		if (isatty(fd)) {
			if (tcgetattr(fd, &tty) < 0) {
				syslog(LOG_ERR, "trx-controller: tcgetattr");
				exit(1);
			} else {
				cfmakeraw(&tty);
				tty.c_cflag |= CLOCAL;
				cfsetspeed(&tty, t->speed);

				if (tcsetattr(fd, TCSADRAIN, &tty) < 0) {
					syslog(LOG_ERR, "trx-controller: tcsetattr");
					exit(1);
				}
			}
		}
	} else if (strlen(t->device) == 17) {	/* Assume Bluetooth RFCOMM */
		struct sockaddr_rc addr = { 0 };

		fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
		if (fd == -1) {
			syslog(LOG_ERR, "trx-controller: can't get bluetooth socket"
			    ": %s", t->device, strerror(errno));
			exit(1);
		}

		addr.rc_family = AF_BLUETOOTH;
		addr.rc_channel = (uint8_t) t->channel;
		str2ba(t->device, &addr.rc_bdaddr);

		if (verbose)
			syslog(LOG_INFO, "trx-controller: attempting to "
			    "connect to %s", t->device);

		if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
			syslog(LOG_ERR, "trx-controller: can't connect to %s"
			    ": %s", t->device, strerror(errno));
			exit(1);
		}

		if (verbose)
			syslog(LOG_INFO, "trx-controller: connected to %s",
			    t->device);
	} else {
		syslog(LOG_ERR, "trx-controller: unknown device %s", t->device);
		exit(1);
	}

	cat_device = fd;
	t->cat_device = fd;

	if (verbose)
		printf("trx_controller: registering the driver\n");

	/*
	 * Call the registerDriver function which had been setup in the
	 * main thread.
	 */
	switch (lua_pcall(t->L, 3, 0, 0)) {
	case LUA_OK:
		break;
	case LUA_ERRRUN:
	case LUA_ERRMEM:
	case LUA_ERRERR:
		syslog(LOG_ERR, "trx-controller: register driver %s",
		    lua_tostring(t->L, -1));
		exit(1);
		break;
	}
	lua_pop(t->L, 1);

	t->is_running = 1;

	/*
	 * We are ready to go, unlock the mutex, so that client-handlers,
	 * trx-handlers, and, trx-pollers can access it.
	 */

	if (verbose)
		printf("trx-controller: ready to control trx %s\n", t->name);

	if (pthread_mutex_unlock(&t->mutex)) {
		syslog(LOG_ERR, "trx-controller: pthread_mutex_unlock");
		exit(1);
	}

	for (;;) {
		int nargs = 1;

		/* Wait on cond, this releases the mutex */
		while (t->handler == NULL) {
			if (pthread_cond_wait(&t->cond1, &t->mutex2)) {
				syslog(LOG_ERR, "trx-controller: "
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
			lua_pushinteger(t->L, t->client_fd);
			t->response = NULL;

			switch (lua_pcall(t->L, 2, 1, 0)) {
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
			syslog(LOG_ERR, "trx-controller: pthread_cond_signal");
			exit(1);
		}
	}
	pthread_cleanup_pop(0);
	return NULL;
}
