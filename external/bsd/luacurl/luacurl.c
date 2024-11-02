/*
 * Copyright (c) 2013 - 2024 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
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

/* CURL interface for Lua */

/*
 * This code has been derived from the libCURL binding by Alexander Marinov:
 *
 * luacurl.c
 *
 * author      : Alexander Marinov (alekmarinov@gmail.com)
 * project     : luacurl
 * description : Binds libCURL to Lua
 * copyright   : The same as Lua license (http://www.lua.org/license.html) and
 *               curl license (http://curl.haxx.se/docs/copyright.html)
 * todo        : multipart formpost building,
 *               curl multi
 *
 * Contributors: Thomas Harning added support for tables/threads as the
 * CURLOPT_*DATA items.
 */

#include <sys/param.h>

#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <curl/easy.h>

#ifndef LIBCURL_VERSION
#include <curl/curlver.h>
#endif

#include <lauxlib.h>

#include "luacurl.h"
#include "multi.h"

/* Fast set table macro */
#define FASTLUA_SET_TABLE(context, key, value_type, value) \
	lua_pushliteral(context, key); \
	lua_push##value_type(context, value); \
	lua_settable(context, -3);

/* Faster set table macro */
#define LUA_SET_TABLE(context, key, value_type, value) \
	lua_push##value_type(context, value); \
	lua_setfield(context, -2, key)

/* wrap curl option with simple option type */
#define C_OPT(n, t)
/* wrap curl option with string list type */
#define C_OPT_SL(n) static const char* KEY_##n = #n;
/* wrap the other curl options not included above */
#define C_OPT_SPECIAL(n)

extern struct int_constant curl_int[];

/* describes all currently supported curl options available to curl 7.15.2 */
#define ALL_CURL_OPT \
	C_OPT_SPECIAL(WRITEDATA) \
	C_OPT(URL, string) \
	C_OPT(PORT, number) \
	C_OPT(PROXY, string) \
	C_OPT(USERPWD, string) \
	C_OPT(PROXYUSERPWD, string) \
	C_OPT(RANGE, string) \
	C_OPT_SPECIAL(READDATA) \
	C_OPT_SPECIAL(WRITEFUNCTION) \
	C_OPT_SPECIAL(READFUNCTION) \
	C_OPT(TIMEOUT, number) \
	C_OPT(INFILESIZE, number) \
	C_OPT(POSTFIELDS, string) \
	C_OPT(REFERER, string) \
	C_OPT(FTPPORT, string) \
	C_OPT(USERAGENT, string) \
	C_OPT(LOW_SPEED_LIMIT, number) \
	C_OPT(LOW_SPEED_TIME, number) \
	C_OPT(RESUME_FROM, number) \
	C_OPT(COOKIE, string) \
	C_OPT_SL(HTTPHEADER) \
	C_OPT_SL(HTTPPOST) \
	C_OPT(SSLCERT, string) \
	C_OPT(SSLKEYPASSWD, string) \
	C_OPT(CRLF, boolean) \
	C_OPT_SL(QUOTE) \
	C_OPT_SPECIAL(HEADERDATA) \
	C_OPT(COOKIEFILE, string) \
	C_OPT(SSLVERSION, number) \
	C_OPT(TIMECONDITION, number) \
	C_OPT(TIMEVALUE, number) \
	C_OPT(CUSTOMREQUEST, string) \
	C_OPT_SL(POSTQUOTE) \
	C_OPT(VERBOSE, boolean) \
	C_OPT(HEADER, boolean) \
	C_OPT(NOPROGRESS, boolean) \
	C_OPT(NOBODY, boolean) \
	C_OPT(FAILONERROR, boolean) \
	C_OPT(UPLOAD, boolean) \
	C_OPT(POST, boolean) \
	C_OPT(FTPLISTONLY, boolean) \
	C_OPT(FTPAPPEND, boolean) \
	C_OPT(NETRC, number) \
	C_OPT(FOLLOWLOCATION, boolean) \
	C_OPT(TRANSFERTEXT, boolean) \
	C_OPT(PUT, boolean) \
	C_OPT_SPECIAL(PROGRESSFUNCTION) \
	C_OPT_SPECIAL(PROGRESSDATA) \
	C_OPT(AUTOREFERER, boolean) \
	C_OPT(PROXYPORT, number) \
	C_OPT(POSTFIELDSIZE, number) \
	C_OPT(HTTPPROXYTUNNEL, boolean) \
	C_OPT(INTERFACE, string) \
	C_OPT(KRB4LEVEL, string) \
	C_OPT(SSL_VERIFYPEER, boolean) \
	C_OPT(CAINFO, string) \
	C_OPT(MAXREDIRS, number) \
	C_OPT(FILETIME, boolean) \
	C_OPT_SL(TELNETOPTIONS) \
	C_OPT(MAXCONNECTS, number) \
	C_OPT(FRESH_CONNECT, boolean) \
	C_OPT(FORBID_REUSE, boolean) \
	C_OPT(RANDOM_FILE, string) \
	C_OPT(EGDSOCKET, string) \
	C_OPT(CONNECTTIMEOUT, number) \
	C_OPT_SPECIAL(HEADERFUNCTION) \
	C_OPT(HTTPGET, boolean) \
	C_OPT(SSL_VERIFYHOST, number) \
	C_OPT(COOKIEJAR, string) \
	C_OPT(SSL_CIPHER_LIST, string) \
	C_OPT(HTTP_VERSION, number) \
	C_OPT(FTP_USE_EPSV, boolean) \
	C_OPT(SSLCERTTYPE, string) \
	C_OPT(SSLKEY, string) \
	C_OPT(SSLKEYTYPE, string) \
	C_OPT(SSLENGINE, string) \
	C_OPT(SSLENGINE_DEFAULT, boolean) \
	C_OPT(DNS_USE_GLOBAL_CACHE, boolean) \
	C_OPT(DNS_CACHE_TIMEOUT, number) \
	C_OPT_SL(PREQUOTE) \
	C_OPT(COOKIESESSION, boolean) \
	C_OPT(CAPATH, string) \
	C_OPT(BUFFERSIZE, number) \
	C_OPT(NOSIGNAL, boolean) \
	C_OPT(PROXYTYPE, number) \
	C_OPT(ENCODING, string) \
	C_OPT_SL(HTTP200ALIASES) \
	C_OPT(UNRESTRICTED_AUTH, boolean) \
	C_OPT(FTP_USE_EPRT, boolean) \
	C_OPT(HTTPAUTH, number) \
	C_OPT(FTP_CREATE_MISSING_DIRS, boolean) \
	C_OPT(PROXYAUTH, number) \
	C_OPT(FTP_RESPONSE_TIMEOUT, number) \
	C_OPT(IPRESOLVE, number) \
	C_OPT(MAXFILESIZE, number) \
	C_OPT(INFILESIZE_LARGE, number) \
	C_OPT(RESUME_FROM_LARGE, number) \
	C_OPT(MAXFILESIZE_LARGE, number) \
	C_OPT(NETRC_FILE, string) \
	C_OPT(FTP_SSL, number) \
	C_OPT(POSTFIELDSIZE_LARGE, number) \
	C_OPT(TCP_NODELAY, boolean) \
	C_OPT(SOURCE_USERPWD, string) \
	C_OPT_SL(SOURCE_PREQUOTE) \
	C_OPT_SL(SOURCE_POSTQUOTE) \
	C_OPT(FTPSSLAUTH, number) \
	C_OPT_SPECIAL(IOCTLFUNCTION) \
	C_OPT_SPECIAL(IOCTLDATA) \
	C_OPT(SOURCE_URL, string) \
	C_OPT_SL(SOURCE_QUOTE) \
	C_OPT(FTP_ACCOUNT, string) \
	C_OPT(COOKIELIST, string) \
	C_OPT(IGNORE_CONTENT_LENGTH, boolean) \
	C_OPT(FTP_SKIP_PASV_IP, boolean) \
	C_OPT(FTP_FILEMETHOD, number) \
	C_OPT(LOCALPORT, number) \
	C_OPT(LOCALPORTRANGE, number)

#if CURL_OLDER(7,12,1) || CURL_NEWER(7,16,2)
#define CURLOPT_SOURCE_PREQUOTE      -MAKE_VERSION_NUM(1,12,7) + 0
#define CURLOPT_SOURCE_POSTQUOTE     -MAKE_VERSION_NUM(1,12,7) + 1
#define CURLOPT_SOURCE_USERPWD       -MAKE_VERSION_NUM(1,12,7) + 2
#endif

#if CURL_OLDER(7,12,2)
#define CURLOPT_FTPSSLAUTH           -MAKE_VERSION_NUM(2,12,7) +0
#endif

#if CURL_OLDER(7,12,3)
#define CURLOPT_TCP_NODELAY          -MAKE_VERSION_NUM(3,12,7) + 0
#define CURLOPT_IOCTLFUNCTION        -MAKE_VERSION_NUM(3,12,7) + 1
#define CURLOPT_IOCTLDATA            -MAKE_VERSION_NUM(3,12,7) + 2
#define CURLE_SSL_ENGINE_INITFAILED  -MAKE_VERSION_NUM(3,12,7) + 3
#define CURLE_IOCTLFUNCTION          -MAKE_VERSION_NUM(3,12,7) + 4
#define CURLE_IOCTLDATA              -MAKE_VERSION_NUM(3,12,7) + 5
#endif

#if CURL_OLDER(7,13,0)
#define CURLOPT_FTP_ACCOUNT          -MAKE_VERSION_NUM(0,13,7) + 2
#define CURLE_INTERFACE_FAILED       -MAKE_VERSION_NUM(0,13,7) + 3
#define CURLE_SEND_FAIL_REWIND       -MAKE_VERSION_NUM(0,13,7) + 4
#endif

#if CURL_OLDER(7,13,0) || CURL_NEWER(7,16,2)
#define CURLOPT_SOURCE_URL           -MAKE_VERSION_NUM(0,13,7) + 0
#define CURLOPT_SOURCE_QUOTE         -MAKE_VERSION_NUM(0,13,7) + 1
#endif

#if CURL_OLDER(7,13,1)
#define CURLE_LOGIN_DENIED           -MAKE_VERSION_NUM(1,13,7) + 0
#endif

/* register static the names of options of type curl_slist */
ALL_CURL_OPT

/*
 *	not supported options for any reason
 *
 *	CURLOPT_STDERR (TODO)
 *	CURLOPT_CLOSEPOLICY
 *	CURLOPT_WRITEINFO
 *	CURLOPT_ERRORBUFFER (all curl operations return error message via
 *	curl_easy_strerror)
 *	CURLOPT_SHARE (TODO)
 *	CURLOPT_PRIVATE (TODO)
 *	CURLOPT_DEBUGFUNCTION (requires curl debug mode)
 *	CURLOPT_DEBUGDATA
 *	CURLOPT_SSLCERTPASSWD (duplicated by SSLKEYPASSWD)
 *	CURLOPT_SSL_CTX_FUNCTION (TODO, this will make sense only if openssl is
 *	available in Lua)
 *	CURLOPT_SSL_CTX_DATA
 */

/* push correctly luaValue to Lua stack */
static void
pushLuaValueT(lua_State* L, int t, union luaValueT v)
{
	switch (t) {
	case LUA_TNIL:
		lua_pushnil(L);
		break;
	case LUA_TBOOLEAN:
		lua_pushboolean(L, v.nval);
		break;
	case LUA_TTABLE:
	case LUA_TFUNCTION:
	case LUA_TTHREAD:
	case LUA_TUSERDATA:
		lua_rawgeti(L, LUA_REGISTRYINDEX, v.nval);
		break;
	case LUA_TLIGHTUSERDATA:
		lua_pushlightuserdata(L, v.ptr);
		break;
	case LUA_TNUMBER:
		lua_pushnumber(L, v.nval);
		break;
	case LUA_TSTRING:
		lua_pushstring(L, v.sval);
		break;
	default:
		luaL_error(L, "invalid type %s\n", lua_typename(L, t));
	}
}

/* curl callbacks connected with Lua functions */
static size_t
readerCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	const char *readBytes;
	curlT *c = stream;

	lua_rawgeti(c->L, LUA_REGISTRYINDEX, c->freaderRef);
	pushLuaValueT(c->L, c->rudtype, c->rud);
	lua_pushnumber(c->L, size * nmemb);
	lua_call(c->L, 2, 1);
	readBytes = lua_tostring(c->L, -1);
	if (readBytes) {
		memcpy(ptr, readBytes, MIN(lua_rawlen(c->L, -1), size * nmemb));
		return MIN(lua_rawlen(c->L, -1), size * nmemb);
	}
	return 0;
}

static size_t
writerCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	curlT *c = stream;

	lua_rawgeti(c->L, LUA_REGISTRYINDEX, c->fwriterRef);
	pushLuaValueT(c->L, c->wudtype, c->wud);
	lua_pushlstring(c->L, ptr, size * nmemb);
	lua_call(c->L, 2, 1);
	return (size_t)lua_tonumber(c->L, -1);
}

static int
progressCallback(void *clientp, double dltotal, double dlnow, double ultotal,
    double ulnow)
{
	curlT *c = clientp;

	lua_rawgeti(c->L, LUA_REGISTRYINDEX, c->fprogressRef);
	pushLuaValueT(c->L, c->pudtype, c->pud);
	lua_pushnumber(c->L, dltotal);
	lua_pushnumber(c->L, dlnow);
	lua_pushnumber(c->L, ultotal);
	lua_pushnumber(c->L, ulnow);
	lua_call(c->L, 5, 1);
	return (int)lua_tonumber(c->L, -1);
}

static size_t
headerCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	curlT *c = stream;

	lua_rawgeti(c->L, LUA_REGISTRYINDEX, c->fheaderRef);
	pushLuaValueT(c->L, c->hudtype, c->hud);
	lua_pushlstring(c->L, ptr, size * nmemb);
	lua_call(c->L, 2, 1);
	return (size_t)lua_tonumber(c->L, -1);
}

#if CURL_NEWER(7,12,3)
static curlioerr
ioctlCallback(CURL *handle, int cmd, void *clientp)
{
	curlT *c = clientp;

	lua_rawgeti(c->L, LUA_REGISTRYINDEX, c->fioctlRef);
	pushLuaValueT(c->L, c->iudtype, c->iud);
	lua_pushnumber(c->L, cmd);
	lua_call(c->L, 2, 1);
	return (curlioerr)lua_tonumber(c->L, -1);
}
#endif

/* Initializes CURL connection */
static int
lcurl_easy_init(lua_State *L)
{
	curlT *c = lua_newuserdata(L, sizeof(curlT));

	c->L = L;
	c->freaderRef = c->fwriterRef = c->fprogressRef = c->fheaderRef
	    = c->fioctlRef = LUA_REFNIL;
	c->rud.nval = c->wud.nval = c->pud.nval = c->hud.nval = c->iud.nval = 0;
	c->rudtype = c->wudtype = c->pudtype = c->hudtype = c->iudtype
	    = LUA_TNIL;

	/* open curl handle */
	c->curl = curl_easy_init();

	/* set metatable to curlT object */
	luaL_getmetatable(L, CURL_EASY_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

/* Escapes URL strings */
static int
lcurl_escape(lua_State *L)
{
	if (!lua_isnil(L, 1)) {
		const char *s = luaL_checkstring(L, 1);
		lua_pushstring(L, curl_escape(s, (int)lua_rawlen(L, 1)));
		return 1;
	} else
		luaL_argerror(L, 1, "string parameter expected");
	return 0;
}

/* Unescapes URL encoding in strings */
static int
lcurl_unescape(lua_State *L)
{
	if (!lua_isnil(L, 1)) {
		const char *s = luaL_checkstring(L, 1);
		lua_pushstring(L, curl_unescape(s, (int)lua_rawlen(L, 1)));
		return 1;
	} else
		luaL_argerror(L, 1, "string parameter expected");
	return 0;
}

static int
lcurl_easy_escape(lua_State *L)
{
	curlT *c = luaL_checkudata(L, 1, CURL_EASY_METATABLE);
	size_t len;
	const char *buf;
	char *outbuf;

	buf = luaL_checklstring(L, 2, &len);
	outbuf = curl_easy_escape(c->curl, buf, len);
	if (outbuf) {
		lua_pushstring(L, outbuf);
		curl_free(outbuf);
	} else
		lua_pushnil(L);
	return 1;
}

static int
lcurl_easy_unescape(lua_State *L)
{
	curlT *c = luaL_checkudata(L, 1, CURL_EASY_METATABLE);
	size_t len;
	int outlen;
	const char *buf;
	char *outbuf;

	buf = luaL_checklstring(L, 2, &len);
	outbuf = curl_easy_unescape(c->curl, buf, len, &outlen);
	if (outbuf) {
		lua_pushlstring(L, outbuf, outlen);
		curl_free(outbuf);
	} else
		lua_pushnil(L);
	return 1;
}

#if CURL_NEWER(7,12,1)
static int
lcurl_easy_reset(lua_State *L)
{
	curlT *c = luaL_checkudata(L, 1, CURL_EASY_METATABLE);

	curl_easy_reset(c->curl);
	return 0;
}
#endif

/* Access curlT object from the Lua stack at specified position  */
static curlT *
tocurl(lua_State *L, int cindex)
{
	curlT *c = luaL_checkudata(L, cindex, CURL_EASY_METATABLE);

	if (!c)
		luaL_argerror(L, cindex, "invalid curl object");
	if (!c->curl)
		luaL_error(L, "attempt to use closed curl object");
	return c;
}

/* Request internal information from the curl session */
static int
lcurl_easy_getinfo(lua_State *L)
{
	curlT *c = tocurl(L, 1);
	CURLINFO nInfo;
	CURLcode code = -1;

	luaL_checktype(L, 2, LUA_TNUMBER);   /* accept info code number only */
	nInfo=lua_tonumber(L, 2);
	if (nInfo>CURLINFO_SLIST) {
		/* string list */
		struct curl_slist *slist = 0;
		if (CURLE_OK ==
		    (code = curl_easy_getinfo(c->curl, nInfo, &slist))) {
			if (slist) {
				int i;
				lua_newtable(L);
				for (i=1; slist; i++, slist=slist->next) {
					lua_pushnumber(L, i);
					lua_pushstring(L, slist->data);
					lua_settable(L, -3);
				}
				curl_slist_free_all(slist);
			} else
				lua_pushnil(L);
			return 1;
		} else
			;/* curl_easy_getinfo returns error */
	} else if (nInfo>CURLINFO_DOUBLE) {
		/* double */
		double value;
		if (CURLE_OK ==
		    (code = curl_easy_getinfo(c->curl, nInfo, &value))) {
			lua_pushnumber(L, value);
			return 1;
		} else
			; /* curl_easy_getinfo returns error */
	} else if (nInfo>CURLINFO_LONG) {
		/* long */
		long value;
		if (CURLE_OK ==
		    (code = curl_easy_getinfo(c->curl, nInfo, &value))) {
			lua_pushinteger(L, (lua_Integer)value);
			return 1;
		} else
			; /* curl_easy_getinfo returns error */
	} else if (nInfo>CURLINFO_STRING) {
		/* string */
		char *value;
		if (CURLE_OK ==
		    (code=curl_easy_getinfo(c->curl, nInfo, &value))) {
			lua_pushstring(L, value);
			return 1;
		} else
			; /* curl_easy_getinfo returns error */
	}
	/* on error */
	/* return nil, error message, error code */
	lua_pushnil(L);
	if (code>CURLE_OK) {
#if CURL_NEWER(7,11,2)
		lua_pushstring(L, curl_easy_strerror(code));
#else
		lua_pushfstring(L, "Curl error: #%d", (code));
#endif
		lua_pushnumber(L, code);
		return 3;
	} else {
		lua_pushfstring(L, "Invalid CURLINFO number: %d", nInfo);
		return 2;
	}
}

/* convert argument n to string allowing nil values */
static union luaValueT
get_string(lua_State *L, int n)
{
	union luaValueT v;

	v.sval = (char *)lua_tostring(L, 3);
	return v;
}

/* convert argument n to number allowing only number convertible values */
static union luaValueT
get_number(lua_State *L, int n)
{
	union luaValueT v;

	v.nval = (int)luaL_checknumber(L, n);
	return v;
}

/*
 * get argument n as boolean but if not boolean argument then fail
 * with Lua error
 */
static union luaValueT
get_boolean(lua_State *L, int n)
{
	union luaValueT v;

	if (!lua_isboolean(L, n))
		luaL_argerror(L, n, "boolean value expected");
	v.nval = (int)lua_toboolean(L, n);
	return v;
}

/*
 * remove and free old slist from registry if any associated with the
 * given key
 */
static void
free_slist(lua_State *L, const char **key)
{
	struct curl_slist *slist;

	lua_pushlightuserdata(L, (void *)key);
	lua_rawget(L, LUA_REGISTRYINDEX);
	slist = (struct curl_slist *)lua_topointer(L, -1);

	if (slist) {
		curl_slist_free_all(slist);

		lua_pushlightuserdata(L, (void *)key);
		lua_pushlightuserdata(L, NULL);
		lua_rawset(L, LUA_REGISTRYINDEX);
	}
}

/* after argument number n combine all arguments to the curl type curl_slist */
static union luaValueT
get_slist(lua_State *L, int n, const char **key)
{
	int i;
	union luaValueT v;
	struct curl_slist *slist = 0;

	/* free the previous slist if any */
	free_slist(L, key);

	/* check if all parameters are strings */
	for (i = n; i < lua_gettop(L); i++)
		luaL_checkstring(L, i);
	for (i = n; i < lua_gettop(L); i++)
		slist = curl_slist_append(slist, lua_tostring(L, i));

	/* set the new slist in registry */
	lua_pushlightuserdata(L, (void *)key);
	lua_pushlightuserdata(L, (void *)slist);
	lua_rawset(L, LUA_REGISTRYINDEX);

	v.slist = slist;
	return v;
}

/* set any supported curl option (see also ALL_CURL_OPT) */
static int
lcurl_easy_setopt(lua_State* L)
{
	union luaValueT v;			/* the result option value */
	int curlOpt;				/* the provided option code  */
	CURLcode code;				/* return curl error code */
	curlT *c = tocurl(L, 1);		/* get self object */

	/* accept only number option  codes */
	luaL_checktype(L, 2, LUA_TNUMBER);

	/* option value is always required */
	if (lua_gettop(L) < 3)
		luaL_error(L,
		    "Invalid number of arguments %d to `setopt' method",
		    lua_gettop(L));

	/* get the curl option code */
	curlOpt = (int)lua_tonumber(L, 2);
	v.nval = 0;

	switch (curlOpt) {
	case CURLOPT_PROGRESSFUNCTION:
	case CURLOPT_READFUNCTION:
	case CURLOPT_WRITEFUNCTION:
	case CURLOPT_HEADERFUNCTION:
	case CURLOPT_IOCTLFUNCTION:
		/* callback options require Lua function value */
		luaL_checktype(L, 3, LUA_TFUNCTION);
	case CURLOPT_READDATA:
	case CURLOPT_WRITEDATA:
	case CURLOPT_PROGRESSDATA:
	case CURLOPT_HEADERDATA:
#if CURL_NEWER(7,12,3)
	case CURLOPT_IOCTLDATA:
#endif
		/*
		 * handle table, userdata and function callback params
		 * specially
		 */
		switch (lua_type(L, 3)) {
		case LUA_TTABLE:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TFUNCTION:
		{
			int ref;
			lua_pushvalue(L, 3);
			/* get reference to the lua object in registry */
			ref = luaL_ref(L, LUA_REGISTRYINDEX);

			/* unregister previous reference to reader if any */
			if (curlOpt == CURLOPT_READFUNCTION) {
				luaL_unref(L, LUA_REGISTRYINDEX, c->freaderRef);
				/* keep the reader function reference in self */
				c->freaderRef = ref;
				/*
				 * redirect the option value to readerCallback
				 */
				v.rcb = (curl_read_callback)readerCallback;
				if (CURLE_OK !=
				    (code = curl_easy_setopt(c->curl,
				    CURLOPT_READDATA, c)))
					goto on_error;
			} else if (curlOpt == CURLOPT_WRITEFUNCTION) {
				luaL_unref(L, LUA_REGISTRYINDEX, c->fwriterRef);
				c->fwriterRef = ref;
				v.wcb=(curl_write_callback)writerCallback;
				if (CURLE_OK !=
				    (code = curl_easy_setopt(c->curl,
				    CURLOPT_WRITEDATA, c)))
					goto on_error;
			} else if (curlOpt == CURLOPT_PROGRESSFUNCTION) {
				luaL_unref(L, LUA_REGISTRYINDEX,
				    c->fprogressRef);
				c->fprogressRef = ref;
				v.pcb =
				    (curl_progress_callback)progressCallback;
				if (CURLE_OK !=
				    (code = curl_easy_setopt(c->curl,
				    CURLOPT_PROGRESSDATA, c)))
					goto on_error;
			} else if (curlOpt == CURLOPT_HEADERFUNCTION) {
				luaL_unref(L, LUA_REGISTRYINDEX, c->fheaderRef);
				c->fheaderRef = ref;
				v.wcb = (curl_write_callback)headerCallback;
				if (CURLE_OK !=
				    (code = curl_easy_setopt(c->curl,
				    CURLOPT_HEADERDATA, c)))
					goto on_error;
			}
#if CURL_NEWER(7,12,3)
			else if (curlOpt == CURLOPT_IOCTLFUNCTION) {
				luaL_unref(L, LUA_REGISTRYINDEX, c->fioctlRef);
				c->fioctlRef = ref;
				v.icb=ioctlCallback;
				if (CURLE_OK !=
				    (code = curl_easy_setopt(c->curl,
				    CURLOPT_IOCTLDATA, c)))
					goto on_error;
			}
#endif
			else
				/*
				 * When the option code is any of
				 * CURLOPT_xxxDATA and the argument is table,
				 * userdata or function set the curl option
				 * value to the lua object reference
				 */
				v.nval = ref;
		}
		break;
	}
	break;

/*
 * Handle all supported curl options differently according the specific option
 * argument type
 */
#undef C_OPT
#define C_OPT(n, t) \
	case CURLOPT_##n: \
		v = get_##t(L, 3); \
		break;

#undef C_OPT_SL
#define C_OPT_SL(n) \
	case CURLOPT_##n: \
		{ \
			v = get_slist(L, 3, &KEY_##n); \
		}break;

/* Expands all the list of switch-case's here */
ALL_CURL_OPT

	default:
		luaL_error(L, "Not supported curl option %d", curlOpt);
	}

	/*
	 * additional check if the option value has compatible type with the
	 * option code
	 */
	switch (lua_type(L, 3)) {

	/* allow function argument only for the special option codes */
	case LUA_TFUNCTION:
		if (curlOpt == CURLOPT_READFUNCTION
		    || curlOpt == CURLOPT_WRITEFUNCTION
		    || curlOpt == CURLOPT_PROGRESSFUNCTION
		    || curlOpt == CURLOPT_HEADERFUNCTION
		    || curlOpt == CURLOPT_IOCTLFUNCTION)
		break;

	/* allow table or userdata only for the callback parameter option */
	case LUA_TTABLE:
	case LUA_TUSERDATA:
		if (curlOpt != CURLOPT_READDATA
		    && curlOpt != CURLOPT_WRITEDATA
		    && curlOpt != CURLOPT_PROGRESSDATA
		    && curlOpt != CURLOPT_HEADERDATA
#if CURL_NEWER(7,12,3)
		    && curlOpt != CURLOPT_IOCTLDATA
#endif
		)
			luaL_error(L,
			    "argument #2 type %s is not compatible with this "
			    "option", lua_typename(L, 3));
		break;
	}

	/* handle curl option for setting callback parameter */
	switch (curlOpt) {
	case CURLOPT_READDATA:
		/* unref previously referenced read data */
		if (c->rudtype == LUA_TFUNCTION
		    || c->rudtype == LUA_TUSERDATA
		    || c->rudtype == LUA_TTABLE
		    || c->rudtype == LUA_TTHREAD)
			luaL_unref(L, LUA_REGISTRYINDEX, c->rud.nval);

		/* set the read data type */
		c->rudtype = lua_type(L, 3);

		/* set the read data value (it can be reference) */
		c->rud = v;

		/* set the real read data to curl as our self object */
		v.ptr = c;
		break;
	case CURLOPT_WRITEDATA:
		if (c->wudtype == LUA_TFUNCTION
		    || c->wudtype == LUA_TUSERDATA
		    || c->wudtype == LUA_TTABLE
		    || c->wudtype == LUA_TTHREAD)
			luaL_unref(L, LUA_REGISTRYINDEX, c->wud.nval);
		c->wudtype = lua_type(L, 3);
		c->wud = v;
		v.ptr = c;
		break;
	case CURLOPT_PROGRESSDATA:
		if (c->pudtype == LUA_TFUNCTION
		    || c->pudtype == LUA_TUSERDATA
		    || c->pudtype == LUA_TTABLE
		    || c->pudtype == LUA_TTHREAD)
			luaL_unref(L, LUA_REGISTRYINDEX, c->pud.nval);
		c->pudtype = lua_type(L, 3);
		c->pud = v;
		v.ptr = c;
		break;
	case CURLOPT_HEADERDATA:
		if (c->hudtype == LUA_TFUNCTION
		    || c->hudtype == LUA_TUSERDATA
		    || c->hudtype == LUA_TTABLE
		    || c->hudtype == LUA_TTHREAD)
			luaL_unref(L, LUA_REGISTRYINDEX, c->hud.nval);
		c->hudtype = lua_type(L, 3);
		c->hud = v;
		v.ptr = c;
		break;
#if CURL_NEWER(7,12,3)
	case CURLOPT_IOCTLDATA:
		if (c->iudtype == LUA_TFUNCTION
		    || c->iudtype == LUA_TUSERDATA
		    || c->iudtype == LUA_TTABLE
		    || c->iudtype == LUA_TTHREAD)
			luaL_unref(L, LUA_REGISTRYINDEX, c->iud.nval);
		c->iudtype = lua_type(L, 3);
		c->iud = v;
		v.ptr = c;
		break;
#endif
	}

	/* set easy the curl option with the processed value */
	if (CURLE_OK ==
	    (code = curl_easy_setopt(c->curl,
	    (int)lua_tonumber(L, 2), v.nval))) {
		/* on success return true */
		lua_pushboolean(L, 1);
		return 1;
	}

on_error:
	/* on fail return nil, error message, error code */
	lua_pushnil(L);
#if CURL_NEWER(7,11,2)
	lua_pushstring(L, curl_easy_strerror(code));
#else
	lua_pushfstring(L, "Curl error: #%d", (code));
#endif
	lua_pushnumber(L, code);
	return 3;
}

/* perform the curl commands */
static int
lcurl_easy_perform(lua_State* L)
{
	CURLcode code;                       /* return error code from curl */
	curlT *c = tocurl(L, 1);             /* get self object */

	code = curl_easy_perform(c->curl);   /* do the curl perform */
	if (CURLE_OK == code) {
		/* on success return true */
		lua_pushboolean(L, 1);
		return 1;
	}
	/* on fail return false, error message, error code */
	lua_pushboolean(L, 0);
#if CURL_NEWER(7,11,2)
	lua_pushstring(L, curl_easy_strerror(code));
#else
	lua_pushfstring(L, "Curl error: #%d", (code));
#endif
	lua_pushnumber(L, code);
	return 3;
}

/* Finalizes CURL */
static int
lcurl_easy_cleanup(lua_State *L)
{
	curlT *c = tocurl(L, 1);

	curl_easy_cleanup(c->curl);
	luaL_unref(L, LUA_REGISTRYINDEX, c->freaderRef);
	luaL_unref(L, LUA_REGISTRYINDEX, c->fwriterRef);
	luaL_unref(L, LUA_REGISTRYINDEX, c->fprogressRef);
	luaL_unref(L, LUA_REGISTRYINDEX, c->fheaderRef);
	luaL_unref(L, LUA_REGISTRYINDEX, c->fioctlRef);
	if (c->rudtype == LUA_TFUNCTION
	    || c->rudtype == LUA_TUSERDATA
	    || c->rudtype == LUA_TTABLE
	    || c->rudtype == LUA_TTHREAD)
		luaL_unref(L, LUA_REGISTRYINDEX, c->rud.nval);
	if (c->wudtype == LUA_TFUNCTION
	    || c->wudtype == LUA_TUSERDATA
	    || c->wudtype == LUA_TTABLE
	    || c->wudtype == LUA_TTHREAD)
		luaL_unref(L, LUA_REGISTRYINDEX, c->wud.nval);
	if (c->pudtype == LUA_TFUNCTION
	    || c->pudtype == LUA_TUSERDATA
	    || c->pudtype == LUA_TTABLE
	    || c->pudtype == LUA_TTHREAD)
		luaL_unref(L, LUA_REGISTRYINDEX, c->pud.nval);
	if (c->hudtype == LUA_TFUNCTION
	    || c->hudtype == LUA_TUSERDATA
	    || c->hudtype == LUA_TTABLE
	    || c->hudtype == LUA_TTHREAD)
		luaL_unref(L, LUA_REGISTRYINDEX, c->hud.nval);
	if (c->iudtype == LUA_TFUNCTION
	    || c->iudtype == LUA_TUSERDATA
	    || c->iudtype == LUA_TTABLE
	    || c->iudtype == LUA_TTHREAD)
		luaL_unref(L, LUA_REGISTRYINDEX, c->iud.nval);

#undef C_OPT
#undef C_OPT_SL
#undef C_OPT_SPECIAL
#define C_OPT(n, t)
#define C_OPT_SL(n) free_slist(L, &KEY_##n);
#define C_OPT_SPECIAL(n)

	ALL_CURL_OPT

	c->curl = 0;
	lua_pushboolean(L, 1);
	return 1;
}

/* garbage collect the curl object */
static int
lcurl_easy_gc(lua_State *L)
{
	curlT *c = (curlT *)luaL_checkudata(L, 1, CURL_EASY_METATABLE);
	if (c && c->curl)
		curl_easy_cleanup(c->curl);
	return 0;
}

static int
lcurl_version(lua_State *L)
{
	lua_pushstring(L, curl_version());
	return 1;
}

static int
lcurl_version_info(lua_State *L)
{
	curl_version_info_data *info;
	int n;

	info = curl_version_info(CURLVERSION_NOW);

	lua_newtable(L);
	lua_pushinteger(L, info->age);
	lua_setfield(L, -2, "age");

	if (info->age >= 0) {
		lua_pushstring(L, info->version);
		lua_setfield(L, -2, "version");
		lua_pushinteger(L, info->version_num);
		lua_setfield(L, -2, "versionNum");
		lua_pushstring(L, info->host);
		lua_setfield(L, -2, "host");
		lua_pushinteger(L, info->features);
		lua_setfield(L, -2, "features");
		lua_pushstring(L, info->libz_version);
		lua_setfield(L, -2, "libzVersion");

		lua_newtable(L);
		for (n = 0; info->protocols[n]; n++) {
			lua_pushinteger(L, n + 1);
			lua_pushstring(L, info->protocols[n]);
			lua_settable(L, -3);
		}
		lua_setfield(L, -2, "protocols");
	}
	if (info->age >= 1) {
		lua_pushstring(L, info->ares);
		lua_setfield(L, -2, "ares");
		lua_pushinteger(L, info->ares_num);
		lua_setfield(L, -2, "aresNum");
	}
	if (info->age >= 2) {
		lua_pushstring(L, info->libidn);
		lua_setfield(L, -2, "libidn");
	}
#if CURL_NEWER(7,16,1)
	if (info->age >= 3) {
		lua_pushinteger(L, info->iconv_ver_num);
		lua_setfield(L, -2, "iconvVersion");
		lua_pushstring(L, info->libssh_version);
		lua_setfield(L, -2, "libsshVersion");
	}
#endif
#if CURL_NEWER(7,57,0)
	if (info->age >= 4) {
		lua_pushinteger(L, info->brotli_ver_num);
		lua_setfield(L, -2, "brotliVersionNum");
		lua_pushstring(L, info->brotli_version);
		lua_setfield(L, -2, "brotliVersion");
	}
#endif
	return 1;
}

static const struct luaL_Reg luacurl_easy_methods[] = {
	{ "cleanup",		lcurl_easy_cleanup },
	{ "close",		lcurl_easy_cleanup },	/* old name */
	{ "setopt",		lcurl_easy_setopt },
	{ "perform",		lcurl_easy_perform },
	{ "getinfo",		lcurl_easy_getinfo },
	{ "escape",		lcurl_easy_escape },
	{ "unescape",		lcurl_easy_unescape },
#if CURL_NEWER(7,12,1)
	{ "reset",		lcurl_easy_reset },
#endif
	{ NULL,			NULL}
};

static const struct luaL_Reg luacurl_multi_methods[] = {
	{ "add_handle",		lcurl_multi_add_handle },
	{ "fds",		lcurl_multi_fds },
	{ "remove_handle",	lcurl_multi_remove_handle },
	{ "perform",		lcurl_multi_perform },
	{ "timeout",		lcurl_multi_timeout },
	{ NULL,			NULL }
};

static const struct luaL_Reg luacurl_funcs[] = {
	{ "easy",		lcurl_easy_init },
	{ "escape",		lcurl_escape },
	{ "multi",		lcurl_multi_init },
	{ "unescape",		lcurl_unescape },
	{ "version",		lcurl_version },
	{ "versionInfo",	lcurl_version_info },
	{ NULL,		NULL }
};

static void
setcurloptions(lua_State *L)
{
#undef C_OPT_SPECIAL
#undef C_OPT
#undef C_OPT_SL
#define C_OPT(n, t) LUA_SET_TABLE(L, "OPT_"#n, number, CURLOPT_##n);
#define C_OPT_SL(n) C_OPT(n, dummy)
#define C_OPT_SPECIAL(n) C_OPT(n, dummy)

ALL_CURL_OPT
}

int
luaopen_curl(lua_State *L)
{
	int n;

	curl_global_init(CURL_GLOBAL_ALL);

	if (luaL_newmetatable(L, CURL_EASY_METATABLE)) {
		luaL_setfuncs(L, luacurl_easy_methods, 0);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, lcurl_easy_gc);
		lua_settable(L, -3);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	if (luaL_newmetatable(L, CURL_MULTI_METATABLE)) {
		luaL_setfuncs(L, luacurl_multi_methods, 0);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, lcurl_multi_gc);
		lua_settable(L, -3);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	luaL_newlib(L, luacurl_funcs);

	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2013 - 2020 "
	    "micro systems marc balmer");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L,  "CURL for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L,  "1.2.0");
	lua_settable(L, -3);

	for (n = 0; n < num_curl_int(); n++) {
		lua_pushinteger(L, curl_int[n].value);
		lua_setfield(L, -2, curl_int[n].name);
	};
	setcurloptions(L);

	return 1;
}
