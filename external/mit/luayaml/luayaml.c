/*
 * Copyright (C) 2018 - 2025 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
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

/* YAML for Lua */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <lua.h>
#include <lauxlib.h>
#include <yaml.h>

#include "luayaml.h"
#include "anchor.h"

#ifdef DEBUG
static int verbose = 0;

static const char *events[] = {
	"YAML_NO_EVENT",
	"YAML_STREAM_START_EVENT",
	"YAML_STREAM_END_EVENT",
	"YAML_DOCUMENT_START_EVENT",
	"YAML_DOCUMENT_END_EVENT",
	"YAML_ALIAS_EVENT",
	"YAML_SCALAR_EVENT",
	"YAML_SEQUENCE_START_EVENT",
	"YAML_SEQUENCE_END_EVENT",
	"YAML_MAPPING_START_EVENT",
	"YAML_MAPPING_END_EVENT"
};

#define event_name(event) \
	(event > 0 && event < sizeof(events)/sizeof(events[0]) ? \
	events[event] : "unknown event type")

#define dprintf(level, fmt, ...) \
	do { if (verbose >= level) fprintf(stderr, fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define dprintf(level, fmt, ...)
#endif

static int parse_error(lua_State *, yaml_parser_t *);
static int parse_mapping(lua_State *, yaml_parser_t *, anchor_t *, int);
static int parse_sequence(lua_State *, yaml_parser_t *, anchor_t *, int);
static int parse_node(lua_State *, yaml_parser_t *, anchor_t *, yaml_event_t,
    int, int);
static int parse_document(lua_State *, yaml_parser_t *, anchor_t *, int);
static int parse_stream(lua_State *, yaml_parser_t *, anchor_t *, int);
static int parse(lua_State *, yaml_parser_t *, anchor_t *, int);

static int
push_boolean(lua_State *L, char *v, int l) {
	if (!strncasecmp(v, "no", l)
	    || !strncasecmp(v, "false", l)
	    || !strncasecmp(v, "off", l)) {
		lua_pushboolean(L, 0);
		return 0;
	} else if (!strncasecmp(v, "yes", l)
	    || !strncasecmp(v, "true", l)
	    || !strncasecmp(v, "on", l)) {
		lua_pushboolean(L, 1);
		return 0;
	} else
		return -1;
}

static int
push_null(lua_State *L, char *v, int l)
{
	if (!strncmp(v, "null", l) || !strncmp(v, "~", l)) {
		lua_pushnil(L);
		return 0;
	} else
		return -1;
}

static int
push_integer(lua_State *L, char *v, int l)
{
	int n = 0;

	if (*v == '+' || *v == '-')
		n++;
	for (; n < l; n++)
		if (!isdigit(v[n]))
			return -1;
	lua_pushinteger(L, atoi(v));
	return 0;
}

static int
push_hexadecimal(lua_State *L, char *v, int l)
{
	int n;

	if (l > 2) {
		if (strncmp(v, "0x", 2))
			return -1;
		for (n = 2; n < l; n++)
			if (!isxdigit(v[n]))
				return -1;
		lua_pushinteger(L, strtol(v, NULL, 16));
		return 0;
	}
	return -1;
}

static void
push_scalar(lua_State *L, char *value, int style, int length, char *tag,
    int env)
{
	if (tag != NULL) {
		if (!strcmp(tag, YAML_BOOL_TAG)) {
			if (push_boolean(L, value, length) < 0)
				lua_pushnil(L);
		} else if (!strcmp(tag, YAML_INT_TAG)) {
			if (push_hexadecimal(L, value, length))
				if (push_integer(L, value, length))
					lua_pushnil(L);
		} else if (!strcmp(tag, YAML_NULL_TAG))
			lua_pushnil(L);
		else if (!strcmp(tag, YAML_FLOAT_TAG))
			lua_pushnumber(L, atof(value));
		else if (!strcmp(tag, YAML_STR_TAG))
			lua_pushlstring(L, value, length);
		else if (!strcmp(tag, LUAYAML_LUA_LOADFILE_TAG)) {
			luaL_loadfile(L, value);
			if (env > 0) {
				lua_pushvalue(L, env);
				lua_setupvalue(L, -2, 1);
			}
		} else if (!strcmp(tag, LUAYAML_LUA_LOAD_TAG)) {
			luaL_loadstring(L, value);
			if (env > 0) {
				lua_pushvalue(L, env);
				lua_setupvalue(L, -2, 1);
			}
		} else if (!strcmp(tag, LUAYAML_LUA_CALL_TAG)) {
			luaL_loadstring(L, value);
			if (env > 0) {
				lua_pushvalue(L, env);
				lua_setupvalue(L, -2, 1);
			}
			lua_call(L, 0, 1);
		} else if (!strcmp(tag, LUAYAML_LUA_CALLFILE_TAG)) {
			luaL_loadfile(L, value);
			if (env > 0) {
				lua_pushvalue(L, env);
				lua_setupvalue(L, -2, 1);
			}
			lua_call(L, 0, 1);
		} else if (!strcmp(tag, LUAYAML_FILE_TAG)) {
			int fd;
			unsigned char *content;
			struct stat sb;

			if (stat(value, &sb))
				luaL_error(L, "can't stat %s", value);

			if ((fd = open(value, O_RDONLY)) == -1)
				luaL_error(L, "can't open %s", value);

			if ((content = mmap(0, sb.st_size, PROT_READ,
			    MAP_PRIVATE, fd, (off_t)0L)) == MAP_FAILED)
				luaL_error(L, "can't mmap %s", value);

			lua_pushlstring(L, (const char *)content, sb.st_size);
			munmap(content, sb.st_size);
			close(fd);
		} else
			lua_pushlstring(L, value, length);
	} else {
		if (style == YAML_SINGLE_QUOTED_SCALAR_STYLE ||
		    style == YAML_DOUBLE_QUOTED_SCALAR_STYLE)
			lua_pushlstring(L, value, length);
		else {
			if (!push_boolean(L, value, length))
				return;
			if (!push_null(L, value, length))
				return;
			if (!push_integer(L, value, length))
				return;
			if (!push_hexadecimal(L, value, length))
				return;
			lua_pushlstring(L, value, length);
		}
	}
}

static int
parse_error(lua_State *L, yaml_parser_t *parser)
{
	return luaL_error(L, "line %d: %s", parser->problem_mark.line,
	    parser->problem);
}

static int
parse_node(lua_State *L, yaml_parser_t *parser, anchor_t *anchors,
    yaml_event_t event, int value, int env)
{
	int ref;

	lua_checkstack(L, 16);

	/* Process event */
	switch (event.type) {
	case YAML_ALIAS_EVENT:
		ref = anchor_get(anchors, event.data.alias.anchor);
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		if (lua_type(L, -1) != LUA_TTABLE) {
			/* The value is already on the top of the stack */
			return 0;
		} else {
			/* Copy the table */
			int t = lua_gettop(L);
			lua_pushnil(L);
			while (lua_next(L, t) != 0) {
				lua_pushvalue(L, -2);
				lua_pushvalue(L, -2);
				lua_settable(L, -6);
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
		}
		return 2;
		break;
	case YAML_SCALAR_EVENT:
		if (!strcmp((char *)event.data.scalar.value, "<<")) {
			/* push nothing on the stack */
			return 1;
		}
		if (value) {
			push_scalar(L, (char *)event.data.scalar.value,
			    event.data.scalar.style,
			    event.data.scalar.length,
			    (char *)event.data.scalar.tag, env);
		} else {
			lua_pushlstring(L,
			    (char *)event.data.scalar.value,
			    event.data.scalar.length);
		}
		dprintf(1, "value: %s", event.data.scalar.value);
		if (event.data.scalar.anchor) {
			int ref;

			dprintf(1, "scalar, anchor: %s\n",
			    event.data.scalar.anchor);
			lua_pushvalue(L, -1);
			ref = luaL_ref(L, LUA_REGISTRYINDEX);
			anchor_set(L, anchors, event.data.sequence_start.anchor,
			    ref);
		}
		if (event.data.scalar.tag)
			dprintf(1, "tag: %s\n", event.data.scalar.tag);
		dprintf(1, "\n");
		break;
	case YAML_SEQUENCE_START_EVENT:
		lua_newtable(L);
		if (event.data.sequence_start.anchor) {
			int ref;

			dprintf(1, "sequence start, anchor: %s\n",
			     event.data.sequence_start.anchor);

			lua_pushvalue(L, -1);
			ref = luaL_ref(L, LUA_REGISTRYINDEX);
			anchor_set(L, anchors, event.data.sequence_start.anchor,
			    ref);
		}
		parse_sequence(L, parser, anchors, env);
		break;
	case YAML_MAPPING_START_EVENT:
		lua_newtable(L);
		if (event.data.mapping_start.anchor) {
			int ref;

			dprintf(1, "mapping start, anchor: %s\n",
				event.data.mapping_start.anchor);

			lua_pushvalue(L, -1);
			ref = luaL_ref(L, LUA_REGISTRYINDEX);
			anchor_set(L, anchors, event.data.mapping_start.anchor,
			    ref);
		}
		parse_mapping(L, parser, anchors, env);
		break;
	default:
		return parse_error(L, parser);
	}
	return 0;
}

static int
parse_mapping(lua_State *L, yaml_parser_t *parser, anchor_t *anchors, int env)
{
	yaml_event_t event;
	int done = 0;
	int value = 0;
	int rv;

	while (!done) {
		if (!yaml_parser_parse(parser, &event))
			return -1;

		dprintf(1, "%s\n", event_name(event.type));

		if (event.type == YAML_MAPPING_END_EVENT)
			done = 1;
		else {
			rv = parse_node(L, parser, anchors, event, value, env);
			if (!rv) {
				if (value)
					lua_settable(L, -3);
				value = 1 - value;
			} else if (rv == 2) {
				value = 0;
			}
		}
		yaml_event_delete(&event);
	}
	return 0;
}

static int
parse_sequence(lua_State *L, yaml_parser_t *parser, anchor_t *anchors, int env)
{
	yaml_event_t event;
	int done = 0;
	int sequence = 1;

	lua_checkstack(L, 2);

	while (!done) {
		if (!yaml_parser_parse(parser, &event))
			return -1;

		dprintf(1, "%s\n", event_name(event.type));

		if (event.type == YAML_SEQUENCE_END_EVENT)
			done = 1;
		else {
			lua_pushinteger(L, sequence++);
			parse_node(L, parser, anchors, event, 0, env);
			lua_settable(L, -3);
		}
		yaml_event_delete(&event);
	}
	return 0;
}

static int
parse_document(lua_State *L, yaml_parser_t *parser,  anchor_t *anchors,
    int env)
{
	yaml_event_t event;
	int done = 0;

	while (!done) {
		if (!yaml_parser_parse(parser, &event))
			return -1;

		dprintf(1, "%s\n", event_name(event.type));

		/* Process event */
		if (event.type == YAML_DOCUMENT_END_EVENT)
			done = 1;
		else
			parse_node(L, parser, anchors, event, 0, env);

		yaml_event_delete(&event);
	}
	return 0;
}

static int
parse_stream(lua_State *L, yaml_parser_t *parser,  anchor_t *anchors, int env)
{
	yaml_event_t event;
	int done = 0;

	while (!done) {
		if (!yaml_parser_parse(parser, &event))
			return -1;

		dprintf(1, "%s\n", event_name(event.type));

		/* Process event */
		switch (event.type) {
		case YAML_DOCUMENT_START_EVENT:
			lua_checkstack(L, 1);
			lua_newtable(L);
			parse_document(L, parser,  anchors, env);
			break;
		case YAML_STREAM_END_EVENT:
			done = 1;
			break;
		default:
			return parse_error(L, parser);
		}
		yaml_event_delete(&event);
	}
	return 0;
}

static int
parse(lua_State *L, yaml_parser_t *parser,  anchor_t *anchors, int env)
{
	yaml_event_t event;
	int done = 0;

	while (!done) {
		if (!yaml_parser_parse(parser, &event))
			return -1;

		dprintf(1, "%s\n", event_name(event.type));

		/* Process event */
		switch (event.type) {
		case YAML_STREAM_START_EVENT:
			parse_stream(L, parser,  anchors, env);
			done = 1;
			break;
		default:
			return parse_error(L, parser);
		}
		yaml_event_delete(&event);
	}
	return 0;
}

static int
parse_string(lua_State *L)
{
	yaml_parser_t parser;
	anchor_t *anchors;
	const unsigned char *input;
	size_t len;
	int env = -1;

	anchors = anchor_init();
	if (!anchors)
		return luaL_error(L, "memory error");

	input = (const unsigned char *)luaL_checklstring(L, 1, &len);
	if (lua_gettop(L) > 1)
		env = 2;

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_string(&parser, input, len);

	if (parse(L, &parser, anchors, env))
		lua_pushnil(L);

	yaml_parser_delete(&parser);
	anchor_free(L, anchors);
	return 1;
}

static int
parse_file(lua_State *L)
{
	yaml_parser_t parser;
	anchor_t *anchors;
	const char *fnam;
	FILE *input;
	int env = -1;

	anchors = anchor_init();
	if (!anchors)
		return luaL_error(L, "memory error");

	fnam = luaL_checkstring(L, 1);
	if (lua_gettop(L) > 1)
		env = 2;

	input = fopen(fnam, "rb");
	if (input == NULL)
		return luaL_error(L, "can't open '%s'", fnam);

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, input);

	if (parse(L, &parser, anchors, env))
		lua_pushnil(L);

	yaml_parser_delete(&parser);
	fclose(input);
	anchor_free(L, anchors);
	return 1;
}

static int
verbosity(lua_State *L)
{
#ifdef DEBUG
	lua_pushinteger(L, verbose);
	verbose = luaL_checkinteger(L, 1);
#else
	lua_pushnil(L);
#endif
	return 1;
}

int
luaopen_yaml(lua_State *L)
{
	struct luaL_Reg luayaml[] = {
		{ "parse",	parse_string },
		{ "parsefile",	parse_file },
		{ "verbosity",	verbosity },
		{ NULL, NULL }
	};

	luaL_newlib(L, luayaml);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2018 - 2025 "
	    "micro systems marc balmer");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "YAML for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "yaml 1.3.0");
	lua_settable(L, -3);
	return 1;
}
