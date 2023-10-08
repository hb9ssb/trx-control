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

/* Provide the 'trxd' Lua module to the driver upper half */

#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "trx-control.h"
#include "trxd.h"

extern command_tag_t *command_tag;

extern void *trx_poller(void *);
extern void *trx_handler(void *);

static int
luatrxd_send(lua_State *L)
{
	int fd;
	size_t len;
	const char *data;

	fd = luaL_checkinteger(L, 1);
	data = luaL_checklstring(L, 2, &len);
	write(fd, data, len);
	return 0;
}

static int
luatrxd_get_transceiver_list(lua_State *L)
{
	command_tag_t *t;
	int n = 1;
	lua_newtable(L);
	for (t = command_tag; t != NULL; t = t->next) {
		lua_pushinteger(L, n++);
		lua_newtable(L);
		lua_pushstring(L, t->name);
		lua_setfield(L, -2, "name");
		lua_pushstring(L, t->device);
		lua_setfield(L, -2, "device");
		lua_pushstring(L, t->driver);
		lua_setfield(L, -2, "driver");
		lua_settable(L, -3);
	}
	return 1;
}

static int
luatrxd_select_transceiver(lua_State *L)
{
	command_tag_t *t;
	int n = 0;
	const char *name;

	name = luaL_checkstring(L, 1);

	for (t = command_tag; t != NULL; t = t->next, n++) {
		if (!strcmp(t->name, name))
			break;
	}

	if (t)
		lua_pushinteger(L, n);
	else
		lua_pushnil(L);
	return 1;
}

static int
luatrxd_start_polling(lua_State *L)
{
	const char *device;
	command_tag_t *t;

	device = luaL_checkstring(L, 1);
	for (t = command_tag; t != NULL; t = t->next) {
		if (!strcmp(t->device, device)) {
			t->poller_running = 1;
			pthread_create(&t->trx_poller, NULL, trx_poller, t);
			lua_pushboolean(L, 1);
			break;
		}
	}
	if (t == NULL)
		lua_pushnil(L);
	return 1;
}

static int
luatrxd_stop_polling(lua_State *L)
{
	const char *device;
	command_tag_t *t;

	device = luaL_checkstring(L, 1);
	for (t = command_tag; t != NULL; t = t->next) {
		if (!strcmp(t->device, device)) {
			if (t->poller_running) {
				t->poller_running = 0;
				pthread_join(t->trx_poller, NULL);
			}
			lua_pushboolean(L, 1);
			break;
		}
	}
	if (t == NULL)
		lua_pushnil(L);
	return 1;
}

static int
luatrxd_start_handling(lua_State *L)
{
	const char *device;
	command_tag_t *t;
	char eol = '\n';

	device = luaL_checkstring(L, 1);
	for (t = command_tag; t != NULL; t = t->next) {
		if (!strcmp(t->device, device)) {
			if (lua_gettop(L) == 2)
				eol = lua_tointeger(L, 2);
			if (pipe(t->handler_pipefd))
				err(1, "pipe");
			t->handler_eol = eol;
			t->handler_running = 1;
			pthread_create(&t->trx_handler, NULL, trx_handler, t);
			lua_pushboolean(L, 1);
			break;
		}
	}
	if (t == NULL)
		lua_pushnil(L);
	return 1;
}

static int
luatrxd_stop_handling(lua_State *L)
{
	const char *device;
	command_tag_t *t;

	device = luaL_checkstring(L, 1);
	for (t = command_tag; t != NULL; t = t->next) {
		if (!strcmp(t->device, device)) {
			if (t->handler_running) {
				t->handler_running = 0;
				write(t->handler_pipefd[1], 0x00, 1);
				pthread_join(t->trx_handler, NULL);
			}
			lua_pushboolean(L, 1);
			break;
		}
	}
	if (t == NULL)
		lua_pushnil(L);
	return 1;
}

static int
luatrxd_send_to_client(lua_State *L)
{
	int fd;
	char *data;

	fd = luaL_checkinteger(L, 1);
	data = (char *)luaL_checkstring(L, 2);
	trxd_writeln(fd, data);
	return 0;
}

int
luaopen_trxd(lua_State *L)
{
	struct luaL_Reg luatrxd[] = {
		{ "send",			luatrxd_send },
		{ "getTransceiverList",		luatrxd_get_transceiver_list },
		{ "selectTransceiver",		luatrxd_select_transceiver },
		{ "startPolling",		luatrxd_start_polling },
		{ "stopPolling",		luatrxd_stop_polling },
		{ "startHandling",		luatrxd_start_handling },
		{ "stopHandling",		luatrxd_stop_handling },
		{ "sendToClient",		luatrxd_send_to_client },
		{ NULL, NULL }
	};

	luaL_newlib(L, luatrxd);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2023 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trx-control internal functions for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "trxd 1.0.0");
	lua_settable(L, -3);

	return 1;
}
