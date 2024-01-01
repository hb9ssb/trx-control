/*
 * Copyright (c) 2023 - 2024 Marc Balmer HB9SSB
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

#include <Xm/Xm.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <lualib.h>
#include <lauxlib.h>

#include "trx-control.h"
#include "xqrg.h"


extern int fd;
extern int verbose;

static int
luaxqrg_addchar(lua_State *L)
{
        qrg_addchar(luaL_checkinteger(L, 1));
        return 0;
}

static int
luaxqrg_addstring(lua_State *L)
{
        qrg_addstring((char *)luaL_checkstring(L, 1));
        return 0;
}
static int
luaxqrg_scrollup(lua_State *L)
{
        qrg_scrollup();
        return 0;
}

static int
luaxqrg_position(lua_State *L)
{
        qrg_position(luaL_checkinteger(L, 1));
        return 0;
}

static int
luaxqrg_reset(lua_State *L)
{
        qrg_reset();
        return 0;
}

static int
luaxqrg_clrscr(lua_State *L)
{
	qrg_clrscr();
	return 0;
}

static int
luaxqrg_clreol(lua_State *L)
{
	qrg_clreol();
	return 0;
}

static int
luaxqrg_readln(lua_State *L)
{
	char *buf;

	buf = trxd_readln(fd);
	if (buf != NULL) {
		if (verbose)
			printf("< %s\n", buf);
		lua_pushstring(L, buf);
		free(buf);
	} else
		lua_pushnil(L);

	return 1;
}

static int
luaxqrg_writeln(lua_State *L)
{
	char *data;
	size_t len;

	data = (char *)luaL_checklstring(L, 1, &len);
	if (verbose)
		printf("> %s\n", data);

	if (trxd_writeln(fd, data) == len + 2)
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);

	return 1;
}

static int
luaxqrg_scroll_start(lua_State *L)
{
	scroll_start(luaL_checkstring(L, 1));
	return 0;
}

static int
luaxqrg_status(lua_State *L)
{
	qrg_status((char *)luaL_checkstring(L, 1));
	return 0;
}

int
luaopen_xqrg(lua_State *L)
{
	struct luaL_Reg luaxqrg[] = {
		{ "addchar",		luaxqrg_addchar },
		{ "addstring",		luaxqrg_addstring },
		{ "scrollup",		luaxqrg_scrollup },
		{ "position",		luaxqrg_position },
                { "reset",		luaxqrg_reset },
                { "clrscr",		luaxqrg_clrscr },
		{ "clreol",		luaxqrg_clreol },
                { "reset",		luaxqrg_reset },
		{ "readln",		luaxqrg_readln },
		{ "writeln",		luaxqrg_writeln },
		{ "scroll_start",	luaxqrg_scroll_start },
		{ "status",		luaxqrg_status },
		{ NULL, NULL }
	};

	luaL_newlib(L, luaxqrg);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2023 - 2024 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trx-control frequency display");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "xqrg " VERSION);
	lua_settable(L, -3);

        return 1;
}
