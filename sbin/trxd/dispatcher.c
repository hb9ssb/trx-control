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

/* Dispatch incoming data from (web)socket-handlers */

/* XXX evaluate if this needs to be a thread on its own */

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "buffer.h"
#include "pathnames.h"
#include "trxd.h"
#include "trx-control.h"

extern int luaopen_json(lua_State *);
extern int luaopen_trxd(lua_State *);
extern void proxy_map(lua_State *, lua_State *, int);
extern void *trx_poller(void *);
extern void *trx_handler(void *);
extern void *extension(void *);

extern char *private_extensions;
extern destination_t *destination;
extern pthread_mutex_t destination_mutex;

extern int verbose;

extern __thread int cat_device;

static void
call_trx_controller(dispatcher_tag_t *d, trx_controller_tag_t *t)
{
	if (pthread_mutex_lock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	t->handler = "requestHandler";
	t->response = NULL;
	t->data = d->data;

	if (pthread_mutex_lock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	/* We signal cond, and mutex gets owned by trx-controller */
	if (pthread_cond_signal(&t->cond1)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	if (pthread_mutex_unlock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}

	while (t->response == NULL) {
		if (pthread_cond_wait(&t->cond2, &t->mutex2)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}

	if (strlen(t->response) > 0) {
		d->sender->data = t->response;
		if (pthread_cond_signal(&d->sender->cond)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
			exit(1);
		}
		pthread_mutex_unlock(&d->sender->mutex);
	} else {
		pthread_mutex_unlock(&d->sender->mutex);
	}

	if (pthread_mutex_unlock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}

	if (pthread_mutex_unlock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}
}

static void
call_sdr_controller(dispatcher_tag_t *d, sdr_controller_tag_t *t)
{
	if (pthread_mutex_lock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	t->handler = "requestHandler";
	t->response = NULL;
	t->data = d->data;

	if (pthread_mutex_lock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	/* We signal cond, and mutex gets owned by trx-controller */
	if (pthread_cond_signal(&t->cond1)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	if (pthread_mutex_unlock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}

	while (t->response == NULL) {
		if (pthread_cond_wait(&t->cond2, &t->mutex2)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}

	if (strlen(t->response) > 0) {
		d->sender->data = t->response;
		if (pthread_cond_signal(&d->sender->cond)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
			exit(1);
		}
		pthread_mutex_unlock(&d->sender->mutex);
	} else {
		pthread_mutex_unlock(&d->sender->mutex);
	}

	if (pthread_mutex_unlock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}

	if (pthread_mutex_unlock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}
}

static void
call_gpio_controller(dispatcher_tag_t *d, gpio_controller_tag_t *t)
{
	if (pthread_mutex_lock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	t->handler = "requestHandler";
	t->response = NULL;
	t->data = d->data;

	if (pthread_mutex_lock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	/* We signal cond, and mutex gets owned by trx-controller */
	if (pthread_cond_signal(&t->cond1)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	if (pthread_mutex_unlock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}

	while (t->response == NULL) {
		if (pthread_cond_wait(&t->cond2, &t->mutex2)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}

	if (strlen(t->response) > 0) {
		d->sender->data = t->response;
		if (pthread_cond_signal(&d->sender->cond)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
			exit(1);
		}
		pthread_mutex_unlock(&d->sender->mutex);
	} else {
		pthread_mutex_unlock(&d->sender->mutex);
	}

	if (pthread_mutex_unlock(&t->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}

	if (pthread_mutex_unlock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}
}

static void
call_nmea(dispatcher_tag_t *d, nmea_tag_t *t)
{
	struct buffer buf;

	buf_init(&buf);
	buf_addstring(&buf,
	    "{\"status\":\"Ok\",\"response\":\"get-fix\",\"from\":\"nmea\","
	    "\"fix\":{");

	if (pthread_mutex_lock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	buf_printf(&buf, "\"date\":\"%02d.%02d.%04d\",",
	    t->day, t->month, t->year > 0 ? t->year + 2000 : 0);
	buf_printf(&buf, "\"time\":\"%02d:%02d:%02d\",",
	    t->hour, t->minute, t->second);
	buf_printf(&buf, "\"status\":\"%d\",", t->status);
	buf_printf(&buf, "\"latitude\":\"%.6f\",", t->latitude);
	buf_printf(&buf, "\"longitude\":\"%.6f\",", t->longitude);
	buf_printf(&buf, "\"altitude\":\"%.2f\",", t->altitude);
	buf_printf(&buf, "\"variation\":\"%.4f\",", t->variation);
	buf_printf(&buf, "\"speed\":\"%.2f\",", t->speed);
	buf_printf(&buf, "\"course\":\"%.4f\",", t->course);
	buf_printf(&buf, "\"mode\":\"%c\",", t->mode);
	buf_printf(&buf, "\"locator\":\"%s\"", t->locator);

	if (pthread_mutex_unlock(&t->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
		exit(1);
	}

	buf_addstring(&buf, "}}");

	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	d->sender->data = buf.data;

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
	buf_free(&buf);
}

static void
destination_not_found(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Destination not found\"}";

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
destination_set(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	d->sender->data = "{\"status\":\"Ok\",\"message\":"
	    "\"Destination set\"}";

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
destination_not_supported(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Destination type not supported\"}";

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
request_not_supported(dispatcher_tag_t *d)
{
	/* The sender mutex is already locked */
	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Request not supported by extension\"}";

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
request_ok(dispatcher_tag_t *d)
{
	/* The sender mutex is already locked */
	d->sender->data = "{\"status\":\"Ok\",\"response\":"
	    "\"Request handled\"}";

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
status_updates_not_supported(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Automatic status updated not supported by destination\"}";

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
listen_not_supported(dispatcher_tag_t *d)
{
	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	d->sender->data = "{\"status\":\"Error\",\"reason\":"
	    "\"Listen not supported by destination\"}";

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
	}
	pthread_mutex_unlock(&d->sender->mutex);
}

static void
start_updater_if_not_running(dispatcher_tag_t *d, trx_controller_tag_t *t)
{
	if (t->poller_required) {
		if (t->poller_running == 0) {
			t->poller_running = 1;
			pthread_create(&t->trx_poller, NULL, trx_poller, t);
		}
	} else {
		if (t->handler_running == 0) {
			t->handler_running = 1;
			cat_device = t->cat_device;

			lua_getglobal(t->L, "_protocol");
			lua_getfield(t->L, -1, "startStatusUpdates");
			lua_pcall(t->L, 0, 1, 0);

			t->handler_eol = lua_tointeger(t->L, -1);
			if (verbose > 1)
				printf("EOL character: %02x\n", t->handler_eol);

			pthread_create(&t->trx_handler, NULL, trx_handler, t);
		}
	}
}

static void
add_sender(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;

	pthread_mutex_lock(&destination_mutex);
	pthread_mutex_lock(&dst->tag.trx->mutex);

	if (dst->tag.trx->senders != NULL) {
		for (l = dst->tag.trx->senders; l; p = l, l = l->next)
			if (l->sender == d->sender)
				break;
		if (l == NULL) {
			p->next = malloc(sizeof(sender_list_t));
			if (p->next == NULL) {
				syslog(LOG_ERR, "malloc");
				exit(1);
			}
			p = p->next;
			p->sender = d->sender;
			p->next = NULL;
			start_updater_if_not_running(d, dst->tag.trx);
		}
	} else {
		dst->tag.trx->senders = malloc(sizeof(sender_list_t));
		if (dst->tag.trx->senders == NULL) {
			syslog(LOG_ERR, "malloc");
			exit(1);
		}
		dst->tag.trx->senders->sender = d->sender;
		dst->tag.trx->senders->next = NULL;
		start_updater_if_not_running(d, dst->tag.trx);
	}
	pthread_mutex_unlock(&dst->tag.trx->mutex);
	pthread_mutex_unlock(&destination_mutex);
}

static void
remove_sender(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;
	trx_controller_tag_t *t;
	int n;

	pthread_mutex_lock(&destination_mutex);
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
		if (verbose > 1)
			printf("stopping status updates from trx\n");

		t = dst->tag.trx;

		if (t->poller_running) {
			t->poller_running = 0;
			if (verbose > 1)
				printf("dispatcher: stopping the poller\n");
			pthread_cancel(t->trx_poller);
		} else if (t->handler_running) {
			t->handler_running = 0;
			if (verbose > 1)
				printf("dispatcher: stopping the handler\n");

			lua_getglobal(t->L, "_protocol");
			lua_getfield(t->L, -1, "stopStatusUpdates");
			lua_pcall(t->L, 0, 1, 0);
		}
	}
	pthread_mutex_unlock(&dst->tag.trx->mutex);
	pthread_mutex_unlock(&destination_mutex);
}

static void
add_listener(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;

	pthread_mutex_lock(&destination_mutex);
	pthread_mutex_lock(&dst->tag.extension->mutex);
	pthread_mutex_lock(&dst->tag.extension->mutex2);

	if (dst->tag.extension->listeners != NULL) {
		for (l = dst->tag.extension->listeners; l; p = l, l = l->next)
			if (l->sender == d->sender)
				break;
		if (l == NULL) {
			p->next = malloc(sizeof(sender_list_t));
			if (p->next == NULL) {
				syslog(LOG_ERR, "malloc");
				exit(1);
		}
		p = p->next;
			p->sender = d->sender;
			p->next = NULL;
		}
	} else {
		dst->tag.extension->listeners = malloc(sizeof(sender_list_t));
		if (dst->tag.extension->listeners == NULL) {
			syslog(LOG_ERR, "malloc");
			exit(1);
		}
		dst->tag.extension->listeners->sender = d->sender;
		dst->tag.extension->listeners->next = NULL;
	}
	pthread_mutex_unlock(&dst->tag.extension->mutex);
	pthread_mutex_unlock(&dst->tag.extension->mutex2);
	pthread_mutex_unlock(&destination_mutex);
}

static void
remove_listener(dispatcher_tag_t *d, destination_t *dst)
{
	sender_list_t *p, *l;
	extension_tag_t * t;

	pthread_mutex_lock(&destination_mutex);
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
	pthread_mutex_unlock(&destination_mutex);
}

static void
call_extension(lua_State *L, dispatcher_tag_t* d, extension_tag_t *e,
    const char *req)
{
	pthread_mutex_lock(&e->mutex);

	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	if (pthread_mutex_lock(&e->mutex2)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	e->done = 0;
	lua_getglobal(e->L, req);

	if (lua_type(e->L, -1) != LUA_TFUNCTION)
		request_not_supported(d);
	else {
		proxy_map(L, e->L, lua_gettop(e->L));
		e->call = 1;

		pthread_cond_signal(&e->cond1);
		if (pthread_mutex_unlock(&e->mutex2)) {
			syslog(LOG_ERR, "dispatcher: pthread_mutex_unlock");
			exit(1);
		}

		while (!e->done)
			pthread_cond_wait(&e->cond2, &e->mutex2);

		e->done = 0;

		lua_getglobal(L, "json");
		if (lua_type(L, -1) != LUA_TTABLE) {
			syslog(LOG_ERR, "dispatcher: table expected");
			exit(1);
		}
		lua_getfield(L, -1, "encode");
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			syslog(LOG_ERR, "dispatcher: function expected");
			exit(1);
		}
		proxy_map(e->L, L, lua_gettop(L));

		switch (lua_pcall(L, 1, 1, 0)) {
		case LUA_OK:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			syslog(LOG_ERR, "dispatcher: %s", lua_tostring(L, -1));
			exit(1);
			break;
		}
		if (lua_type(L, -1) != LUA_TSTRING) {
			syslog(LOG_ERR,
			    "dispatcher: table does not encode to JSON ");
			exit(1);
		}

		d->sender->data = (char *)lua_tostring(L, -1);

		if (pthread_cond_signal(&d->sender->cond)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
			exit(1);
		}

		while (d->sender->data != NULL) {
			if (pthread_cond_wait(&d->sender->cond2,
			    &d->sender->mutex)) {
				syslog(LOG_ERR, "dispatcher: "
				    "pthread_cond_wait");
				exit(1);
			}
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
	case DEST_SDR:
		call_sdr_controller(d, to->tag.sdr);
		break;
	case DEST_GPIO:
		call_gpio_controller(d, to->tag.gpio);
		break;
	case DEST_INTERNAL:
		if (!strcmp(to->name, "nmea")) {
			if (!strcmp(req, "get-fix"))
				call_nmea(d, to->tag.nmea);
			else
				request_not_supported(d);
		} else
			destination_not_supported(d);
		break;
	case DEST_EXTENSION:
		call_extension(L, d, to->tag.extension, req);
		break;
	default:
		destination_not_supported(d);
	}
}

static const char *
type2name(int type)
{
	switch (type) {
	case DEST_TRX:
		return "transceiver";
		break;
	case DEST_SDR:
		return "sdr";
		break;
	case DEST_ROTOR:
		return "rotor";
		break;
	case DEST_RELAY:
		return "relay";
		break;
	case DEST_GPIO:
		return "gpio";
		break;
	case DEST_INTERNAL:
		return "internal";
		break;
	case DEST_EXTENSION:
		return "extension";
		break;
	default:
		return "unknown";
	}
}

void
list_destination(dispatcher_tag_t *d, const char *type)
{
	struct buffer buf;
	destination_t *dest;
	int first;

	if (pthread_mutex_lock(&d->sender->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	buf_init(&buf);
	buf_addstring(&buf,
	    "{\"status\":\"Ok\",\"response\":\"list-destination\",");
	if (type)
		buf_printf(&buf, "\"type\":\"%s\",", type);
	buf_addstring(&buf, "\"destination\":[");

	pthread_mutex_lock(&destination_mutex);
	for (first = 1, dest = destination; dest != NULL; dest = dest->next) {
		if (type && strcmp(type, type2name(dest->type)))
			continue;

		if (!first)
			buf_addchar(&buf, ',');
		else
			first = 0;

		buf_addstring(&buf, "{\"name\":\"");
		buf_addstring(&buf, dest->name);
		buf_addstring(&buf, "\",");

		buf_addstring(&buf, "\"type\":\"");
		buf_addstring(&buf, type2name(dest->type));
		buf_addchar(&buf, '"');

		if (dest->type == DEST_TRX) {
			if (dest->tag.trx->is_default)
				buf_addstring(&buf, ",\"default\":true");
			if (dest->tag.trx->audio_input)
				buf_printf(&buf, ",\"audioIn\":\"%s\"",
				    dest->tag.trx->audio_input);
			if (dest->tag.trx->audio_output)
				buf_printf(&buf, ",\"audioOut\":\"%s\"",
				    dest->tag.trx->audio_output);
		}

		buf_addchar(&buf, '}');
	}
	pthread_mutex_unlock(&destination_mutex);

	buf_addstring(&buf, "]}");

	d->sender->data = buf.data;

	if (pthread_cond_signal(&d->sender->cond)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	while (d->sender->data != NULL) {
		if (pthread_cond_wait(&d->sender->cond2, &d->sender->mutex)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
			exit(1);
		}
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
		switch (dst->type) {
		case DEST_TRX:
			remove_sender(d, dst);
			break;
		case DEST_EXTENSION:
			remove_listener(d, dst);
			break;
		default:
			break;
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

static void
initialize_private_extensions(lua_State *L)
{
	int top;

	/* The tabe with the extensions is a the Lua stack top */
	top = lua_gettop(L);
	lua_pushnil(L);
	while (lua_next(L, top)) {
		extension_tag_t *t;
		const char *p;
		char script[PATH_MAX], *name;

		t = malloc(sizeof(extension_tag_t));
		if (t == NULL) {
			syslog(LOG_ERR, "memory allocation failure");
			exit(1);
		}
		t->has_config = 0;
		t->listeners = NULL;
		t->L = luaL_newstate();
		if (t->L == NULL) {
			syslog(LOG_ERR, "cannot create Lua state");
			exit(1);
		}
		luaL_openlibs(t->L);
		luaopen_trxd(t->L);
		lua_setglobal(t->L, "trxd");
		luaopen_json(t->L);
		lua_setglobal(t->L, "json");

		t->call = t->done = 0;
		name = (char *)lua_tostring(L, -2);

		lua_getglobal(t->L, "package");
		lua_getfield(t->L, -1, "cpath");
		lua_pushstring(t->L, ";");
		lua_pushstring(t->L, _PATH_LUA_CPATH);
		lua_concat(t->L, 3);
		lua_setfield(t->L, -2, "cpath");
		lua_pop(t->L, 1);

		lua_getglobal(t->L, "package");
		lua_getfield(t->L, -1, "path");
		lua_pushstring(t->L, ";");
		lua_pushstring(t->L, _PATH_LUA_PATH);
		lua_concat(t->L, 3);
		lua_setfield(t->L, -2, "path");
		lua_pop(t->L, 1);

		lua_getfield(L, -1, "path");
		if (lua_isstring(L, -1)) {
			p = lua_tostring(L, -1);
			lua_getglobal(t->L, "package");
			lua_getfield(t->L, -1, "path");
			lua_pushstring(t->L, ";");
			lua_pushstring(t->L, p);
			lua_concat(t->L, 3);
			lua_setfield(t->L, -2, "path");
			lua_pop(t->L, 1);
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "cpath");
		if (lua_isstring(L, -1)) {
			p = lua_tostring(L, -1);
			lua_getglobal(t->L, "package");
			lua_getfield(t->L, -1, "cpath");
			lua_pushstring(t->L, ";");
			lua_pushstring(t->L, p);
			lua_concat(t->L, 3);
			lua_setfield(t->L, -2, "cpath");
			lua_pop(t->L, 1);
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "script");
		if (!lua_isstring(L, -1)) {
			syslog(LOG_ERR,
			    "missing extension script name");
			exit(1);
		}
		p = lua_tostring(L, -1);

		if (strchr(p, '/')) {
			syslog(LOG_ERR,
			    "script name must not contain slashes");
			exit(1);
		}
		snprintf(script, sizeof(script), "%s/%s.lua",
		    _PATH_EXTENSION, p);

		lua_pop(L, 1);

		if (luaL_loadfile(t->L, script)) {
			syslog(LOG_ERR, "%s", lua_tostring(t->L, -1));
			exit(1);
		}

		lua_getfield(L, -1, "configuration");
		if (lua_istable(L, -1)) {
			proxy_map(L, t->L, lua_gettop(t->L));
			t->has_config = 1;
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "callable");
		if (lua_isboolean(L, -1))
			t->is_callable = lua_toboolean(L, -1);
		else
			t->is_callable = 1;
		lua_pop(L, 1);

#if 0
		if (add_destination(name, DEST_EXTENSION, t)) {
			syslog(LOG_ERR, "names must be unique");
			exit(1);
		}
#endif
		if (pthread_mutex_init(&t->mutex, NULL))
			goto terminate;

		if (pthread_mutex_init(&t->mutex2, NULL))
			goto terminate;

		if (pthread_cond_init(&t->cond1, NULL))
			goto terminate;

		if (pthread_cond_init(&t->cond2, NULL))
			goto terminate;

		/* Create the extension thread */
		pthread_create(&t->extension, NULL, extension, t);
		lua_pop(L, 1);
	}
terminate:
	lua_pop(L, 1);
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

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "dispatcher: pthread_detach");
		exit(1);
	}

	pthread_cleanup_push(cleanup, arg);

	if (pthread_setname_np(pthread_self(), "dispatcher")) {
		syslog(LOG_ERR, "dispatcher: pthread_setname_np");
		exit(1);
	}

	/* Check if we have a default transceiver */
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
	if (L == NULL) {
		syslog(LOG_ERR, "trx-controller: luaL_newstate");
		exit(1);
	}

	pthread_cleanup_push(cleanup_lua, L);

	luaL_openlibs(L);
	luaopen_json(L);
	lua_setglobal(L, "json");

	/* Initialize private extensions */
	if (private_extensions) {
		printf("initialize private extensions from:\n%s\n", private_extensions);
		lua_getglobal(L, "json");
		lua_getfield(L, -1, "decode");
		lua_pushstring(L, private_extensions);
		if (lua_istable(L, -1)) {
			switch (lua_pcall(L, 1, 1, 0)) {
			case LUA_OK:
				/* Initialize the extensions */
				initialize_private_extensions(L);
				lua_pop(L, 1);
				break;
			case LUA_ERRRUN:
			case LUA_ERRMEM:
			case LUA_ERRERR:
				syslog(LOG_ERR, "%s", lua_tostring(L, -1));
				exit(1);
				break;
			}
		}
		lua_pop(L, 2);
	}

	if (pthread_mutex_lock(&d->mutex)) {
		syslog(LOG_ERR, "dispatcher: pthread_mutex_lock");
		exit(1);
	}

	if (verbose)
		printf("dispatcher: ready\n");

	d->data = NULL;
	if (pthread_cond_signal(&d->cond2)) {
		syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
		exit(1);
	}

	for (;;) {
		while (d->data == NULL) {
			if (pthread_cond_wait(&d->cond, &d->mutex)) {
				syslog(LOG_ERR, "dispatcher: pthread_cond_wait");
				exit(1);
			}
		}
		lua_getglobal(L, "json");
		if (lua_type(L, -1) != LUA_TTABLE) {
			syslog(LOG_ERR, "dispatcher: table expected");
			exit(1);
		}
		lua_getfield(L, -1, "decode");
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			syslog(LOG_ERR, "dispatcher: function expected");
			exit(1);
		}

		lua_pushstring(L, d->data);
		switch (lua_pcall(L, 1, 1, 0)) {
		case LUA_OK:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			syslog(LOG_ERR, "dispatcher: %s", lua_tostring(L, -1));
			exit(1);
			break;
		}
		if (lua_type(L, -1) != LUA_TTABLE) {
			syslog(LOG_ERR, "dispatcher: "
			    "JSON does not decode to table. Skipping request.");
			goto skip_request;
		}
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
				if (dst->type == DEST_TRX) {
					add_sender(d, dst);
					request_ok(d);
				} else
					status_updates_not_supported(d);
			} else if (req && !strcmp(req, "stop-status-updates")) {
				if (dst->type == DEST_TRX) {
					remove_sender(d, dst);
					request_ok(d);
				} else
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
			} else if (req && !strcmp(req, "list-destination")) {
				const char *type = NULL;
				lua_getfield(L, request, "type");
				type = lua_tostring(L, -1);
				lua_pop(L, 2);
				list_destination(d, type);
			} else if (req) {
				dispatch(L, d, dst, req);
				/* XXX check stack depth */
				/* lua_pop(L, 4); */
			} else
				destination_set(d);
		}
skip_request:
		free(d->data);
		d->data = NULL;
		if (pthread_cond_signal(&d->cond2)) {
			syslog(LOG_ERR, "dispatcher: pthread_cond_signal");
			exit(1);
		}
	}
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	return NULL;
}
