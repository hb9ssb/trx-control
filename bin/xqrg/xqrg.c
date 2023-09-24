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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <Xm/MainW.h>
#include <Xm/TextF.h>
#include <Xm/List.h>
#include <Xm/MwmUtil.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
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

int emulator_ref = LUA_REFNIL;
int verbosity = 0;
int fd;

static void xqrg_input_cb(XtPointer client_data, int *source, XtInputId *id);

int client_fd = -1;
XtInputId qrg_input;

static ConfigRec config;

static XtResource resources[] = {
	{ XtNhost, XtCString, XtRString, sizeof (char *),
	  XtOffsetOf(ConfigRec, host), XtRString, "localhost" },
	{ XtNport, XtCString, XtRString, sizeof (char *),
	  XtOffsetOf(ConfigRec, port), XtRString, "14285" },
	{ XtNscript, XtCString, XtRString, sizeof (char *),
	  XtOffsetOf(ConfigRec, script), XtRString, "xqrg.lua" }

};

static XrmOptionDescRec options[] = {
	{ "-h", XtNhost,	XrmoptionSepArg,	"localhost" },
	{ "-e", XtNscript,	XrmoptionSepArg,	"" },
	{ "-p", XtNport,	XrmoptionSepArg,	"14285" },
};

extern int luaopen_json(lua_State *);

static void
usage(void)
{
	(void)fprintf(stderr, "usage: xqrg [-h host] [-e script] "
	    "[-p port] transceiver\n");
	exit(1);
}

void
xqrg_input_cb(XtPointer client_data, int *source, XtInputId *id)
{
	unsigned char c;

	if (read(*source, &c, 1) == 1) {
		if (emulator_ref != LUA_REFNIL) {
			lua_rawgeti(L, LUA_REGISTRYINDEX, emulator_ref);
			lua_pushinteger(L, c);
			if (lua_pcall(L, 1, 0, 0))
				errx(1, "error processing character: %s",
				    lua_tostring(L, -1));
		} else
			errx(1, "no emulation selected");
	} else {
		XtRemoveInput(qrg_input);
		close(*source);
		client_fd = -1;
	}
}

int
main(int argc, char *argv[])
{
	struct stat statbuf;
	char script[PATH_MAX];
	struct addrinfo hints, *res, *res0;
	int listen_fd, errv, val;

	setenv("LANG", "de_DE.UTF-8", 0);
	XtSetLanguageProc(NULL, NULL, NULL);

	toplevel = XtOpenApplication(&app, "XQRG", options, XtNumber(options),
	    &argc, argv, NULL, sessionShellWidgetClass, NULL, 0);
	XtGetApplicationResources(toplevel, &config, resources,
	    XtNumber(resources), NULL, 0);

	if (argc != 2)
		usage();


	L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_xqrg(L);
	lua_setglobal(L, "xqrg");
	luaopen_json(L);
	lua_setglobal(L, "json");

	/* Create the main window and working area */
	create_qrg(toplevel);
	update_status();

	XtRealizeWidget(toplevel);

	if (!stat(config.script, &statbuf)) {
		if (luaL_dofile(L, config.script))
			errx(1, "%s\n", lua_tostring(L, -1));
	} else {
		snprintf(script, sizeof(script), "%s/%s%s",
		    PATH_XQRG, config.script,
		    strstr(config.script, ".lua") == NULL ? ".lua" : "");

		if (!stat(script, &statbuf)) {
			if (luaL_dofile(L, script))
				err(1, "%s\n", lua_tostring(L, -1));
		} else
			errx(1, "script %s not found", config.script);
	}

	fd = connect_trxd(config.host, config.port);

	if (fd < 0)
		err(1, "can not connect to %s:%s", config.host, config.port);

	XtAppAddInput(app, fd, (XtPointer)XtInputReadMask, xqrg_input_cb,
	    NULL);
	XtAppMainLoop(app);

	lua_close(L);
	return 0;
}
