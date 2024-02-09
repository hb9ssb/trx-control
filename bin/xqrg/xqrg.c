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

#include <sys/types.h>
#include <sys/stat.h>

#include <Xm/MainW.h>
#include <Xm/TextF.h>
#include <Xm/List.h>
#include <Xm/MwmUtil.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <lualib.h>
#include <lauxlib.h>

#include "trx-control.h"
#include "xqrg.h"

XtAppContext app;
Widget toplevel;
lua_State *L;

int handler_ref = LUA_REFNIL;
int verbose = 0;
int fd;

static void xqrg_input_cb(XtPointer client_data, int *source, XtInputId *id);

XtInputId qrg_input;

static ConfigRec config;

static XtResource resources[] = {
	{ XtNhost, XtCString, XtRString, sizeof (char *),
	  XtOffsetOf(ConfigRec, host), XtRString, "localhost" },
	{ XtNport, XtCString, XtRString, sizeof (char *),
	  XtOffsetOf(ConfigRec, port), XtRString, "14285" },
	{ XtNverbose, XtCBoolean, XtRBoolean, sizeof(Boolean),
	  XtOffsetOf(ConfigRec, verbose), XtRString, "False" },
};

static XrmOptionDescRec options[] = {
	{ "-h", XtNhost,	XrmoptionSepArg,	"localhost" },
	{ "-p", XtNport,	XrmoptionSepArg,	"14285" },
	{ "-v", XtNverbose,	XrmoptionNoArg,		"True" }
};

extern int luaopen_json(lua_State *);

static void
usage(void)
{
	(void)fprintf(stderr, "usage: xqrg [-v] [-h host] [-p port] trx\n");
	exit(1);
}

void
xqrg_input_cb(XtPointer client_data, int *source, XtInputId *id)
{
	char *buf;

	buf = trxd_readln(fd);

	if (buf) {
		if (verbose)
			printf("< %s\n", buf);
		lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref);
		lua_pushstring(L, buf);
		free(buf);
		if (lua_pcall(L, 1, 0, 0))
			errx(1, "error processing string: %s",
			    lua_tostring(L, -1));
	} else {
		lua_close(L);
		err(1, "trxd terminated");
	}
}

int
main(int argc, char *argv[])
{
	struct stat statbuf;
	char script[PATH_MAX];

	setenv("LANG", "de_DE.UTF-8", 0);
	XtSetLanguageProc(NULL, NULL, NULL);

	toplevel = XtOpenApplication(&app, "XQRG", options, XtNumber(options),
	    &argc, argv, NULL, sessionShellWidgetClass, NULL, 0);
	XtGetApplicationResources(toplevel, &config, resources,
	    XtNumber(resources), NULL, 0);

	if (argc != 2)
		usage();

	verbose = config.verbose;

	L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_xqrg(L);
	lua_setglobal(L, "xqrg");
	luaopen_json(L);
	lua_setglobal(L, "json");

	/* Create the main window and working area */
	create_qrg(toplevel);
	qrg_status("Idle");

	XtRealizeWidget(toplevel);

	fd = trxd_connect(config.host, config.port);

	if (fd < 0)
		err(1, "can't connect to %s:%s", config.host, config.port);

	/* XXX give trxd(8) a bit slack to setup everything */
	sleep(1);

	snprintf(script, sizeof(script), "%s/xqrg.lua", PATH_XQRG);
	if (luaL_loadfile(L, script) != LUA_OK)
		err(1, "xqrg.lua not loaded: %s", lua_tostring(L, -1));
	lua_pushstring(L, argv[1]);
	lua_pushstring(L, config.host);
	lua_pushstring(L, config.port);
	switch (lua_pcall(L, 3, 1, 0)) {
	case LUA_OK:
		break;
	case LUA_ERRRUN:
	case LUA_ERRMEM:
	case LUA_ERRERR:
		err(1, "Lua error: %s", lua_tostring(L, -1));
		break;
	}

	if (lua_type(L, -1) != LUA_TFUNCTION) {
		lua_close(L);
		err(1, "unexpected return value from xqrg.lua");
	}
	handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	XtAppAddInput(app, fd, (XtPointer)XtInputReadMask, xqrg_input_cb,
	    NULL);
	XtAppMainLoop(app);

	lua_close(L);
	return 0;
}
