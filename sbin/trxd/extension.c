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

/* trx-control extensions written in Lua */

#include <err.h>
#include <errno.h>
#include <pthread.h>
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

extern int verbose;

static void
cleanup(void *arg)
{
	if (verbose > 1)
		printf("extension: cleanup\n");
	lua_close(arg);
}

void *
extension(void *arg)
{
	extension_tag_t *tag = (extension_tag_t *)arg;
	lua_State *L;
	int n, extension_ref;
	char script[PATH_MAX];

	L = NULL;
	if (pthread_detach(pthread_self()))
		err(1, "extension: pthread_detach");
	if (verbose)
		printf("extension: initialising the %s extension\n", tag->name);

	if (pthread_setname_np(pthread_self(), "trxd-extension"))
		err(1, "extension: pthread_setname_np");
	/*
	 * Lock this transceivers mutex, so that no other thread accesses
	 * while we are initialising.
	 */
	if (pthread_mutex_lock(&tag->mutex))
		err(1, "extension: pthread_mutex_lock");
	if (verbose > 1)
		printf("extension: mutex locked\n");

	if (pthread_mutex_lock(&tag->mutex2))
		err(1, "extension: pthread_mutex_lock");
	if (verbose > 1)
		printf("extension: mutex2 locked\n");

	if (strchr(tag->script, '/'))
		err(1, "extension: script name must not contain slashes");

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL)
		err(1, "extension: luaL_newstate");

	pthread_cleanup_push(cleanup, L);

	luaL_openlibs(L);

	snprintf(script, sizeof(script), "%s/%s.lua", _PATH_EXTENSION,
	    tag->script);

	if (luaL_dofile(L, script))
		err(1, "extension: %s", lua_tostring(L, -1));
	if (lua_type(L, -1) != LUA_TTABLE)
		err(1, "extension: table expected");
	else
		extension_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	/*
	 * We are ready to go, unlock the mutex, so that client-handlers,
	 * trx-handlers, and, try-pollers can access it.
	 */
	if (pthread_mutex_unlock(&tag->mutex))
		err(1, "extension: pthread_mutex_unlock");
	if (verbose > 1)
		printf("extension: mutex unlocked\n");

	while (1) {
		int nargs = 1;

		/* Wait on cond, this releases the mutex */
		if (verbose > 1)
			printf("extension: wait for cond1\n");
		while (tag->handler == NULL) {
			if (pthread_cond_wait(&tag->cond1, &tag->mutex2))
				err(1, "extension: pthread_cond_wait");
			if (verbose > 1)
				printf("extension: cond1 changed\n");
		}

		if (verbose > 1) {
			printf("extension: request for %s", tag->handler);
			if (tag->data)
				printf(" with data '%s'\n", tag->data);
			printf("\n");
		}

		lua_geti(L, LUA_REGISTRYINDEX, extension_ref);
		lua_getfield(L, -1, tag->handler);
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			tag->reply = "command not supported, "
			    "please submit a bug report";
		} else {
			lua_pushstring(L, tag->data);

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
		}
		lua_pop(L, 2);
		tag->handler = NULL;

		if (pthread_cond_signal(&tag->cond2))
			err(1, "extension: pthread_cond_signal");
		if (verbose > 1)
			printf("extension: cond2 signaled\n");
	}
	pthread_cleanup_pop(0);
	return NULL;
}
