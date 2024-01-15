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
extern void proxy_map(lua_State *, lua_State *, int);
extern void *trx_poller(void *);
extern void *trx_handler(void *);

extern destination_t *destination;
extern int verbose;

static void
call_trx_controller(dispatcher_tag_t *d, trx_controller_tag_t *t)
{
	printf("call trx controller\n");
	if (pthread_mutex_lock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	t->handler = "requestHandler";
	t->reply = NULL;
	t->data = d->data;

	if (pthread_mutex_lock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_lock");

	/* We signal cond, and mutex gets owned by trx-controller */
	if (pthread_cond_signal(&t->cond1))
		err(1, "dispatcher: pthread_cond_signal");

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");

	while (t->reply == NULL) {
		if (pthread_cond_wait(&t->cond2, &t->mutex2))
			err(1, "dispatcher: pthread_cond_wait");
	}

	if (strlen(t->reply) > 0) {
		d->sender->data = t->reply;
		if (pthread_cond_signal(&d->sender->cond))
			err(1, "dispatcher: pthread_cond_signal");
		pthread_mutex_unlock(&d->sender->mutex);
	} else {
		pthread_mutex_unlock(&d->sender->mutex);
	}

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");

	if (pthread_mutex_unlock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_unlock");
}

static void
call_gpio_controller(dispatcher_tag_t *d, gpio_controller_tag_t *t)
{
	printf("call gpio controller\n");
	if (pthread_mutex_lock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	t->handler = "requestHandler";
	t->reply = NULL;
	t->data = d->data;

	if (pthread_mutex_lock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_lock");

	/* We signal cond, and mutex gets owned by trx-controller */
	if (pthread_cond_signal(&t->cond1))
		err(1, "dispatcher: pthread_cond_signal");

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");

	while (t->reply == NULL) {
		if (pthread_cond_wait(&t->cond2, &t->mutex2))
			err(1, "dispatcher: pthread_cond_wait");
	}

	if (strlen(t->reply) > 0) {
		d->sender->data = t->reply;
		if (pthread_cond_signal(&d->sender->cond))
			err(1, "dispatcher: pthread_cond_signal");
		pthread_mutex_unlock(&d->sender->mutex);
	} else {
		pthread_mutex_unlock(&d->sender->mutex);
	}

	if (pthread_mutex_unlock(&t->mutex2))
		err(1, "dispatcher: pthread_mutex_unlock");

	if (pthread_mutex_unlock(&t->mutex))
		err(1, "dispatcher: pthread_mutex_unlock");
}

static void
destination_not_found(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Destination not found\"}";

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
destination_set(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	d->sender->data = "{\"status\":\"Ok\",\"message\":"
	    "\"Destination set\"}";

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
destination_not_supported(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Destination type not supported\"}";

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
request_not_supported(dispatcher_tag_t *d)
{
	/* The sender mutex is already locked */
	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Request not supported by extension\"}";

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
request_ok(dispatcher_tag_t *d)
{
	/* The sender mutex is already locked */
	d->sender->data = "{\"status\":\"Ok\",\"reply\":"
	    "\"Request handled\"}";

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
status_updates_not_supported(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Automatic status updated not supported by destination\"}";

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
listen_not_supported(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Listen not supported by destination\"}";

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
start_updater_if_not_running(trx_controller_tag_t *t)
{
	if (t->poller_required) {
		if (t->poller_running == 0) {
			t->poller_running = 1;
			pthread_create(&t->trx_poller, NULL, trx_poller, t);
		}
	} else {
		if (t->handler_running = 0) {
			t->handler_running = 1;
			lua_geti(t->L, LUA_REGISTRYINDEX, t->ref);
			lua_getfield(t->L, -1, "startStatusUpdates");
			lua_pcall(t->L, 0, 1, 0);
			t->handler_eol = lua_tointeger(t->L, -1);
			if (verbose > 1)
				printf("EOL character is %c\n", t->handler_eol);
			pthread_create(&t->trx_handler, NULL, trx_handler, t);
		}
	}
}

static void
add_sender(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;

	pthread_mutex_lock(&dst->tag.trx->mutex);

	if (dst->tag.trx->senders != NULL) {
		for (l = dst->tag.trx->senders; l; p = l, l = l->next)
			if (l->sender == d->sender)
				break;
		if (l == NULL) {
			p->next = malloc(sizeof(sender_list_t));
			if (p->next == NULL)
				err(1, "malloc");
			p = p->next;
			p->sender = d->sender;
			p->next = NULL;
			start_updater_if_not_running(dst->tag.trx);
		}
	} else {
		dst->tag.trx->senders = malloc(sizeof(sender_list_t));
		if (dst->tag.trx->senders == NULL)
			err(1, "malloc");
		dst->tag.trx->senders->sender = d->sender;
		dst->tag.trx->senders->next = NULL;
		start_updater_if_not_running(dst->tag.trx);
	}
	pthread_mutex_unlock(&dst->tag.trx->mutex);
}

static void
remove_sender(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;
	trx_controller_tag_t * t;
	int n;

	pthread_mutex_lock(&dst->tag.trx->mutex);

	for (l = dst->tag.trx->senders, p = NULL; l; p = l, l = l->next) {
		if (l->sender == d->sender) {
			if (p == NULL) {
				dst->tag.trx->senders = NULL;
				free(l);
				break;
			} else {
				p->next = l->next;
				free(l);
				break;
			}
		}
	}
	l = dst->tag.trx->senders;
	for (n = 0; l != NULL; ++n) {
		if (l->next == NULL) {
			++n;
			break;
		} else
			l = l->next;
	}

	if (n == 0) {
		t = dst->tag.trx;

		if (t->poller_running) {
			t->poller_running = 0;
			if (verbose > 1)
				printf("dispatcher: stopping the poller\n");
			pthread_cancel(t->trx_poller);
		} else if (dst->tag.trx->handler_running) {
			t->handler_running = 0;
			if (verbose > 1)
				printf("dispatcher: stopping the handler\n");
			lua_geti(t->L, LUA_REGISTRYINDEX, t->ref);
			lua_getfield(t->L, -1, "stopStatusUpdates");
			lua_pcall(t->L, 0, 1, 0);
			pthread_cancel(t->trx_handler);
		}
	}
	pthread_mutex_unlock(&dst->tag.trx->mutex);
}

static void
add_listener(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;

	pthread_mutex_lock(&dst->tag.extension->mutex);
	pthread_mutex_lock(&dst->tag.extension->mutex2);

	if (dst->tag.extension->listeners != NULL) {
		for (l = dst->tag.extension->listeners; l; p = l, l = l->next)
			if (l->sender == d->sender)
				break;
		if (l == NULL) {
			p->next = malloc(sizeof(sender_list_t));
			if (p->next == NULL)
				err(1, "malloc");
			p = p->next;
			p->sender = d->sender;
			p->next = NULL;
		}
	} else {
		dst->tag.extension->listeners = malloc(sizeof(sender_list_t));
		if (dst->tag.extension->listeners == NULL)
			err(1, "malloc");
		dst->tag.extension->listeners->sender = d->sender;
		dst->tag.extension->listeners->next = NULL;
	}
	pthread_mutex_unlock(&dst->tag.extension->mutex);
	pthread_mutex_unlock(&dst->tag.extension->mutex2);
}

static void
remove_listener(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;
	extension_tag_t * t;

	pthread_mutex_lock(&dst->tag.extension->mutex);
	pthread_mutex_lock(&dst->tag.extension->mutex2);

	for (l = dst->tag.extension->listeners, p = NULL; l;
	    p = l, l = l->next) {
		if (l->sender == d->sender) {
			if (p == NULL) {
				dst->tag.extension->listeners = NULL;
				free(l);
				break;
			} else {
				p->next = l->next;
				free(l);
				break;
			}
		}
	}
	pthread_mutex_unlock(&dst->tag.extension->mutex);
	pthread_mutex_unlock(&dst->tag.extension->mutex2);
}

static void
call_extension(lua_State *L, dispatcher_tag_t* d, extension_tag_t *e,
    const char *req)
{
	pthread_mutex_lock(&e->mutex);

	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	if (pthread_mutex_lock(&e->mutex2))
		err(1, "dispatcher: pthread_mutex_lock");

	e->done = 0;
	lua_getglobal(e->L, req);

	if (lua_type(e->L, -1) != LUA_TFUNCTION)
		request_not_supported(d);
	else {
		proxy_map(L, e->L, lua_gettop(e->L));
		e->call = 1;

		pthread_cond_signal(&e->cond1);
		if (pthread_mutex_unlock(&e->mutex2))
			err(1, "dispatcher: pthread_mutex_unlock");

		while (!e->done)
			pthread_cond_wait(&e->cond2, &e->mutex2);

		e->done = 0;

		lua_getglobal(L, "json");
		if (lua_type(L, -1) != LUA_TTABLE)
			errx(1, "dispatcher: table expected");
		lua_getfield(L, -1, "encode");
		if (lua_type(L, -1) != LUA_TFUNCTION)
			errx(1, "dispatcher: function expected");

		proxy_map(e->L, L, lua_gettop(L));

		switch (lua_pcall(L, 1, 1, 0)) {
		case LUA_OK:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			err(1, "dispatcher: %s", lua_tostring(L, -1));
			break;
		}
		if (lua_type(L, -1) != LUA_TSTRING)
			errx(1, "dispatcher: table does not encode to JSON ");

		d->sender->data = (char *)lua_tostring(L, -1);

		if (pthread_cond_signal(&d->sender->cond))
			err(1, "dispatcher: pthread_cond_signal");

		while (d->sender->data != NULL) {
			if (pthread_cond_wait(&d->sender->cond2,
			    &d->sender->mutex))
				err(1, "dispatcher: pthread_cond_wait");
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
	pthread_mutex_unlock(&e->mutex2);
	pthread_mutex_unlock(&e->mutex);
}

static void
dispatch(lua_State *L, dispatcher_tag_t *d, destination_t *to, const char *req)
{
	switch (to->type) {
	case DEST_TRX:
		call_trx_controller(d, to->tag.trx);
		break;
	case DEST_GPIO:
		call_gpio_controller(d, to->tag.gpio);
		break;
	case DEST_EXTENSION:
		call_extension(L, d, to->tag.extension, req);
		break;
	default:
		destination_not_supported(d);
	}
}

void
list_destination(dispatcher_tag_t *d)
{
	struct buffer buf;
	destination_t *dest;

	if (pthread_mutex_lock(&d->sender->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	buf_init(&buf);
	buf_addstring(&buf, "{\"status\":\"Ok\",\"reply\":\"list-destination\","
	    "\"destination\":[");
	for (dest = destination; dest != NULL; dest = dest->next) {
		if (dest != destination)
			buf_addchar(&buf, ',');
		buf_addstring(&buf, "{\"name\":\"");
		buf_addstring(&buf, dest->name);
		buf_addstring(&buf, "\",");

		buf_addstring(&buf, "\"type\":\"");
		switch (dest->type) {
		case DEST_TRX:
			buf_addstring(&buf, "transceiver");
			break;
		case DEST_ROTOR:
			buf_addstring(&buf, "rotor");
			break;
		case DEST_RELAY:
			buf_addstring(&buf, "relay");
			break;
		case DEST_GPIO:
			buf_addstring(&buf, "gpio");
			break;
		case DEST_EXTENSION:
			buf_addstring(&buf, "extension");
			break;
		}
		buf_addstring(&buf, "\"}");
	}
	buf_addstring(&buf, "]}");

	d->sender->data = buf.data;

	if (pthread_cond_signal(&d->sender->cond))
		err(1, "dispatcher: pthread_cond_signal");

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex))
			err(1, "dispatcher: pthread_cond_wait");
	}
	pthread_mutex_unlock(&d->sender->mutex);
	buf_free(&buf);
}

static void
cleanup(void *arg)
{
	dispatcher_tag_t *d = (dispatcher_tag_t *)arg;
	destination_t *dst;

	for (dst = destination; dst != NULL; dst = dst->next) {
		if (dst->type == DEST_TRX) {
			if (pthread_mutex_lock(&dst->tag.trx->mutex))
				err(1, "pthread_mutex_lock");
			remove_sender(d, dst);
			if (pthread_mutex_unlock(&dst->tag.trx->mutex))
				err(1, "pthread_mutex_unlock");
		}
	}
	free(arg);
}

static void
cleanup_lua(void *arg)
{
	lua_State *L = (lua_State *)arg;

	lua_close(L);
}

void *
dispatcher(void *arg)
{
	dispatcher_tag_t *d = (dispatcher_tag_t *)arg;
	trx_controller_tag_t *t;
	destination_t *to, *dst;
	lua_State *L;
	int status, request;
	const char *dest, *req;

	if (pthread_detach(pthread_self()))
		err(1, "dispatcher: pthread_detach");

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "trxd-dispatcher"))
		err(1, "dispatcher: pthread_setname_np");

	/* Check if have a default transceiver */
	for (to = destination; to != NULL; to = to->next)
		if (to->type == DEST_TRX && to->tag.trx->is_default)
			break;

	if (to == NULL)
		for (to = destination; to != NULL; to = to->next)
			if (to->type == DEST_TRX)
				break;

	if (to == NULL)
		 to = destination;

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL)
		err(1, "trx-controller: luaL_newstate");

	pthread_cleanup_push(cleanup_lua, L);

	luaL_openlibs(L);
	luaopen_json(L);
	lua_setglobal(L, "json");

	if (pthread_mutex_lock(&d->mutex))
		err(1, "dispatcher: pthread_mutex_lock");

	for (;;) {
		for (d->data = NULL; d->data == NULL; ) {
			if (pthread_cond_wait(&d->cond, &d->mutex))
				err(1, "dispatcher: pthread_cond_wait");
		}
		lua_getglobal(L, "json");
		if (lua_type(L, -1) != LUA_TTABLE)
			errx(1, "dispatcher: table expected");
		lua_getfield(L, -1, "decode");
		if (lua_type(L, -1) != LUA_TFUNCTION)
			errx(1, "dispatcher: function expected");
		lua_pushstring(L, d->data);
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
		dst = NULL;
		lua_getfield(L, request, "to");
		if (lua_type(L, -1) == LUA_TSTRING) {
			dest = lua_tostring(L, -1);

			for (dst = destination; dst != NULL; dst = dst->next) {
				if (!strcmp(dst->name, dest)) {
					to = dst;
					break;
				}
			}
		} else
			dst = to;

		lua_getfield(L, request, "request");
		req = lua_tostring(L, -1);
		lua_pop(L, 2);

		if (dst == NULL)
			destination_not_found(d);
		else {
			if (req && !strcmp(req, "start-status-updates")) {
				if (dst->type == DEST_TRX)
					add_sender(d, dst);
				else
					status_updates_not_supported(d);
			} else if (req && !strcmp(req, "stop-status-updates")) {
				if (dst->type == DEST_TRX)
					remove_sender(d, dst);
				else
					status_updates_not_supported(d);
			} else if (req && !strcmp(req, "listen")) {
				if (dst->type == DEST_EXTENSION) {
					add_listener(d, dst);
					request_ok(d);
				} else
					listen_not_supported(d);
			} else if (req && !strcmp(req, "unlisten")) {
				if (dst->type == DEST_EXTENSION) {
					remove_listener(d, dst);
					request_ok(d);
				} else
					listen_not_supported(d);
			} else if (req && !strcmp(req, "list-destination"))
				list_destination(d);
			else if (req) {
				dispatch(L, d, dst, req);
				/* XXX check stack depth */
				/* lua_pop(L, 4); */
			} else
				destination_set(d);
		}
		free(d->data);
	}
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	return NULL;
}
