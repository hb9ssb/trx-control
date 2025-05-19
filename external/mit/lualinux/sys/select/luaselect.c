/*
 * Copyright (c) 2023 - 2025 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* select() for Lua */

#include <sys/select.h>

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

#include "luaselect.h"

/* fd_set handling functions */
static int
linux_fd_set(lua_State *L)
{
	fd_set *fdset;

	fdset = (fd_set *)lua_newuserdata(L, sizeof(fd_set));
	FD_ZERO(fdset);
	luaL_getmetatable(L, FD_SET_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int
linux_fd_set_clr(lua_State *L)
{
	fd_set *fdset;

	fdset = luaL_checkudata(L, 1, FD_SET_METATABLE);
	FD_CLR(luaL_checkinteger(L, 2), fdset);
	return 0;
}

static int
linux_fd_set_isset(lua_State *L)
{
	fd_set *fdset;

	fdset = luaL_checkudata(L, 1, FD_SET_METATABLE);
	lua_pushboolean(L, FD_ISSET(luaL_checkinteger(L, 2), fdset));
	return 1;
}

static int
linux_fd_set_set(lua_State *L)
{
	fd_set *fdset;

	fdset = luaL_checkudata(L, 1, FD_SET_METATABLE);
	FD_SET(luaL_checkinteger(L, 2), fdset);
	return 0;
}

static int
linux_fd_set_zero(lua_State *L)
{
	fd_set *fdset;

	fdset = luaL_checkudata(L, 1, FD_SET_METATABLE);
	FD_ZERO(fdset);
	return 0;
}

/* select itself */
static int
linux_select(lua_State *L)
{
	struct timeval *tv, tval;
	fd_set *readfds, *writefds, *errorfds;
	int nfds;

	tv = NULL;
	readfds = writefds = errorfds = NULL;

	nfds = luaL_checkinteger(L, 1);
	if (lua_isuserdata(L, 2))
		readfds = luaL_checkudata(L, 2, FD_SET_METATABLE);
	if (lua_isuserdata(L, 3))
		writefds = luaL_checkudata(L, 3, FD_SET_METATABLE);
	if (lua_isuserdata(L, 4))
		errorfds = luaL_checkudata(L, 4, FD_SET_METATABLE);

	switch (lua_gettop(L)) {
	case 5:
		tv = &tval;
		tval.tv_sec = 0;
		tval.tv_usec = lua_tointeger(L, 5);
		break;
	case 6:
		tv = &tval;
		tval.tv_sec = lua_tointeger(L, 5);
		tval.tv_usec = lua_tointeger(L, 6);
		break;
	default:
		tv = NULL;
	}

	lua_pushinteger(L, select(nfds, readfds, writefds, errorfds, tv));
	return 1;
}

int
luaopen_linux_sys_select(lua_State *L)
{
	struct luaL_Reg lualinuxselect[] = {
		/* select */
		{ "select",	linux_select },
		{ "fd_set",	linux_fd_set },
		{ NULL, NULL }
	};
	struct luaL_Reg fd_set_methods[] = {
		{ "clr",	linux_fd_set_clr },
		{ "isset",	linux_fd_set_isset },
		{ "set",	linux_fd_set_set },
		{ "zero",	linux_fd_set_zero },
		{ NULL,		NULL }
	};
	if (luaL_newmetatable(L, FD_SET_METATABLE)) {
		luaL_setfuncs(L, fd_set_methods, 0);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);

		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	luaL_newlib(L, lualinuxselect);
	return 1;
}
