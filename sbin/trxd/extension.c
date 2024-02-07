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
	extension_tag_t *t = (extension_tag_t *)arg;

	lua_close(t->L);
	free(arg);
}

__thread extension_tag_t	*extension_tag;

void *
extension(void *arg)
{
	extension_tag_t *t = (extension_tag_t *)arg;

	if (pthread_detach(pthread_self()))
		err(1, "extension: pthread_detach");

	extension_tag = t;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "trxd-extension"))
		err(1, "extension: pthread_setname_np");

	if (t->is_callable) {
		if (pthread_mutex_lock(&t->mutex2))
			err(1, "extension: pthread_mutex_lock");

		if (t->has_config)
			lua_call(t->L, 1, 1);
		else
			lua_call(t->L, 0, 1);

		for (;;) {
			t->call = 0;
			/* Wait on cond, this releases the mutex */
			while (t->call == 0) {
				if (pthread_cond_wait(&t->cond1, &t->mutex2))
					err(1, "extension: pthread_cond_wait");
			}
			t->call = 0;
			switch (lua_pcall(t->L, 1, 1, 0)) {
			case LUA_OK:
				break;
			case LUA_ERRRUN:
			case LUA_ERRMEM:
			case LUA_ERRERR:
				syslog(LOG_ERR, "extension: Lua error: %s",
				    lua_tostring(t->L, -1));
				exit(1);
				break;
			}
			t->done = 1;
			if (pthread_cond_signal(&t->cond2))
				err(1, "extension: pthread_cond_signal");
		}
	} else {
		if (t->has_config)
			lua_call(t->L, 1, 1);
		else
			lua_call(t->L, 0, 1);
	}

	pthread_cleanup_pop(0);
	return NULL;
}
