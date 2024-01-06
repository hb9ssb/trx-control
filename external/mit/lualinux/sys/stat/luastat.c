/*
 * Copyright (c) 2023 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
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

/* Lua binding for Linux */

#include <sys/stat.h>

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <unistd.h>

static int
linux_stat(lua_State *L)
{
	struct stat statbuf;

	if (stat(luaL_checkstring(L, 1), &statbuf))
		lua_pushnil(L);
	else {
		lua_newtable(L);
		lua_pushinteger(L, statbuf.st_dev);
		lua_setfield(L, -2, "st_uid");
		lua_pushinteger(L, statbuf.st_ino);
		lua_setfield(L, -2, "st_ino");
		lua_pushinteger(L, statbuf.st_mode);
		lua_setfield(L, -2, "st_mode");
		lua_pushinteger(L, statbuf.st_nlink);
		lua_setfield(L, -2, "st_nlink");
		lua_pushinteger(L, statbuf.st_uid);
		lua_setfield(L, -2, "st_uid");
		lua_pushinteger(L, statbuf.st_gid);
		lua_setfield(L, -2, "st_gid");
		lua_pushinteger(L, statbuf.st_rdev);
		lua_setfield(L, -2, "st_rdev");
		lua_pushinteger(L, statbuf.st_size);
		lua_setfield(L, -2, "st_size");
		lua_pushinteger(L, statbuf.st_blksize);
		lua_setfield(L, -2, "st_blksize");
		lua_pushinteger(L, statbuf.st_blocks);
		lua_setfield(L, -2, "st_blocks");
		lua_pushinteger(L, statbuf.st_atime);
		lua_setfield(L, -2, "st_atime");
		lua_pushinteger(L, statbuf.st_mtime);
		lua_setfield(L, -2, "st_mtime");
		lua_pushinteger(L, statbuf.st_ctime);
		lua_setfield(L, -2, "st_ctime");
	}
	return 1;
}

static int
linux_lstat(lua_State *L)
{
	struct stat statbuf;

	if (lstat(luaL_checkstring(L, 1), &statbuf))
		lua_pushnil(L);
	else {
		lua_newtable(L);
		lua_pushinteger(L, statbuf.st_dev);
		lua_setfield(L, -2, "st_uid");
		lua_pushinteger(L, statbuf.st_ino);
		lua_setfield(L, -2, "st_ino");
		lua_pushinteger(L, statbuf.st_mode);
		lua_setfield(L, -2, "st_mode");
		lua_pushinteger(L, statbuf.st_nlink);
		lua_setfield(L, -2, "st_nlink");
		lua_pushinteger(L, statbuf.st_uid);
		lua_setfield(L, -2, "st_uid");
		lua_pushinteger(L, statbuf.st_gid);
		lua_setfield(L, -2, "st_gid");
		lua_pushinteger(L, statbuf.st_rdev);
		lua_setfield(L, -2, "st_rdev");
		lua_pushinteger(L, statbuf.st_size);
		lua_setfield(L, -2, "st_size");
		lua_pushinteger(L, statbuf.st_blksize);
		lua_setfield(L, -2, "st_blksize");
		lua_pushinteger(L, statbuf.st_blocks);
		lua_setfield(L, -2, "st_blocks");
		lua_pushinteger(L, statbuf.st_atime);
		lua_setfield(L, -2, "st_atime");
		lua_pushinteger(L, statbuf.st_mtime);
		lua_setfield(L, -2, "st_mtime");
		lua_pushinteger(L, statbuf.st_ctime);
		lua_setfield(L, -2, "st_ctime");
	}
	return 1;
}

int
luaopen_linux_sys_stat(lua_State *L)
{
	struct luaL_Reg luastat[] = {
		{ "stat",	linux_stat },
		{ "lstat" ,	linux_lstat },
		{ NULL, NULL }
	};

	luaL_newlib(L, luastat);
	return 1;
}
