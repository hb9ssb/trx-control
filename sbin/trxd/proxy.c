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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

void
proxy_map(lua_State *L, lua_State *R, int t)
{
	int top, istable = 0;
	char nam[64];

	luaL_checkstack(R, 32, "out of stack space");
	switch (lua_type(L, -2)) {
	case LUA_TNUMBER:
		lua_pushnumber(R, lua_tonumber(L, -2));
		snprintf(nam, sizeof nam, "%d", (int)lua_tonumber(L, -2));
		break;
	case LUA_TSTRING:
		snprintf(nam, sizeof nam, "%s", lua_tostring(L, -2));
		lua_pushstring(R, lua_tostring(L, -2));
		break;
	case LUA_TTABLE:
	case LUA_TNIL:
		istable = 1;
		break;
	default:
		luaL_error(L, "proxy: data type '%s' is not "
		    "supported as an index value", luaL_typename(L, -2));
		return;
	}
	switch (lua_type(L, -1)) {
	case LUA_TBOOLEAN:
		lua_pushboolean(R, lua_toboolean(L, -1));
		break;
	case LUA_TNUMBER:
		if (lua_isinteger(L, -1))
			lua_pushinteger(R, lua_tointeger(L, -1));
		else
			lua_pushnumber(R, lua_tonumber(L, -1));
		break;
	case LUA_TSTRING:
		lua_pushstring(R, lua_tostring(L, -1));
		break;
	case LUA_TNIL:
		lua_pushnil(R);
		break;
	case LUA_TTABLE:
		top = lua_gettop(L);
		lua_newtable(R);
		lua_pushnil(L);  /* first key */
		while (lua_next(L, top) != 0) {
			proxy_map(L, R, lua_gettop(R));
			lua_pop(L, 1);
		}
		break;
	default:
		printf("unknown type %s\n", nam);
	}
	if (!istable)
		lua_settable(R, t);
}
