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

#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "trxd.h"

extern int fd;

static int
luatrx_version(lua_State *L)
{
	lua_pushstring(L, TRXD_VERSION);
	return 1;
}

static int
luatrx_setspeed(lua_State *L)
{
	struct termios tty;

	if (isatty(fd)) {
		if (tcgetattr(fd, &tty) < 0) {
			lua_pushboolean(L, 0);
		} else {
			cfsetspeed(&tty, luaL_checkinteger(L, 1));
			if (tcsetattr(fd, TCSADRAIN, &tty) < 0) {
				lua_pushboolean(L, 0);
			} else
				lua_pushboolean(L, 1);
		}
	} else
		lua_pushboolean(L, 0);
	return 1;
}

static int
luatrx_read(lua_State *L)
{
	char buf[256];
	size_t len;

	len = read(fd, buf, sizeof(buf));
	if (len > 0)
		lua_pushlstring(L, buf, len);
	else
		lua_pushnil(L);
	return 1;
}

static int
luatrx_write(lua_State *L)
{
	const char *data;
	size_t len;

	data = luaL_checklstring(L, 1, &len);
	write(fd, data, len);
	return 0;
}

int
luaopen_trx(lua_State *L)
{
	struct luaL_Reg luatrx[] = {
		{ "version",	luatrx_version },
		{ "setspeed",	luatrx_setspeed },
		{ "read",	luatrx_read },
		{ "write",	luatrx_write },
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
	lua_pushliteral(L, "trx 1.0.0");
	lua_settable(L, -3);

	return 1;
}
