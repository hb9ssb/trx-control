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

/* The trxctl Lua module */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

extern int connect_trxd(const char *, const char *);
extern int fd;
extern int verbosity;

static int
luatrxctl_connect(lua_State *L)
{
	if (fd)
		close(fd);
	fd = connect_trxd(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
	lua_pushboolean(L, fd > 0);
	return 1;
}

static int
luatrxctl_read(lua_State *L)
{
	char buf[256];
	size_t len;

	len = read(fd, buf, sizeof(buf));
	if (len > 0) {
		if (verbosity)
			printf("< %s\n", buf);
		lua_pushlstring(L, buf, len);
	} else
		lua_pushnil(L);
	return 1;
}

static int
luatrxctl_write(lua_State *L)
{
	const char *data;
	size_t len;

	data = luaL_checklstring(L, 1, &len);
	if (verbosity)
		printf("> %s\n", data);
	write(fd, data, len);
	return 0;
}

static int
luatrxctl_writeln(lua_State *L)
{
	const char *data;
	char *buf;
	char crlf[2] = { 0x0d, 0x0a };
	size_t len;

	data = luaL_checklstring(L, 1, &len);
	buf = malloc(len + 3);
	snprintf(buf, len + 3, "%s\0x0d\0x0a", data);
	if (verbosity)
		printf("> %s\n", data);
	write(fd, buf, len + 2);
	free(buf);
	return 0;
}

int
luaopen_trxctl(lua_State *L)
{
	struct luaL_Reg luatrxctl[] = {
		{ "connect",	luatrxctl_connect },
		{ "read",	luatrxctl_read },
		{ "write",	luatrxctl_write },
		{ "writeln",	luatrxctl_writeln },
		{ NULL, NULL }
	};

	luaL_newlib(L, luatrxctl);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2023 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trxctl for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "trxctl 1.0.0");
	lua_settable(L, -3);

	return 1;
}
