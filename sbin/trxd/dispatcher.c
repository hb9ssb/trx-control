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

/* Dispatch incoming data from (web)socket-handlers */

/* XXX evaluate if this needs to be a thread on its own */

#include <err.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "buffer.h"
#include "trxd.h"
#include "trx-control.h"

extern int luaopen_json(lua_State *);

extern trx_controller_tag_t *trx_controller_tag;
extern destination_t *destination;
extern int verbose;

static void
cleanup(void *arg)
{
	if (verbose > 1)
		printf("dispatcher: cleanup\n");
	free(arg);
}

void
call_trx_controller(dispatcher_tag_t *d)
{
	trx_controller_tag_t *t = d->trx_controller;

	if (pthread_mutex_lock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: mutex locked\n");

	if (pthread_mutex_lock(&t->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: sender mutex locked\n");

	t->handler = "requestHandler";
	t->reply = NULL;
	t->data = d->data;
	t->new_tag = t;

	if (pthread_mutex_lock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: mutex2 locked\n");

	/* We signal cond, and mutex gets owned by trx-controller */
	if (pthread_cond_signal(&t->cond1))
		err(1, "dispatcher: pthread_cond_signal");
	if (verbose > 1)
		printf("dispatcher: cond1 signaled\n");

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");
	if (verbose > 1)
		printf("dispatcher: mutex unlocked\n");

	while (t->reply == NULL) {
		if (pthread_cond_wait(&t->cond2, &t->mutex2))
			err(1, "dispatcher: pthread_cond_wait");
		if (verbose > 1)
			printf("dispatcher: cond2 changed\n");
	}

	free(d->data);
	if (strlen(t->reply) > 0 && !d->terminate) {
		printf("calling the sender thread\n");
		t->sender->data = t->reply;
		if (pthread_cond_signal(&t->sender->cond))
			err(1, "dispatcher: pthread_cond_signal");
		pthread_mutex_unlock(&t->sender->mutex);
	} else {
		printf("not calling the sender thread\n");
		pthread_mutex_unlock(&t->sender->mutex);
	}

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");
	if (verbose > 1)
		printf("dispatcher: mutex2 unlocked\n");

	if (pthread_mutex_unlock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_unlock");
	if (verbose > 1)
		printf("dispatcher: mutex unlocked\n");

	/* Check if we changed the transceiver, keep the sender! */
	if (t->new_tag != t) {
		t->new_tag->sender = t->sender;
		d->trx_controller = t->new_tag;
	}
}

void
dispatch(lua_State *L, dispatcher_tag_t *d)
{
	call_trx_controller(d);
}

void
list_trx(dispatcher_tag_t *d)
{
	struct buffer buf;
	trx_controller_tag_t *t = d->trx_controller;

	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	buf_init(&buf);
	buf_addstring(&buf, "{\"status\":\"Ok\",\"reply\":\"list-trx\","
	    "\"data\":[");
	for (t = trx_controller_tag; t != NULL; t = t->next) {
		if (t != trx_controller_tag)
			buf_addchar(&buf, ',');
		buf_addstring(&buf, "{\"driver\":\"");
		buf_addstring(&buf, t->driver);
		buf_addstring(&buf, "\",");

		buf_addstring(&buf, "\"name\":\"");
		buf_addstring(&buf, t->name);
		buf_addstring(&buf, "\",");

		buf_addstring(&buf, "\"device\":\"");
		buf_addstring(&buf, t->device);
		buf_addstring(&buf, "\"}");
	}
	buf_addstring(&buf, "]}");

	d->sender->data = buf.data;

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");
	pthread_mutex_unlock(&d->sender->mutex);
	/* XXX only free after data has been sent out */
	buf_free(&buf);
}

void *
dispatcher(void *arg)
{
	dispatcher_tag_t *d = (dispatcher_tag_t *)arg;
	trx_controller_tag_t *t;
	lua_State *L;
	int status, nread, n, terminate, request;
	char *buf, *p;
	const char *command, *param, *dest, *req;

	pthread_cleanup_push(cleanup, arg);

	if (pthread_detach(pthread_self()))
		err(1, "dispatcher: pthread_detach");

	if (pthread_setname_np(pthread_self(), "trxd-dispatcher"))
		err(1, "dispatcher: pthread_setname_np");

	/* Check if have a default transceiver */
	for (t = trx_controller_tag; t != NULL; t = t->next)
		if (t->is_default)
			break;

	/* If there is no default transceiver, use the first one */
	if (t == NULL)
		t = trx_controller_tag;

	t->sender = d->sender;
	d->trx_controller = t;

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL)
		err(1, "trx-controller: luaL_newstate");

	luaL_openlibs(L);
	luaopen_json(L);
	lua_setglobal(L, "json");

	if (pthread_mutex_lock(&d->mutex))
		err(1, "dispatcher: pthread_mutex_lock");
	if (verbose > 1)
		printf("dispatcher: mutex locked\n");

	for (terminate = 0; !terminate ;) {
		printf("dispatcher: wait for condition to change\n");
		for (d->data = NULL; d->data == NULL; ) {
			if (pthread_cond_wait(&d->cond, &d->mutex))
				err(1, "dispatcher: pthread_cond_wait");
			if (verbose > 1)
				printf("dispatcher: cond changed\n");
		}
		lua_getglobal(L, "json");
		if (lua_type(L, -1) != LUA_TTABLE)
			errx(1, "dispatcher: table expected");
		lua_getfield(L, -1, "decode");
		if (lua_type(L, -1) != LUA_TFUNCTION)
			errx(1, "dispatcher: function expected");
		lua_pushstring(L, d->data);
		printf("call json decode\n");
		switch (lua_pcall(L, 1, 1, 0)) {
		case LUA_OK:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			err(1, "dispatcher: %s", lua_tostring(L, -1));
			break;
		}
		if (lua_type(L, -1) != LUA_TTABLE)
			errx(1, "dispatcher: JSON does not decode to table");

		/* decoded JSON data is now on top of the stack */
		request = lua_gettop(L);
		dest = NULL;
		lua_getfield(L, request, "destination");
		if (lua_type(L, -1) == LUA_TSTRING)
			dest = lua_tostring(L, 1);
		lua_getfield(L, request, "request");
		if (lua_type(L, -1) == LUA_TSTRING)
			printf("and the request is: %s\n", lua_tostring(L, -1));
		req = lua_tostring(L, -1);
		lua_pop(L, 2);

		if (!strcmp(req, "list-trx"))
			list_trx(d);
		else
			dispatch(L, d);
		lua_pop(L, 2);
	}
	pthread_cleanup_pop(0);
	return NULL;
}
