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

#include <lua.h>
#include <lauxlib.h>

#include <curl/curl.h>

#include "luacurl.h"
#include "multi.h"

int
lcurl_multi_init(lua_State *L)
{
	CURLM *multi_handle, **m;

	multi_handle = curl_multi_init();
	if (multi_handle == NULL)
		return luaL_error(L, "internal CURL error");
	m = lua_newuserdata(L, sizeof(CURLM *));
	*m = multi_handle;
	luaL_getmetatable(L, CURL_MULTI_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

int
lcurl_multi_add_handle(lua_State *L)
{
	CURLM **multi_handle = (CURLM **)luaL_checkudata(L, 1,
	    CURL_MULTI_METATABLE);
	curlT *c = (curlT *)luaL_checkudata(L, 2, CURL_EASY_METATABLE);

	curl_multi_add_handle(*multi_handle, c->curl);
	lua_pushboolean(L, 1);
	return 1;
}

int
lcurl_multi_perform(lua_State *L)
{
	CURLM **multi_handle = (CURLM **)luaL_checkudata(L, 1,
	    CURL_MULTI_METATABLE);
	int running_handles;

	curl_multi_perform(*multi_handle, &running_handles);
	lua_pushinteger(L, running_handles);
	return 1;
}

int
lcurl_multi_timeout(lua_State *L)
{
	CURLM **multi_handle = (CURLM **)luaL_checkudata(L, 1,
	    CURL_MULTI_METATABLE);
	long timeout;

	curl_multi_timeout(*multi_handle, &timeout);
	lua_pushinteger(L, timeout);
	return 1;
}

int
lcurl_multi_fds(lua_State *L)
{
	CURLM **multi_handle = (CURLM **)luaL_checkudata(L, 1,
	    CURL_MULTI_METATABLE);
	fd_set readfds, writefds, excfds;
	int fd, max_fd, ridx, widx, eidx;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&excfds);
	curl_multi_fdset(*multi_handle, &readfds, &writefds, &excfds, &max_fd);
	lua_newtable(L);
	lua_newtable(L);
	lua_newtable(L);

	ridx = widx = eidx = 1;

	for (fd = 0; fd < max_fd; fd++) {
		if (FD_ISSET(fd, &readfds)) {
			lua_pushinteger(L, ridx++);
			lua_pushinteger(L, fd);
			lua_settable(L, -5);
		}

		if (FD_ISSET(fd, &writefds)) {
			lua_pushinteger(L, widx++);
			lua_pushinteger(L, fd);
			lua_settable(L, -4);
		}

		if (FD_ISSET(fd, &excfds)) {
			lua_pushinteger(L, eidx++);
			lua_pushinteger(L, fd);
			lua_settable(L, -3);
		}
	}
	return 3;
}

int
lcurl_multi_remove_handle(lua_State *L)
{
	CURLM **multi_handle = (CURLM **)luaL_checkudata(L, 1,
	    CURL_MULTI_METATABLE);
	curlT *c = (curlT *)luaL_checkudata(L, 2, CURL_EASY_METATABLE);

	curl_multi_remove_handle(*multi_handle, c->curl);
	lua_pushboolean(L, 1);
	return 1;
}

int
lcurl_multi_gc(lua_State *L)
{
	CURLM **multi_handle = (CURLM **)luaL_checkudata(L, 1,
	    CURL_MULTI_METATABLE);

	if (*multi_handle) {
		curl_multi_cleanup(*multi_handle);
		*multi_handle = NULL;
	}
	return 0;

}
