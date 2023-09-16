/*
 * Copyright (C) 2011 - 2022 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
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

/* JSON interface for Lua */

/*
 * This code has been derived from the public domain LuaJSON Library 1.1
 * written by Nathaniel Musgrove (proton.zero@gmail.com), for the original
 * code see http://luaforge.net/projects/luajsonlib/
 */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "buffer.h"

#define JSON_NULL_METATABLE 	"JSON null object"
#define JSON_GCMEM_METATABLE	"JSON garbage collected memory"

static void decode_value(lua_State *, char **, int);
static void decode_string(lua_State *, char **);
static void encode(lua_State *, struct buffer *);

static jmp_buf env;

/*
 * Garbage collected memory
 */
static void *
gcmalloc(lua_State *L, size_t size)
{
	void **p;
	p = lua_newuserdata(L, size);
	*p = NULL;
	luaL_setmetatable(L, JSON_GCMEM_METATABLE);
	return p;
}

/* Memory can be free'ed immediately or left to the garbage collector */
static void
gcfree(void *p)
{
	void **mem = (void **)p;
	free(*mem);
	*mem = NULL;
}

static int
gcmem_clear(lua_State *L)
{
	void **p = luaL_checkudata(L, 1, JSON_GCMEM_METATABLE);

	free(*p);
	*p = NULL;
	return 0;
}

static int
json_error(lua_State *L, const char *fmt, ...)
{
	va_list ap;
	int len;
	char **msg;

	msg = gcmalloc(L, sizeof(char *));
	va_start(ap, fmt);
	len = vasprintf((char **)msg, fmt, ap);
	va_end(ap);

	lua_pushnil(L);
	if (len != -1) {
		lua_pushstring(L, *msg);
		gcfree(msg);
	} else
		lua_pushstring(L, "internal error: vasprintf failed");
	longjmp(env, 0);
}

static unsigned int
digit2int(lua_State *L, const unsigned char digit)
{
	unsigned int val;

	if (digit >= '0' && digit <= '9')
		val = digit - '0';
	else if (digit >= 'a' || digit <= 'f')
		val = digit - 'a' + 10;
	else if (digit >= 'A' || digit <= 'F')
		val = digit - 'A' + 10;
	else
		json_error(L, "Invalid hex digit");
	return val;
}

static unsigned int
fourhex2int(lua_State *L, const unsigned char *code)
{
	unsigned int utf = 0;

	utf += digit2int(L, code[0]) * 4096;
	utf += digit2int(L, code[1]) * 256;
	utf += digit2int(L, code[2]) * 16;
	utf += digit2int(L, code[3]);
	return utf;
}

static const char *
code2utf8(lua_State *L, const unsigned char *code, char buf[4])
{
	unsigned int utf = 0;

	utf = fourhex2int(L, code);
	if (utf < 128) {
		buf[0] = utf & 0x7F;
		buf[1] = buf[2] = buf[3] = 0;
	} else if (utf < 2048) {
		buf[0] = ((utf >> 6) & 0x1F) | 0xC0;
		buf[1] = (utf & 0x3F) | 0x80;
		buf[2] = buf[3] = 0;
	} else if (utf < 65536) {
		buf[0] = ((utf >> 12) & 0x0F) | 0xE0;
		buf[1] = ((utf >> 6) & 0x3F) | 0x80;
		buf[2] = (utf & 0x3F) | 0x80;
		buf[3] = 0;
	} else {
		buf[0] = ((utf >> 18) & 0x07) | 0xF0;
		buf[1] = ((utf >> 12) & 0x3F) | 0x80;
		buf[2] = ((utf >> 6) & 0x3F) | 0x80;
		buf[3] = (utf & 0x3F) | 0x80;
	}
	return buf;
}

static void
skip_ws(char **s)
{
	while (isspace(**s))
		(*s)++;
}

static void
decode_array(lua_State *L, char **s, int null)
{
	int i = 1;

	luaL_checkstack(L, 2, "Out of stack space");
	(*s)++;
	lua_newtable(L);
	for (i = 1; 1; i++) {
		skip_ws(s);
		lua_pushinteger(L, i);
		decode_value(L, s, null);
		lua_settable(L, -3);
		skip_ws(s);
		if (**s == ',') {
			(*s)++;
			skip_ws(s);
		} else
			break;
	}
	skip_ws(s);
	if (**s == ']')
		(*s)++;
	else
		json_error(L, "array does not end with ']'");
}

static void
decode_object(lua_State *L, char **s, int null)
{
	luaL_checkstack(L, 1, "Out of stack space");

	(*s)++;
	lua_newtable(L);
	skip_ws(s);

	if (**s != '}') {
		while (1) {
			skip_ws(s);
			decode_string(L, s);
			skip_ws(s);
			if (**s != ':')
				json_error(L, "object lacks separator ':'");
			(*s)++;
			skip_ws(s);
			decode_value(L, s, null);
			lua_settable(L, -3);
			skip_ws(s);
			if (**s == ',') {
				(*s)++;
				skip_ws(s);
			} else
				break;
		}
		skip_ws(s);
	}
	if (**s == '}')
		(*s)++;
	else
		json_error(L, "objects does not end with '}'");
}

static void
decode_string(lua_State *L, char **s)
{
	size_t len;
	char *newstr = NULL, *newc, *beginning, *end, *nextEscape = NULL;
	char utfbuf[4] = "";

	luaL_checkstack(L, 1, "Out of stack space");
	(*s)++;
	beginning = *s;
	for (end = NULL; **s != '\0' && end == NULL; (*s)++) {
		if (**s == '"' && (*((*s) - 1) != '\\'))
			end = *s;
	}
	if (end == NULL)
		return;
	*s = beginning;
	len = strlen(*s);
	if ((newstr = malloc(len + 1)) == NULL)
		json_error(L, "memory error");
	memset(newstr, 0, len + 1);
	newc = newstr;
	while (*s != end) {
		nextEscape = strchr(*s, '\\');
		if (nextEscape > end)
			nextEscape = NULL;
		if (nextEscape == *s) {
			switch (*((*s) + 1)) {
			case '"':
				*newc = '"';
				newc++;
				(*s) += 2;
				break;
			case '\\':
				*newc = '\\';
				newc++;
				(*s) += 2;
				break;
			case '/':
				*newc = '/';
				newc++;
				(*s) += 2;
				break;
			case 'b':
				*newc = '\b';
				newc++;
				(*s) += 2;
				break;
			case 'f':
				*newc = '\f';
				newc++;
				(*s) += 2;
				break;
			case 'n':
				*newc = '\n';
				newc++;
				(*s) += 2;
				break;
			case 'r':
				*newc = '\r';
				newc++;
				(*s) += 2;
				break;
			case 't':
				*newc = '\t';
				newc++;
				(*s) += 2;
				break;
			case 'u':
				code2utf8(L, (unsigned char *)(*s) + 2, utfbuf);
				len = strlen(utfbuf);
				strcpy(newc, utfbuf);
				newc += len;
				(*s) += 6;
				break;
			default:
				json_error(L, "invalid escape character");
				break;
			}
		} else if (nextEscape != NULL) {
			len = nextEscape - *s;
			strncpy(newc, *s, len);
			newc += len;
			(*s) += len;
		} else {
			len = end - *s;
			strncpy(newc, *s, len);
			newc += len;
			(*s) += len;
		}
	}
	*newc = 0;
	lua_pushstring(L, newstr);
	(*s)++;
	free(newstr);
}

static void
decode_value(lua_State *L, char **s, int null)
{
	skip_ws(s);

	luaL_checkstack(L, 1, "Out of stack space");
	if (!strncmp(*s, "false", 5)) {
		lua_pushboolean(L, 0);
		*s += 5;
	} else if (!strncmp(*s, "true", 4)) {
		lua_pushboolean(L, 1);
		*s += 4;
	} else if (!strncmp(*s, "null", 4)) {
		switch (null) {
		case 0:
			lua_pushstring(L, "");
			break;
		case 1:
			lua_newtable(L);
			luaL_setmetatable(L, JSON_NULL_METATABLE);
			break;
		case 2:
			lua_pushnil(L);
			break;
		}
		*s += 4;
	} else if (isdigit(**s) || **s == '+' || **s == '-') {
		char *p = *s;
		int isfloat = 0;

		/* advance pointer past the number */
		while (isdigit(**s) || **s == '+' || **s == '-'
		    || **s == 'e' || **s == 'E' || **s == '.') {
		    	if (**s == 'e' || **s == 'E' || **s == '.')
		    		isfloat = 1;
			(*s)++;
		}
		if (isfloat)
			lua_pushnumber(L, atof(p));
		else
			lua_pushinteger(L, atol(p));
	} else {
		switch (**s) {
		case '[':
			decode_array(L, s, null);
			break;
		case '{':
			decode_object(L, s, null);
			break;
		case '"':
			decode_string(L, s);
			break;
		case ']':	/* ignore end of empty array */
			lua_pushnil(L);
			break;
		default:
			json_error(L, "syntax error");
			break;
		}
	}
}

static int
json_decode(lua_State *L)
{
	char *s;
	int null;
	const char *const options[] = {
		"empty-string",
		"json-null",
		"nil",
		NULL
	};

	s = (char *)luaL_checkstring(L, 1);

	null = luaL_checkoption(L, 2, "json-null", options);

	if (!setjmp(env)) {
		decode_value(L, &s, null);
		return 1;
	} else
		return 2;
}

/* encode JSON */

/* encode_string assumes an UTF-8 string */
static void
encode_string(lua_State *L, struct buffer *b, unsigned char *s)
{
	char hexbuf[6];

	buf_addchar(b, '"');
	for (; *s; s++) {
		switch (*s) {
		case '\\':
			buf_addstring(b, "\\\\");
			break;
		case '"':
			buf_addstring(b, "\\\"");
			break;
		case '\b':
			buf_addstring(b, "\\b");
			break;
		case '\f':
			buf_addstring(b, "\\f");
			break;
		case '\n':
			buf_addstring(b, "\\n");
			break;
		case '\r':
			buf_addstring(b, "\\r");
			break;
		case '\t':
			buf_addstring(b, "\\t");
			break;
		default:
		/* Convert UTF-8 to unicode
		 * 00000000 - 0000007F: 0xxxxxxx
		 * 00000080 - 000007FF: 110xxxxx 10xxxxxx
		 * 00000800 - 0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx
		 * 00010000 - 001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		 */
			if ((*s & 0x80) == 0)
				buf_addchar(b, *s);
			else if (((*s >> 5) & 0x07) == 0x06) {
				buf_addstring(b, "\\u");
				snprintf(hexbuf, sizeof hexbuf, "%04x",
				    ((*s & 0x1f) << 6) | (*(s + 1) & 0x3f));
				buf_addstring(b, hexbuf);
				s++;
			} else if (((*s >> 4) & 0x0f) == 0x0e) {
				buf_addstring(b, "\\u");
				snprintf(hexbuf, sizeof hexbuf, "%04x",
				    ((*s & 0x0f) << 12) |
				    ((*(s + 1) & 0x3f) << 6) |
				    (*(s + 2) & 0x3f));
				buf_addstring(b, hexbuf);
				s += 2;
			} else if (((*s >> 3) & 0x1f) == 0x1e) {
				buf_addstring(b, "\\u");
				snprintf(hexbuf, sizeof hexbuf, "%04x",
				    ((*s & 0x07) << 18) |
				    ((*(s + 1) & 0x3f) << 12) |
				    ((*(s + 2) & 0x3f) << 6) |
				    (*(s + 3) & 0x3f));
				buf_addstring(b, hexbuf);
				s += 3;
			}
			break;
		}
	}
	buf_addchar(b, '"');
}

static void
encode(lua_State *L, struct buffer *b)
{
	int e, t, n, m;

	switch (lua_type(L, -1)) {
	case LUA_TBOOLEAN:
		buf_addstring(b, lua_toboolean(L, -1) ? "true" : "false");
		lua_pop(L, 1);
		break;
	case LUA_TNUMBER:
		buf_addstring(b, lua_tostring(L, -1));
		lua_pop(L, 1);
		break;
	case LUA_TSTRING:
		encode_string(L, b, (unsigned char *)lua_tostring(L, -1));
		lua_pop(L, 1);
		break;
	case LUA_TTABLE:
		/* check if this is the null value */
		luaL_checkstack(L, 2, "out of stack space");
		if (lua_getmetatable(L, -1)) {
			luaL_getmetatable(L, JSON_NULL_METATABLE);
			if (lua_compare(L, -2, -1, LUA_OPEQ)) {
				lua_pop(L, 2);
				buf_addstring(b, "null");
				lua_pop(L, 1);
				break;
			}
			lua_pop(L, 2);
		}
		/* if there are t[1] .. t[n], output them as array */
		n = 0;
		e = 1;
		t = lua_gettop(L);
		for (m = 1; ; m++) {
			lua_geti(L, t, m);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				break;
			}
			buf_addchar(b, n ? ',' : '[');
			encode(L, b);
			n++;
		}
		if (n) {
			buf_addchar(b, ']');
			lua_pop(L, 1);
			e = 0;
			break;
		}

		/* output non-numerical indices as object */
		t = lua_gettop(L);
		lua_pushnil(L);
		n = 0;
		while (lua_next(L, t) != 0) {
			int key /*, value */;

			key = lua_absindex(L, -2);
			/* value = lua_absindex(L, -1); */

			if (lua_type(L, -2) == LUA_TNUMBER) {
				lua_pop(L, 1);
				continue;
			}
			buf_addstring(b, n ? ",\"" : "{\"");
			buf_addstring(b, lua_tostring(L, -2));
			buf_addstring(b, "\":");
			encode(L, b);
			lua_pushvalue(L, key);
			/* lua_remove(L, value); */
			/* lua_remove(L, key); */
			n++;
		}
		if (n) {
			buf_addchar(b, '}');
			e = 0;
		}

		if (e)
			buf_addstring(b, "[]");
		lua_pop(L, 1);
		break;
	case LUA_TNIL:
		buf_addstring(b, "null");
		lua_pop(L, 1);
		break;
	default:
		json_error(L, "Lua type %s is incompatible with JSON",
		    luaL_typename(L, -1));
		lua_pop(L, 1);
	}
}

static int
json_encode(lua_State *L)
{
	struct buffer b;

	buf_init(&b);
	encode(L, &b);
	buf_push(&b, L);
	buf_free(&b);
	return 1;
}

static int
json_isnull(lua_State *L)
{
	if (lua_getmetatable(L, -1)) {
		luaL_getmetatable(L, JSON_NULL_METATABLE);
		if (lua_compare(L, -2, -1, LUA_OPEQ)) {
			lua_pop(L, 2);
			lua_pushboolean(L, 1);
			goto done;
		}
		lua_pop(L, 2);
	}
	lua_pushboolean(L, 0);
done:
	return 1;
}

static int
json_null(lua_State *L)
{
	lua_pushstring(L, "null");
	return 1;
}

int
luaopen_json(lua_State* L)
{
	static const struct luaL_Reg methods[] = {
		{ "decode",	json_decode },
		{ "encode",	json_encode },
		{ "isnull",	json_isnull },
		{ NULL,		NULL }
	};
	static const struct luaL_Reg null_methods[] = {
		{ "__tostring",	json_null },
		{ "__call",	json_null },
		{ NULL,		NULL }
	};
	luaL_newlib(L, methods);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2011 - 2022 "
	    "micro systems marc balmer");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "JSON encoder/decoder for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "json 1.3.0");
	lua_settable(L, -3);

	lua_newtable(L);

	/* The null metatable */
	if (luaL_newmetatable(L, JSON_NULL_METATABLE)) {
		luaL_setfuncs(L, null_methods, 0);
		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);

	}
	lua_setmetatable(L, -2);

	if (luaL_newmetatable(L, JSON_GCMEM_METATABLE)) {
		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, gcmem_clear);
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	lua_setfield(L, -2, "null");
	return 1;
}
