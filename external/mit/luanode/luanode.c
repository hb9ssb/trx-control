/*
 * Copyright (c) 2014 - 2025 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
 * All rights reserved.
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

/* Lua nodes */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "luanode.h"

struct node_state {
	lua_State *L;
	int nargs;
};

static void *node(void *);

int luaopen_node(lua_State *L);

/* Node functions */
static void
map_table(lua_State *L, lua_State *R, int t, int global)
{
	int top, pop = 0;
	char nam[64];

	switch (lua_type(L, -2)) {
	case LUA_TNUMBER:
		lua_pushnumber(R, lua_tonumber(L, -2));
		snprintf(nam, sizeof nam, "%d", (int)lua_tonumber(L, -2));
		break;
	case LUA_TSTRING:
		snprintf(nam, sizeof nam, "%s", lua_tostring(L, -2));
		lua_pushstring(R, lua_tostring(L, -2));
		break;
	default:
		luaL_error(L, "index must not be %s", luaL_typename(L, -2));
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
			pop = 1;
			map_table(L, R, lua_gettop(R), 0);
			lua_pop(L, 1);
		}
		if (pop)
			lua_pop(L, 1);
		break;
	default:
		luaL_error(L, "value must not be %s", luaL_typename(L, -1));
		return;
	}
	if (global) {
		lua_setglobal(R, nam);
		lua_pop(R, 1);
	} else
		lua_settable(R, t);
}

void
node_openlibs(lua_State *L)
{
	luaL_openlibs(L);
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "preload");
	lua_pushcfunction(L, luaopen_node);
	lua_setfield(L, -2, "node");
	lua_pop(L, 1);
}

static int
node_create(lua_State *L)
{
	lua_State *R;
	pthread_t t;
	int n, top, pop = 0;
	struct node_state *s;

	R = luaL_newstate();
	if (R == NULL)
		return luaL_error(L, "can not create a new Lua state");

	node_openlibs(R);

	if (luaL_loadfile(R, luaL_checkstring(L, 1))) {
		lua_close(R);
		return luaL_error(L, "can not load Lua code from %s",
		    luaL_checkstring(L, 1));
	}

	/* Map arguments, if any, to the new state */
	for (n = 2; n <= lua_gettop(L); n++) {
		switch (lua_type(L, n)) {
		case LUA_TBOOLEAN:
			lua_pushboolean(R, lua_toboolean(L, n));
			break;
		case LUA_TNUMBER:
			if (lua_isinteger(L, n))
				lua_pushinteger(R, lua_tointeger(L, n));
			else
				lua_pushnumber(R, lua_tonumber(L, n));
			break;
		case LUA_TSTRING:
			lua_pushstring(R, lua_tostring(L, n));
			break;
		case LUA_TNIL:
			lua_pushnil(R);
			break;
		case LUA_TTABLE:
			top = n;
			lua_newtable(R);
			lua_pushnil(L);  /* first key */
			while (lua_next(L, top) != 0) {
				pop = 1;
				map_table(L, R, lua_gettop(R), 0);
				lua_pop(L, 1);
			}
			if (pop)
				lua_pop(L, 1);
			break;
		default:
			return luaL_error(L, "argument must not be %s",
			    luaL_typename(L, n));
		}
	}
	s = malloc(sizeof(struct node_state));
	s->L = R;
	s->nargs = n - 2;

	if (pthread_create(&t, NULL, node, s)) {
		lua_close(R);
		free(s);
		return luaL_error(L, "can not create a new node");
	}
	lua_pushinteger(L, (lua_Integer)t);
	return 1;
}

static int
node_id(lua_State *L)
{
	lua_pushnumber(L, (unsigned long)pthread_self());
	lua_pushinteger(L, getpid());
	return 2;
}

static int
node_sleep(lua_State *L)
{
	sleep(luaL_checkinteger(L, 1));
	return 0;
}

static void
node_set_info(lua_State *L)
{
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2014 - 2025 by "
	    "micro systems marc balmer");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "Lua nodes");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "node 1.0.3");
	lua_settable(L, -3);
}

int
luaopen_node(lua_State *L)
{
	struct luaL_Reg functions[] = {
		{ "create",	node_create },
		{ "id",		node_id },
		{ "sleep",	node_sleep },
		{ NULL,		NULL }
	};

	luaL_newlib(L, functions);
	node_set_info(L);
	return 1;
}

static void *
node(void *state)
{
	struct node_state *s = state;

	pthread_detach(pthread_self());

	if (lua_pcall(s->L, s->nargs, 0, 0))
		printf("pcall failed: %s\n", lua_tostring(s->L, -1));
	lua_close(s->L);
	free(s);

	return NULL;
}
