/*
 * Copyright (c) 2013 - 2020 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Micro Systems Marc Balmer nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL MICRO SYSTEMS MARC BALMER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Lua binding for CURL */

#ifndef __LUACURL_H__
#define __LUACURL_H__

#define CURL_EASY_METATABLE	"CURL easy handle"

#define MAKE_VERSION_NUM(x,y,z) (z + (y << 8) + (x << 16))
#define CURL_NEWER(x,y,z) (MAKE_VERSION_NUM(x,y,z) <= LIBCURL_VERSION_NUM)
#define CURL_OLDER(x,y,z) (MAKE_VERSION_NUM(x,y,z) > LIBCURL_VERSION_NUM)

/* wrap values of arbitrary type */
union luaValueT
{
	struct curl_slist	*slist;
	curl_read_callback	 rcb;
	curl_write_callback	 wcb;
	curl_progress_callback	 pcb;
#if CURL_NEWER(7,12,3)
	curl_ioctl_callback	 icb;
#endif
	long			 nval;
	char			*sval;
	void			*ptr;
};

/* CURL object wrapper type */
typedef struct
{
	CURL		*curl;
	lua_State	*L;
	int 		 fwriterRef;
	int		 wudtype;
	union luaValueT	 wud;
	int		 freaderRef;
	int		 rudtype;
	union luaValueT	 rud;
	int		 fprogressRef;
	int		 pudtype;
	union luaValueT	 pud;
	int		 fheaderRef;
	int		 hudtype;
	union luaValueT	 hud;
	int		 fioctlRef;
	int		 iudtype;
	union		 luaValueT iud;
} curlT;

struct int_constant {
	char *name;
	long value;
};

struct str_constant {
	char *name;
	char *value;
};

extern size_t num_curl_int(void);

#endif /* __LUACURL_H__ */
