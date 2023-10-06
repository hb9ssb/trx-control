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

#ifndef __TRXD_H__
#define __TRXD_H__

#include <pthread.h>

#define TRXD_VERSION	"1.0.0"
#define TRXD_USER	"trxd"
#define TRXD_GROUP	"trxd"

#define SWITCH_TAG	"switch-tag:"

typedef struct command_tag {
	/* The first mutex locks the trx-controller */
	pthread_mutex_t		 mutex;
	pthread_cond_t		 cond;	/* A handler is set */
	const char		*handler;

	pthread_mutex_t		 mutex2;
	pthread_cond_t		 cond2;	/* A reply is set */
	char			*reply;

	const char		*name;
	const char		*device;
	const char		*driver;
	int			 is_default;

	char			*data;

	int			 client_fd;
	int			 cat_device;
	pthread_t		 trx_controller;
	pthread_t		 trx_poller;
	pthread_t		 trx_handler;
	int			 is_running;
	int			 poller_running;
	int			 poller_suspended;
	int			 handler_eol;
	int			 handler_pipefd[2];

	struct command_tag	*new_tag;
	struct command_tag	*next;
} command_tag_t;

#endif /* __TRXD_H__ */
