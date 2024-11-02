/*
 * Copyright (c) 2024 Marc Balmer HB9SSB
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

/* Handle Avahi */

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

#include "trxd.h"
#include "trx-control.h"

extern int verbose;

static AvahiEntryGroup *group = NULL;
static AvahiSimplePoll *simple_poll = NULL;

static char *name = NULL;

static void create_services(AvahiClient *, websocket_listener_t *);

static void
entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state,
    void *userdata)
{
	websocket_listener_t *w = (websocket_listener_t *)userdata;

	assert(g == group || group == NULL);
	group = g;

	/* Called whenever the entry group state changes */
	switch (state) {
		case AVAHI_ENTRY_GROUP_ESTABLISHED:

		/* The entry group has been established successfully */
		if (verbose)
			syslog(LOG_INFO,
			    "avahi-handler: service '%s' established.\n",
			    name);
		break;
	case AVAHI_ENTRY_GROUP_COLLISION: {
		char *n;
		/*
		 * A service name collision with a remote service
		 * happened. Let's pick a new name
		 */
		n = avahi_alternative_service_name(name);
		avahi_free(name);
		name = n;
		if (verbose)
			syslog(LOG_INFO, "avahi-hahndler: service name "
			    "collision, renaming service to '%s'\n", name);

		/* And recreate the services */
		create_services(avahi_entry_group_get_client(g), w);
		break;
	}
	case AVAHI_ENTRY_GROUP_FAILURE:
		syslog(LOG_ERR, "avahi-handler: entry group failure: %s\n",
		    avahi_strerror(
		    avahi_client_errno(avahi_entry_group_get_client(g))));

		/*
		 * Some kind of failure happened while we were registering
		 * our services
		 */
		avahi_simple_poll_quit(simple_poll);
		break;
	case AVAHI_ENTRY_GROUP_UNCOMMITED:
	case AVAHI_ENTRY_GROUP_REGISTERING:
		;
	}
}

static void
create_services(AvahiClient *c, websocket_listener_t *w)
{
	char *n, path[128], ssl[128];
	int ret;
	assert(c);

	/*
	 * If this is the first time we're called, let's create a new
	 * entry group if necessary
	 */
	if (!group)
		if (!(group = avahi_entry_group_new(c,
		    entry_group_callback, w))) {
			syslog(LOG_ERR, "avahi-handler: "
			    "avahi_entry_group_new() failed: %s\n",
			    avahi_strerror(avahi_client_errno(c)));
			goto fail;
		}

	/*
	 * If the group is empty (either because it was just created, or
	 * because it was reset previously, add our entries.
	 */
	if (avahi_entry_group_is_empty(group)) {
		if (verbose)
			syslog(LOG_INFO,
			    "avahi-handler: adding service '%s'\n", name);

		/* Create TXT data */
		snprintf(path, sizeof(path), "path=%s", w->path);

		/*
		 * We will now add two services and one subtype to the entry
		 * group. The two services have the same name, but differ in
		 * the service type (IPP vs. BSD LPR). Only services with the
		 * same name should be put in the same entry group.
		 */

		/* Add the service for trx-control */
		if ((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC,
		    AVAHI_PROTO_UNSPEC, 0, name, "_trx-control._tcp", NULL,
		    NULL, atoi(w->listen_port),
		    path, w->certificate ? "ssl=true" : NULL, NULL)) < 0) {
			if (ret == AVAHI_ERR_COLLISION)
				goto collision;
			syslog(LOG_ERR, "avahi-handler: failed to add "
			    "_trx-control._tcp service: %s\n",
			    avahi_strerror(ret));
			goto fail;
		}

		/* Tell the server to register the service */
		if ((ret = avahi_entry_group_commit(group)) < 0) {
			syslog(LOG_ERR, "avahi-handler: failed to commit entry"
			    " group: %s\n", avahi_strerror(ret));
			goto fail;
		}
	}
	return;

collision:
	/*
	 * A service name collision with a local service happened.
	 * Pick a new name.
	 */
	n = avahi_alternative_service_name(name);
	avahi_free(name);
	name = n;
	syslog(LOG_INFO, "avahi-handler: service name collision, renaming "
	    "service to '%s'\n", name);
	avahi_entry_group_reset(group);
	create_services(c, w);
	return;

fail:
	avahi_simple_poll_quit(simple_poll);
}

static void
client_callback(AvahiClient *c, AvahiClientState state, void *userdata)
{
	websocket_listener_t *w = (websocket_listener_t *)userdata;

	/* Called whenever the client or server state changes. */
	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
		/*
		 * The server has startup successfully and registered its host
		 * name on the network, so it's time to create our services.
		 */
		create_services(c, w);
		break;
	case AVAHI_CLIENT_FAILURE:
		syslog(LOG_ERR, "avahi-handler: client failure: %s\n",
		    avahi_strerror(avahi_client_errno(c)));
		avahi_simple_poll_quit(simple_poll);
		break;
	case AVAHI_CLIENT_S_COLLISION:
		/*
		 * Let's drop our registered services. When the server is back
		 * in AVAHI_SERVER_RUNNING state, we will register them
		 * again with the new host name.
		 */
	case AVAHI_CLIENT_S_REGISTERING:
		/*
		 * The server records are now being established. This
		 * might be caused by a host name change. We need to wait
		 * for our own records to register until the host name is
		 * properly esatblished.
		 */
		if (group)
			avahi_entry_group_reset(group);
		break;
	case AVAHI_CLIENT_CONNECTING:
		;
	}
}

static void
cleanup_client(void *arg)
{
	AvahiClient *client = (AvahiClient *)arg;

	if (verbose)
		syslog(LOG_NOTICE, "avahi-handler: terminating\n");

	avahi_client_free(client);
	avahi_free(name);
}

static void
cleanup_simple_poll(void *arg)
{
	AvahiSimplePoll *simple_poll = (AvahiSimplePoll *)arg;

	if (verbose)
		syslog(LOG_NOTICE, "avahi-handler: terminate simple_poll\n");

	avahi_simple_poll_free(simple_poll);
}

void *
avahi_handler(void *arg)
{
	websocket_listener_t *w = (websocket_listener_t *)arg;
	char hostname[128];
	AvahiClient *client = NULL;
	int error;

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "avahi-handler: pthread_detach");
		exit(1);
	}

	/* Allocate main loop object */
	if (!(simple_poll = avahi_simple_poll_new())) {
		syslog(LOG_ERR, "avahi-handler: failed to create simple poll "
		    "object\n");
		exit(1);
	}

	if (!gethostname(hostname, sizeof(hostname)))
		asprintf(&name, "trx-control (trxd) on %s", hostname);
	else
		name = avahi_strdup("trx-control (trxd)");

	client = avahi_client_new(avahi_simple_poll_get(simple_poll), 0,
	    client_callback, w, &error);

	if (!client) {
		syslog(LOG_ERR, "avahi-handler: failed to create client: %s",
		    avahi_strerror(error));
		exit(1);
	}
	pthread_cleanup_push(cleanup_client, client);
	pthread_cleanup_push(cleanup_simple_poll, simple_poll);

	if (pthread_setname_np(pthread_self(), "avahi")) {
		syslog(LOG_ERR,"avahi-handler: pthread_setname_np");
		exit(1);
	}

	avahi_simple_poll_loop(simple_poll);

	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	return NULL;
}
