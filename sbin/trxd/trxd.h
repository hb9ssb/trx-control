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

#ifndef __TRXD_H__
#define __TRXD_H__

#include <pthread.h>

#include <openssl/ssl.h>

#include <lua.h>
#include <lualib.h>

#define TRXD_USER	"trxd"
#define TRXD_GROUP	"trxd"

typedef struct sender_tag sender_tag_t;

typedef struct sender_list {
	sender_tag_t		*sender;
	struct sender_list	*next;
} sender_list_t;

typedef struct trx_controller_tag {
	/* The first mutex locks the trx-controller */
	pthread_mutex_t		 mutex;

	pthread_mutex_t		 mutex2;
	pthread_cond_t		 cond1;	/* A handler is set */
	const char		*handler;

	pthread_cond_t		 cond2;	/* A response is set */
	char			*response;

	char			*name;
	const char		*device;
	int			 speed;		/* For serial devices */
	int			 channel;	/* For RFCOMM devices */
	char			*audio_input;
	char			*audio_output;
	const char		*trx;		/* trx description (YAML) */
	int			 is_default;

	lua_State		*L;
	int			 ref;

	char			*data;

	int			 client_fd;
	int			 cat_device;
	pthread_t		 trx_controller;
	pthread_t		 trx_poller;
	pthread_t		 trx_handler;
	int			 is_running;
	int			 poller_required;
	int			 poller_running;
	int			 poller_suspended;
	int			 handler_running;
	int			 handler_eol;

	sender_list_t		*senders;
} trx_controller_tag_t;

typedef struct sdr_controller_tag {
	/* The first mutex locks the sdr-controller */
	pthread_mutex_t		 mutex;

	pthread_mutex_t		 mutex2;
	pthread_cond_t		 cond1;	/* A handler is set */
	const char		*handler;

	pthread_cond_t		 cond2;	/* A response is set */
	char			*response;

	char			*name;
	const char		*device;
	int			 speed;		/* For serial devices */
	int			 channel;	/* For RFCOMM devices */
	const char		*driver;
	int			 is_default;

	lua_State		*L;
	int			 ref;

	char			*data;

	int			 client_fd;
	int			 cat_device;
	pthread_t		 sdr_controller;
	pthread_t		 sdr_handler;
	int			 is_running;
	int			 handler_running;
	int			 handler_eol;

	sender_list_t		*senders;
} sdr_controller_tag_t;

#define LOCATORMAX		6

typedef struct nmea_tag {
	pthread_mutex_t		 mutex;

	int			 fd;
	pthread_t		 nmea_handler;

	/* Fix data */
	int			year, month, day, hour, minute, second;
	int			status;		/* signal status */
	double			latitude;
	double			longitude;
	double			altitude;
	double			speed;
	double			course;
	double			variation;	/* Magnetic variation */
	char			mode;		/* GPS mode */
	char			locator[LOCATORMAX + 1];
} nmea_tag_t;

typedef struct gpio_controller_tag {
	/* The first mutex locks the gpio-controller */
	pthread_mutex_t		 mutex;

	pthread_mutex_t		 mutex2;
	pthread_cond_t		 cond1;	/* A handler is set */
	const char		*handler;

	pthread_cond_t		 cond2;	/* A response is set */
	char			*response;

	char			*name;
	const char		*device;
	int			 speed;
	const char		*driver;

	lua_State		*L;
	int			 ref;

	char			*data;

	int			 gpio_device;
	pthread_t		 gpio_controller;
	pthread_t		 gpio_poller;
	pthread_t		 gpio_handler;
	int			 is_running;
	int			 poller_required;
	int			 poller_running;
	int			 poller_suspended;
	int			 handler_running;

	sender_list_t		*senders;
} gpio_controller_tag_t;

typedef struct relay_controller_tag {
	/* The first mutex locks the relay-controller */
	pthread_mutex_t		 mutex;

	pthread_mutex_t		 mutex2;
	pthread_cond_t		 cond1;	/* A handler is set */
	const char		*handler;

	pthread_cond_t		 cond2;	/* A response is set */
	char			*response;

	char			*name;
	const char		*device;
	const char		*driver;
	int			 is_default;

	char			*data;

	pthread_t		 relay_controller;
	pthread_t		 relay_poller;
	pthread_t		 relay_handler;
	int			 is_running;
	int			 poller_running;
	int			 poller_suspended;
	int			 handler_running;
} relay_controller_tag_t;

typedef struct extension_tag {
	/* The first mutex locks the extension */
	pthread_mutex_t		 mutex;
	pthread_mutex_t		 mutex2;

	pthread_cond_t		 cond1;	/* Call the extension */
	int			 call;

	pthread_cond_t		 cond2;	/* The extension returned */
	int			 done;
	int			 has_config;
	int			 is_callable;

	lua_State		*L;

	pthread_t		 extension;

	sender_list_t		*listeners;
} extension_tag_t;

enum DestinationType {
	DEST_TRX,
	DEST_SDR,
	DEST_ROTOR,
	DEST_RELAY,
	DEST_GPIO,
	DEST_INTERNAL,
	DEST_EXTENSION
};

typedef struct destination {
	char			*name;
	enum DestinationType	 type;

	union {
		trx_controller_tag_t	*trx;
		sdr_controller_tag_t	*sdr;
		gpio_controller_tag_t	*gpio;
		nmea_tag_t		*nmea;
		relay_controller_tag_t	*relay;
		extension_tag_t		*extension;
	} tag;

	struct destination	*previous;
	struct destination	*next;
} destination_t;

typedef struct signal_input {
	extension_tag_t	*extension;
	int		 fd;
	const char	*func;
	pthread_t	 signal_input;
} signal_input_t;

typedef struct websocket_listener {
	char			*bind_addr;
	char			*listen_port;
	char			*path;
	char			*root;
	char			*certificate;
	char			*key;
	int			 announce;

	int			 socket;

	/* For secure websockets */
	SSL_CTX			*ctx;
	SSL			*ssl;

	pthread_t		 listener;
	pthread_t		 announcer;
} websocket_listener_t;

typedef struct websocket {
	int			 socket;

	/* For secure websockets */
	SSL_CTX			*ctx;
	SSL			*ssl;

	sender_tag_t		*sender;
	pthread_t		 listen_thread;
} websocket_t;

/*
 * A dispatcher thread is needed per client.  It receives the request from
 * a socket-handler or websocket-handler and dispatches it to the right
 * controller (or extension).
 */
typedef struct dispatcher_tag {
	/* The first mutex locks the dispatcher */
	pthread_mutex_t		 mutex;
	pthread_mutex_t		 mutex2;
	pthread_cond_t		 cond;	/* data is ready to be dispatched */
	pthread_cond_t		 cond2;	/* Request has been handled */

	char			*data;

	sender_tag_t		*sender;
	pthread_t		 dispatcher;
} dispatcher_tag_t;

/*
 * A sender thread is needed per client, so that i/o problems do
 * do not lock a controller.  Also some protocols, notably websockets,
 * do not just regular i/o, but need some framing.  The specific
 * sender thread can deal with this.
 */
typedef struct sender_tag {
	/* The first mutex locks the sender */
	pthread_mutex_t		 mutex;
	pthread_mutex_t		 mutex2;
	pthread_cond_t		 cond;	/* data is ready to be send set */
	pthread_cond_t		 cond2;	/* data has been sent */
	char			*data;

	int			 socket;

	/* For secure sockets */
	SSL_CTX			*ctx;
	SSL			*ssl;

	pthread_t		 sender;
} sender_tag_t;

#endif /* __TRXD_H__ */
