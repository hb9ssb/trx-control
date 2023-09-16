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

typedef struct command_tag {
	pthread_mutex_t		 mutex;
	pthread_cond_t		 cond;
	pthread_mutex_t		 rmutex;
	pthread_cond_t		 rcond;
	pthread_mutex_t		 ai_mutex;

	const char		*name;
	const char		*device;
	const char		*driver;
	const char		*command;
	const char		*param;
	char			*reply;
	int			 client_fd;
	int			 cat_device;
	pthread_t		 trx_control;
	int			 is_running;
	struct command_tag	*next;
} command_tag_t;

#endif /* __TRXD_H__ */
