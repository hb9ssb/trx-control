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
#include <stdlib.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "trx-control.h"
#include "trxd.h"

extern destination_t *destination;

extern void *trx_poller(void *);
extern void *trx_handler(void *);

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


static int
luatrxd_notify_listeners(lua_State *L)
{
	int n;
	sender_list_t *l;
	const char *device;
	char *data;
	destination_t *d;

	device = luaL_checkstring(L, 1);
	data = (char *)luaL_checkstring(L, 2);

	for (d = destination; d != NULL; d = d->next) {
		if (d->type == DEST_TRX
		    && !strcmp(d->tag.trx->device, device)) {
			for (l = d->tag.trx->senders; l != NULL; l = l->next) {
				if (pthread_mutex_lock(&l->sender->mutex))
					err(1, "luatrxd: pthread_mutex_lock");
				l->sender->data = data;
				if (pthread_cond_signal(&l->sender->cond))
					err(1, "luatrxd: pthread_cond_signal");
				if (pthread_mutex_unlock(&l->sender->mutex))
					err(1, "luatrxd: pthread_mutex_unlock");
			}
			break;
		}
	}
	return 0;
}

static int
luatrxd_num_listeners(lua_State *L)
{
	int n;
	sender_list_t *l;
	const char *device;
	destination_t *d;

	device = luaL_checkstring(L, 1);
	for (d = destination; d != NULL; d = d->next) {
		if (d->type == DEST_TRX
		    && !strcmp(d->tag.trx->device, device)) {
			l = d->tag.trx->senders;
			for (n = 0; l != NULL; ++n) {
				if (l->next == NULL) {
					++n;
					break;
				} else
					l = l->next;
			}
			lua_pushinteger(L, n);
			break;
		}
	}
	if (d == NULL)
		lua_pushnil(L);

	return 1;
}

int
luaopen_trxd(lua_State *L)
{
	struct luaL_Reg luatrxd[] = {
		{ "notifyListeners",		luatrxd_notify_listeners },
		{ "numListeners",		luatrxd_num_listeners },

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
	lua_pushliteral(L, "trxd 1.1.0");
	lua_settable(L, -3);

	return 1;
}
