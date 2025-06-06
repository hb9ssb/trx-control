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

/* Provide the 'gpioController' Lua module to the driver upper half */

#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "trx-control.h"
#include "trxd.h"

extern __thread gpio_controller_tag_t	*gpio_controller_tag;

static int
notify_listeners(lua_State *L)
{
	sender_list_t *l;
	char *data;

	data = (char *)luaL_checkstring(L, 1);

	for (l = gpio_controller_tag->senders; l != NULL; l = l->next) {
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

int
luaopen_gpio_controller(lua_State *L)
{
	struct luaL_Reg luagpiocontroller[] = {
		{ "notifyListeners",		notify_listeners },
		{ NULL, NULL }
	};

	luaL_newlib(L, luagpiocontroller);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2023 - 2024 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trx-control internal functions for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "gpioController " TRXD_VERSION);
	lua_settable(L, -3);

	return 1;
}
