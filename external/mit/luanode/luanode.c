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
#include <signal.h>

#include "luanode.h"

static void *node_handler(void *);

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
	luaopen_node(L);
	lua_setglobal(L, "node");
}

static int
node_create(lua_State *L)
{
	int n, top, pop = 0;
	node_t *child, **node;
	static __thread node_t *parent = NULL;

	child = malloc(sizeof(node_t));
	child->L = luaL_newstate();
	child->killable = 1;

	luaL_setmetatable(L, NODE_METATABLE);
	if (L == NULL)
		return luaL_error(L, "can not create a new Lua state");

	node_openlibs(child->L);

	if (luaL_loadfile(child->L, luaL_checkstring(L, 1))) {
		lua_close(child->L);
		return luaL_error(L, "can not load Lua code from %s",
		    luaL_checkstring(L, 1));
	}

	if (parent == NULL) {
		parent = malloc(sizeof(node_t));
		parent->L = L;
		parent->handler = pthread_self();
		parent->killable = 0;
	}

	/* Push the parent node as first argument */
	node = lua_newuserdata(child->L, sizeof(node_t *));
	*node = parent;
 	luaL_setmetatable(child->L, NODE_METATABLE);

	/* Map arguments, if any, to the new state */
	for (n = 2; n <= lua_gettop(L); n++) {
		switch (lua_type(L, n)) {
		case LUA_TBOOLEAN:
			lua_pushboolean(child->L, lua_toboolean(L, n));
			break;
		case LUA_TNUMBER:
			if (lua_isinteger(L, n))
				lua_pushinteger(child->L, lua_tointeger(L, n));
			else
				lua_pushnumber(child->L, lua_tonumber(L, n));
			break;
		case LUA_TSTRING:
			lua_pushstring(child->L, lua_tostring(L, n));
			break;
		case LUA_TNIL:
			lua_pushnil(child->L);
			break;
		case LUA_TTABLE:
			top = n;
			lua_newtable(child->L);
			lua_pushnil(L);  /* first key */
			while (lua_next(L, top) != 0) {
				pop = 1;
				map_table(L, child->L, lua_gettop(child->L), 0);
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
	child->nargs = n - 1;

	node = lua_newuserdata(L, sizeof(node_t *));
	*node = child;
	luaL_setmetatable(L, NODE_METATABLE);

	if (pthread_create(&child->handler, NULL, node_handler, child)) {
		lua_close(child->L);
		return luaL_error(L, "can not create a new node");
	}

	return 1;
}

static int
node_cancel(lua_State *L)
{
	node_t *node = *(node_t **)luaL_checkudata(L, 1, NODE_METATABLE);

	if (node->killable)
		lua_pushboolean(L, pthread_cancel(node->handler) == 0);
	else
		lua_pushboolean(L, 0);
	return 1;
}

int
luaopen_node(lua_State *L)
{
	struct luaL_Reg functions[] = {
		{ "create",	node_create },
		{ NULL,		NULL }
	};
	struct luaL_Reg node_methods[] = {
		{ "cancel",	node_cancel },
		{ NULL, NULL }
	};
	int n;

	if (luaL_newmetatable(L, NODE_METATABLE)) {
		luaL_setfuncs(L, node_methods, 0);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);

		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	luaL_newlib(L, functions);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2014 - 2025 by "
	    "micro systems marc balmer");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "Lua nodes");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "node 1.1.0");
	lua_settable(L, -3);
	return 1;
}

static void
cleanup(void *arg)
{
	node_t *n = arg;

	lua_close(n->L);
	free(arg);
}

static void *
node_handler(void *arg)
{
	node_t *n = arg;

	pthread_detach(pthread_self());
	pthread_cleanup_push(cleanup, arg);

	if (lua_pcall(n->L, n->nargs, 0, 0)) {
		printf("pcall failed: %s\n", lua_tostring(n->L, -1));
		pthread_exit(NULL);
	}
	lua_close(n->L);

	pthread_cleanup_pop(0);
	return NULL;
}
