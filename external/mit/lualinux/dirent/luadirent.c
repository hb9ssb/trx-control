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

/* Directory functions for Lua */

#include <lua.h>
#include <lauxlib.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "luadirent.h"

static int linux_opendir(lua_State *);
static int linux_readdir(lua_State *);
static int linux_telldir(lua_State *);
static int linux_seekdir(lua_State *);
static int linux_rewinddir(lua_State *);
static int linux_closedir(lua_State *);

static int
linux_opendir(lua_State *L)
{
	DIR *dirp;
	DIR **dirpp;

	if (!(dirp = opendir(luaL_checkstring(L, 1)))) {
		lua_pushnil(L);
		lua_pushfstring(L, "%s: %s", lua_tostring(L, 1),
		    strerror(errno));
		return 2;
	} else {
		dirpp = lua_newuserdata(L, sizeof(DIR **));
		*dirpp = dirp;
		luaL_setmetatable(L, DIR_METATABLE);
	}
	return 1;
}

static int
linux_readdir(lua_State *L)
{
	DIR **dirp;
	struct dirent *dirent;

	dirp = luaL_checkudata(L, 1, DIR_METATABLE);
	dirent = readdir(*dirp);
	if (dirent != NULL) {
		lua_newtable(L);
		lua_pushinteger(L, dirent->d_ino);
		lua_setfield(L, -2, "d_ino");
		lua_pushinteger(L, dirent->d_off);
		lua_setfield(L, -2, "d_off");
		lua_pushinteger(L, dirent->d_reclen);
		lua_setfield(L, -2, "d_reclen");
		lua_pushinteger(L, dirent->d_type);
		lua_setfield(L, -2, "d_type");
		lua_pushstring(L, dirent->d_name);
		lua_setfield(L, -2, "d_name");
	} else
		lua_pushnil(L);
	return 1;
}

static int
linux_telldir(lua_State *L)
{
	DIR **dirp = luaL_checkudata(L, 1, DIR_METATABLE);
	lua_pushinteger(L, telldir(*dirp));
	return 1;
}

static int
linux_seekdir(lua_State *L)
{
	DIR **dirp = luaL_checkudata(L, 1, DIR_METATABLE);
	seekdir(*dirp, luaL_checkinteger(L, 2));
	return 0;
}

static int
linux_rewinddir(lua_State *L)
{
	DIR **dirp = luaL_checkudata(L, 1, DIR_METATABLE);
	rewinddir(*dirp);
	return 0;
}

static int
linux_closedir(lua_State *L)
{
	DIR **dirp = luaL_checkudata(L, 1, DIR_METATABLE);
	if (*dirp) {
		lua_pushboolean(L, closedir(*dirp) == 0);
		*dirp = NULL;
	} else
		lua_pushboolean(L, 0);
	return 1;
}

int
luaopen_linux_dirent(lua_State *L)
{
	struct luaL_Reg dirent[] = {
		/* dirent */
		{ "opendir",	linux_opendir },
		{ NULL, NULL }
	};
	struct luaL_Reg dir_methods[] = {
		{ "__gc",	linux_closedir },
		{ "__close",	linux_closedir },
		{ "read",	linux_readdir },
		{ "tell",	linux_telldir },
		{ "seek",	linux_seekdir },
		{ "rewind",	linux_rewinddir },
		{ "close",	linux_closedir },
		{ NULL,		NULL }
	};

	if (luaL_newmetatable(L, DIR_METATABLE)) {
		luaL_setfuncs(L, dir_methods, 0);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);

		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	luaL_newlib(L, dirent);
	return 1;
}
