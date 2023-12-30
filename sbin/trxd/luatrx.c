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

/* Provide the 'trx' Lua module to transceiver drivers */

#include <err.h>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "trxd.h"

static int
luatrx_version(lua_State *L)
{
	lua_pushstring(L, TRXD_VERSION);
	return 1;
}

static int
get_fd(lua_State *L)
{
	int fd;

	lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_CAT_FD);
	fd = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return fd;
}

static int
luatrx_read(lua_State *L)
{
	struct pollfd pfd;
	char buf[256];
	size_t len, nread, nfds;
	int fd;

	len = luaL_checkinteger(L, 1);

	fd = get_fd(L);

	pfd.fd = fd;
	pfd.events = POLLIN;

	nread = 0;
	while (nread < len) {
		nfds = poll(&pfd, 1, 500);
		if (nfds == -1)
			return luaL_error(L, "poll error");
		if (nfds == 1) {
			nread += read(fd, &buf[nread], len - nread);
		} else {
			nread = 0;
			break;
		}
	}

	if (nread > 0)
		lua_pushlstring(L, buf, nread);
	else
		lua_pushnil(L);

	return 1;
}

static int
luatrx_write(lua_State *L)
{
	const char *data;
	size_t len;
	int fd;

	fd = get_fd(L);

	data = luaL_checklstring(L, 1, &len);
	tcflush(fd, TCIFLUSH);
	write(fd, data, len);
	tcdrain(fd);
	return 0;
}

static int
bcd_to_string(lua_State *L)
{
	unsigned char *bcd_string, *string, *p;
	size_t len;
	int n;

	bcd_string = (unsigned char *)luaL_checklstring(L, 1, &len);
	string = p = malloc(len * 2 + 1);
	for (n = 0; n < len; n++, bcd_string++) {
		*p++ = '0' + (*bcd_string >> 4);
		*p++ = '0' + (*bcd_string & 0x0f);
	}
	*p = '\0';
	lua_pushstring(L, string);
	free(string);
	return 1;
}

static int
string_to_bcd(lua_State *L)
{
	unsigned char *bcd_string, *string, *p;
	size_t len;
	int n;

	string = (unsigned char *)luaL_checklstring(L, 1, &len);
	bcd_string = p = malloc(len);
	for (n = 0; n < len / 2; n++) {
		*p++ = *string++ - '0' << 4 | *string++ & 0x0f;
	}
	*p = '\0';

	lua_pushlstring(L, bcd_string, len / 2);
	free(bcd_string);
	return 1;
}

int
luaopen_trx(lua_State *L)
{
	struct luaL_Reg luatrx[] = {
		{ "version",		luatrx_version },
		{ "read",		luatrx_read },
		{ "write",		luatrx_write },
		{ "bcdToString",	bcd_to_string },
		{ "stringToBcd",	string_to_bcd },
		{ NULL, NULL }
	};

	luaL_newlib(L, luatrx);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2023 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trx-control for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "trx 1.1.0");
	lua_settable(L, -3);

	return 1;
}
