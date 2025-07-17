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

/* Control relays */

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
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"

extern int luaopen_trxd(lua_State *);
extern int luaopen_json(lua_State *);

extern trx_controller_tag_t *trx_controller_tag;
extern int verbose;

static void
cleanup(void *arg)
{
	relay_controller_tag_t *t = (relay_controller_tag_t *)arg;
	free(t->name);
	free(arg);
}

static void
cleanup_lua(void *arg)
{
	lua_State *L = (lua_State *)arg;
	lua_close(L);
}

void *
relay_controller(void *arg)
{
	relay_controller_tag_t *t = (relay_controller_tag_t *)arg;
	lua_State *L;

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "relay-controller: pthread_detach");
		exit(1);
	}

	if (pthread_setname_np(pthread_self(), "relay")) {
		syslog(LOG_ERR, "relay-controller: pthread_setname_np");
		exit(1);
	}

	pthread_cleanup_push(cleanup, arg);

	/*
	 * Lock this relays mutex, so that no other thread accesses
	 * while we are initializing.
	 */
	if (pthread_mutex_lock(&t->mutex)) {
		syslog(LOG_ERR, "relay-controller: pthread_mutex_lock");
		exit(1);
	}

	if (pthread_mutex_lock(&t->mutex2)) {
		syslog(LOG_ERR, "relay-controller: pthread_mutex_lock");
		exit(1);
	}

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL) {
		syslog(LOG_ERR, "relay-controller: luaL_newstate");
		exit(1);
	}

	pthread_cleanup_push(cleanup_lua, L);

	luaL_openlibs(L);

	luaopen_trxd(L);
	lua_setglobal(L, "trxd");
	luaopen_json(L);
	lua_setglobal(L, "json");

	t->is_running = 1;

	/*
	 * We are ready to go, unlock the mutex, so that client-handlers,
	 * relay-handlers, and, relay-pollers can access it.
	 */
	if (pthread_mutex_unlock(&t->mutex)) {
		syslog(LOG_ERR, "relay-controller: pthread_mutex_unlock");
		exit(1);
	}

	while (1) {
		/* Wait on cond, this releases the mutex */
		while (t->handler == NULL) {
			if (pthread_cond_wait(&t->cond1, &t->mutex2)) {
				syslog(LOG_ERR, "relay-controller: "
				    "pthread_cond_wait");
				exit(1);
			}
		}

		lua_pushstring(L, t->data);

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
			t->response = (char *)lua_tostring(L, -1);
		else
			t->response = "";

		lua_pop(L, 2);
		t->handler = NULL;

		if (pthread_cond_signal(&t->cond2)) {
			syslog(LOG_ERR, "relay-controller: "
			    "pthread_cond_signal");
			exit(1);
		}
	}
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	return NULL;
}
