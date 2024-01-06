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

/* Lua binding for Linux logging */

#include <sys/types.h>

#include <errno.h>
#include <grp.h>
#include <lua.h>
#include <lauxlib.h>
#include <signal.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>

static int syslog_options[] = {
	LOG_CONS,
	LOG_NDELAY,
	LOG_NOWAIT,
	LOG_ODELAY,
	LOG_PERROR,
	LOG_PID
};

static const char *syslog_option_names[] = {
	"cons",
	"ndelay",
	"nowait",
	"odelay",
	"perror",
	"pid",
	NULL
};

static int syslog_facilities[] = {
	LOG_AUTH,
	LOG_AUTHPRIV,
	LOG_CRON,
	LOG_DAEMON,
	LOG_FTP,
	LOG_KERN,
	LOG_LOCAL0,
	LOG_LOCAL1,
	LOG_LOCAL2,
	LOG_LOCAL3,
	LOG_LOCAL4,
	LOG_LOCAL5,
	LOG_LOCAL6,
	LOG_LOCAL7,
	LOG_LPR,
	LOG_MAIL,
	LOG_NEWS,
	LOG_SYSLOG,
	LOG_USER,
	LOG_UUCP
};

static const char *syslog_facility_names[] = {
	"auth",
	"authpriv",
	"cron",
	"daemon",
	"ftp",
	"kern",
	"local0",
	"local1",
	"local2",
	"local3",
	"local4",
	"local5",
	"local6",
	"local7",
	"lpr",
	"mail",
	"news",
	"syslog",
	"user",
	"uucp",
	NULL
};

static int
linux_openlog(lua_State *L)
{
	const char *ident;
	int n, option, facility;

	ident = luaL_checkstring(L, 1);

	for (option = 0, n = 2; n < lua_gettop(L); n++)
		option |= syslog_options[luaL_checkoption(L, n, NULL,
		    syslog_option_names)];
	facility = syslog_facilities[luaL_checkoption(L, n, NULL,
	    syslog_facility_names)];
	openlog(ident, option, facility);
	return 0;
}

static int priorities[] = {
	LOG_EMERG,
	LOG_ALERT,
	LOG_CRIT,
	LOG_ERR,
	LOG_WARNING,
	LOG_NOTICE,
	LOG_INFO,
	LOG_DEBUG
};

static const char *priority_names[] = {
	"emerg",
	"alert",
	"crit",
	"err",
	"warning",
	"notice",
	"info",
	"debug",
	NULL
};

static int
linux_syslog(lua_State *L)
{
	syslog(priorities[luaL_checkoption(L, 1, NULL, priority_names)], "%s",
	    luaL_checkstring(L, 2));
	return 0;
}

static int
linux_closelog(lua_State *L)
{
	closelog();
	return 0;
}

static int
linux_setlogmask(lua_State *L)
{
	lua_pushinteger(L, setlogmask(luaL_checkinteger(L, 1)));
	return 1;
}

int
luaopen_linux_sys_log(lua_State *L)
{
	struct luaL_Reg lualog[] = {
		/* syslog */
		{ "openlog",	linux_openlog },
		{ "syslog",	linux_syslog },
		{ "closelog",	linux_closelog },
		{ "setlogmask",	linux_setlogmask },

		{ NULL, NULL }
	};
	luaL_newlib(L, lualog);
	return 1;
}
