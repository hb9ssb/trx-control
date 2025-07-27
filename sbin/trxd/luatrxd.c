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

/* Provide the 'trxd' Lua module to all Lua states */

#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include <zmq.h>

#include "luazmq.h"
#include "trx-control.h"
#include "trxd.h"

extern int verbose;
extern __thread extension_tag_t	*extension_tag;
extern void *zmq_ctx;

extern void *signal_input(void *);

static int
luatrxd_notify(lua_State *L)
{
	sender_list_t *l;
	char *data;

	data = (char *)luaL_checkstring(L, 1);

	for (l = extension_tag->listeners; l != NULL; l = l->next) {
		if (pthread_mutex_lock(&l->sender->mutex)) {
			syslog(LOG_ERR, "luatrxd: pthread_mutex_lock");
			exit(1);
		}
		l->sender->data = data;
		if (pthread_cond_signal(&l->sender->cond)) {
			syslog(LOG_ERR, "luatrxd: pthread_cond_signal");
			exit(1);
		}
		if (pthread_mutex_unlock(&l->sender->mutex)) {
			syslog(LOG_ERR, "luatrxd: pthread_mutex_unlock");
			exit(1);
		}
	}
	return 0;
}

static int
luatrxd_signal_input(lua_State *L)
{
	signal_input_t *s;

	s = malloc(sizeof(signal_input_t));
	if (s == NULL)
		return luaL_error(L, "out of memory");
	s->fd = luaL_checkinteger(L, 1);
	s->func = strdup(luaL_checkstring(L, 2));
	s->extension = extension_tag;

	/* Create the signal-input thread */
	pthread_create(&s->signal_input, NULL, signal_input, s);
	return 0;
}

static int
luatrxd_locator(lua_State *L)
{
	double lat, lon;
	char locator[6];

	lat = luaL_checknumber(L, 1);
	lon = luaL_checknumber(L, 2);

	if (lat >= 90.0 || lat < -90.0) {
		lua_pushnil(L);
		lua_pushstring(L, "latitude out of range");
		return 2;
	}

	if (lon >= 180.0 || lon < -180.0) {
		lua_pushnil(L);
		lua_pushstring(L, "longitude out of range");
		return 2;
	}

	lon += 180.0;
	lat += 90.0;

	locator[0] = 'A' + lon / 20;
	locator[1] = 'A' + lat / 10;
	locator[2] = '0' + (int)lon % 20 / 2;
	locator[3] = '0' + (int)lat % 10;
	locator[4] = 'A' + (lon - (int)lon / 2 * 2 ) * 12;
	locator[5] = 'A' + (lat - (int)lat) * 24;

	lua_pushlstring(L, locator, sizeof(locator));
	return 1;
}

static int
luatrxd_verbose(lua_State *L)
{
	lua_pushinteger(L, verbose);
	return 1;
}

static int
luatrxd_version(lua_State *L)
{
	lua_pushstring(L, TRXD_VERSION);
	return 1;
}

static int
luatrxd_zmq_context(lua_State *L)
{
	void **ctx;

	ctx = lua_newuserdata(L, sizeof(void *));
	*ctx = zmq_ctx;
	luaL_getmetatable(L, ZMQ_CTX_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

int
luaopen_trxd(lua_State *L)
{
	struct luaL_Reg luatrxd[] = {
		{ "notify",		luatrxd_notify },
		{ "signalInput",	luatrxd_signal_input },
		{ "locator",		luatrxd_locator },
		{ "verbose",		luatrxd_verbose },
		{ "version",		luatrxd_version },
		{ "zmqContext",		luatrxd_zmq_context },
		{ NULL, NULL }
	};

	luaL_newlib(L, luatrxd);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2023 - 2025 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trx-control internal functions for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "trxd " TRXD_VERSION);
	lua_settable(L, -3);

	return 1;
}
