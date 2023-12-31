/*
 * Copyright (c) 2023 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
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

/* Expat for Lua */

#include <expat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "buffer.h"
#include "luaexpat.h"

static void
data(void *data, const char *cdata, int len)
{
	lua_State *L = (lua_State *)data;

	lua_pushlstring(L, cdata, len);
	lua_setfield(L, -2, FIELD_TEXT);
}

static void
start_element(void *data, const char *element, const char **attribute)
{
	lua_State *L = (lua_State *)data;
	static int n = 1;

	lua_newtable(L);

	if (*attribute) {
		lua_newtable(L);
		while (*attribute) {
			lua_pushstring(L, *(attribute + 1));
			lua_setfield(L, -2, *attribute);
			attribute += 2;
		}
		lua_setfield(L, -2, FIELD_ATTR);
	}
}

static void
end_element(void *data, const char *element)
{
	lua_State *L = (lua_State *)data;

	lua_setfield(L, -2, element);
}

static int
luaexpat_decode(lua_State *L)
{
	size_t nbytes;
	const char *xml;
	XML_Parser parser;

	xml = luaL_checklstring(L, 1, &nbytes);

	lua_newtable(L);

	parser = XML_ParserCreate(NULL);
	XML_SetElementHandler(parser, start_element, end_element);
	XML_SetCharacterDataHandler(parser, data);
	XML_SetUserData(parser, L);
	if (XML_Parse(parser, xml, nbytes, XML_TRUE) == XML_STATUS_ERROR) {
		warnx("XML error: %s\n",
		    XML_ErrorString(XML_GetErrorCode(parser)));
		lua_pop(L, 1);
		lua_pushnil(L);
	}
	XML_ParserFree(parser);
	return 1;
}

static void
encode(lua_State *L, struct buffer *b, const char *name)
{
	int t;
	int has_text_or_children;

	if (lua_type(L, -1) == LUA_TBOOLEAN && !lua_toboolean(L, -1))
		return;

	has_text_or_children = 0;
	buf_addstring(b, "<");
	buf_addstring(b, name);

	if (lua_type(L, -1) == LUA_TTABLE) {
		lua_getfield(L, -1, "xmlattr");
		if (lua_type(L, -1) == LUA_TTABLE) {
			int n = lua_gettop(L);
			lua_pushnil(L);
			while (lua_next(L, n) != 0) {
				buf_addstring(b, " ");
				buf_addstring(b, lua_tostring(L, -2));
				buf_addstring(b, "=\"");
				buf_addstring(b, lua_tostring(L, -1));
				buf_addstring(b, "\"");
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}

	switch (lua_type(L, -1)) {
	case LUA_TTABLE:
		lua_getfield(L, -1, "xmltext");
		if (lua_type(L, -1) != LUA_TNIL) {
			buf_addstring(b, ">");
			buf_addstring(b, lua_tostring(L, -1));
			has_text_or_children = 1;
		}
		lua_pop(L, 1);

		t = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, t) != 0) {
			if (strncmp(lua_tostring(L, -2), "xml", 3)) {
				if (!has_text_or_children) {
					buf_addstring(b, ">");
					has_text_or_children = 1;
				}
				if (lua_type(L, -1) == LUA_TTABLE) {
					int key;

					key = lua_absindex(L, -2);
					encode(L, b, lua_tostring(L, -2));
					lua_pushvalue(L, key);
				} else
					encode(L, b, lua_tostring(L, -2));
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		if (has_text_or_children) {
			buf_addstring(b, "</");
			buf_addstring(b, name);
			buf_addstring(b, ">");
		} else
			buf_addstring(b, "/>");
		break;
	case LUA_TBOOLEAN:
		buf_addstring(b, "/>");
		break;
	default:
		buf_addstring(b, ">");
		buf_addstring(b, lua_tostring(L, -1));
		buf_addstring(b, "</");
		buf_addstring(b, name);
		buf_addstring(b, ">");
	}
}

static int
luaexpat_encode(lua_State *L)
{
	struct buffer b;
	int t;

	buf_init(&b);
	buf_addstring(&b, "<?xml version=\"1.0\"");
	if (lua_type(L, 2) == LUA_TSTRING) {
		buf_addstring(&b, " encoding=\"");
		buf_addstring(&b, lua_tostring(L, 2));
		lua_pop(L, 1);
	}
	buf_addstring(&b, "\"?>");

	t = lua_gettop(L);
	lua_pushnil(L);
	lua_next(L, t);
	encode(L, &b, lua_tostring(L, -2));
	buf_push(&b, L);
	buf_free(&b);
	return 1;
}

int
luaopen_expat(lua_State *L)
{
	struct luaL_Reg luaexpat[] = {
		{ "decode",		luaexpat_decode },
		{ "encode",		luaexpat_encode },
		{ NULL,			NULL }
	};

	luaL_newlib(L, luaexpat);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2023 "
	    "micro systems marc balmer");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "Expat for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "expat 1.0.0");
	lua_settable(L, -3);

	return 1;
}
