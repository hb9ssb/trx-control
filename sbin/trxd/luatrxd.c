/*
 * Copyright (c) 2023 - 2024 Marc Balmer HB9SSB
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

extern int verbose;
extern __thread extension_tag_t	*extension_tag;

extern void *signal_input(void *);

static int
luatrxd_notify(lua_State *L)
{
	sender_list_t *l;
	char *data;

	data = (char *)luaL_checkstring(L, 1);

	for (l = extension_tag->listeners; l != NULL; l = l->next) {
		if (pthread_mutex_lock(&l->sender->mutex))
			err(1, "luatrxd: pthread_mutex_lock");
		l->sender->data = data;
		if (pthread_cond_signal(&l->sender->cond))
			err(1, "luatrxd: pthread_cond_signal");
		if (pthread_mutex_unlock(&l->sender->mutex))
			err(1, "luatrxd: pthread_mutex_unlock");
	}
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

	if (verbose)
		printf("trxd: signal input for extension %p on file "
		    "descriptor %d\n", s->extension, s->fd);

	/* Create the signal-input thread */
	pthread_create(&s->signal_input, NULL, signal_input, s);
}

static int
luatrxd_verbose(lua_State *L)
{
	lua_pushinteger(L, verbose);
	return 1;
}

int
luaopen_trxd(lua_State *L)
{
	struct luaL_Reg luatrxd[] = {
		{ "notify",		luatrxd_notify },
		{ "signalInput",	luatrxd_signal_input },
		{ "verbose",		luatrxd_verbose },
		{ NULL, NULL }
	};

	luaL_newlib(L, luatrxd);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2023 - 2024 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trx-control internal functions for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "trxd " TRXD_VERSION);
	lua_settable(L, -3);

	return 1;
}
