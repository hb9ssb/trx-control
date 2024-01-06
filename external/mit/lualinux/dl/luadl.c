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

/* Dynamic linker interface for Lua */

#include <lua.h>
#include <lauxlib.h>
#include <dlfcn.h>

#include "luadl.h"

static int dlopen_flags[] = {
	RTLD_LAZY,
	RTLD_NOW,
	RTLD_GLOBAL,
	RTLD_LOCAL,
	RTLD_NODELETE,
	RTLD_NOLOAD,
#ifdef __GLIBC__
	RTLD_DEEPBIND
#endif
};

static const char *dlopen_options[] = {
	"lazy",
	"now",
	"global",
	"local",
	"nodelete",
	"noload",
#ifdef __GLIBC__
	"deepbind",
#endif
	NULL
};

int
linux_dlopen(lua_State *L)
{
	void *p, **u;
	int n, flags;

	for (flags = 0, n = 2; n <= lua_gettop(L); n++)
		flags |= dlopen_flags[luaL_checkoption(L, n, NULL,
		    dlopen_options)];
	if (!(p = dlopen(luaL_checkstring(L, 1), flags))) {
		lua_pushnil(L);
		lua_pushstring(L, dlerror());
		return 2;
	} else {
		u = lua_newuserdata(L, sizeof(void **));
		*u = p;
		luaL_setmetatable(L, DL_METATABLE);
	}
	return 1;
}

int
linux_dlerror(lua_State *L)
{
	lua_pushstring(L, dlerror());
	return 1;
}

int
linux_dlsym(lua_State *L)
{
	void **p, **s, *symbol;

	p = luaL_checkudata(L, 1, DL_METATABLE);
	if (!(symbol = dlsym(*p, luaL_checkstring(L, 2)))) {
		lua_pushnil(L);
		lua_pushstring(L, dlerror());
		return 2;
	} else {
		s = lua_newuserdata(L, sizeof(void **));
		*s = symbol;
		luaL_setmetatable(L, DLSYM_METATABLE);
	}
	return 1;
}

int
linux_dlclose(lua_State *L)
{
	void **p = luaL_checkudata(L, 1, DL_METATABLE);

	if (*p) {
		lua_pushboolean(L, dlclose(*p) == 0 ? 1 : 0);
		*p = NULL;
		return 1;
	} else
		return 0;
}

int
luaopen_linux_dl(lua_State *L)
{
	struct luaL_Reg lualinuxdl[] = {
		/* dynamic linker */
		{ "open",	linux_dlopen },
		{ "error",	linux_dlerror },
		{ "sym",	linux_dlsym },
		{ "close",	linux_dlclose },
		{ NULL, NULL }
	};
	struct luaL_Reg dl_methods[] = {
		{ "__gc",	linux_dlclose },
		{ "__close",	linux_dlclose },
		{ "__index",	linux_dlsym },
		{ NULL,		NULL }
	};
	if (luaL_newmetatable(L, DL_METATABLE)) {
		luaL_setfuncs(L, dl_methods, 0);
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);
	if (luaL_newmetatable(L, DLSYM_METATABLE)) {
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	luaL_newlib(L, lualinuxdl);
	return 1;
}
