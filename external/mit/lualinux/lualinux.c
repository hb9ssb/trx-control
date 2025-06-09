/*
 * Copyright (c) 2023 - 2025 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
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

/* Lua binding for Linux */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <alloca.h>
#include <errno.h>
#include <grp.h>
#include <lua.h>
#include <lauxlib.h>
#include <signal.h>
#ifdef ALPINE
#include <bsd/stdlib.h>
#else
#include <stdlib.h>
#endif
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "lualinux.h"

extern char *crypt(const char *key, const char *salt);
typedef void (*sighandler_t)(int);

static void
reaper(int signal)
{
	wait(NULL);
}

static int
linux_arc4random(lua_State *L)
{
	lua_pushinteger(L, arc4random());
	return 1;
}

static int
linux_chdir(lua_State *L)
{
	lua_pushinteger(L, chdir(luaL_checkstring(L, 1)));
	return 1;
}

static int
linux_dup2(lua_State *L)
{
	lua_pushinteger(L, dup2(luaL_checkinteger(L, 1),
	    luaL_checkinteger(L, 2)));
	return 1;
}

static int
linux_errno(lua_State *L)
{
	lua_pushinteger(L, errno);
	return 1;
}

static int
linux_strerror(lua_State *L)
{
	lua_pushstring(L, strerror(luaL_checkinteger(L, 1)));
	return 1;
}

static int
linux_fork(lua_State *L)
{
	lua_pushinteger(L, fork());
	return 1;
}

static int
linux_kill(lua_State *L)
{
	lua_pushinteger(L, kill((pid_t)luaL_checkinteger(L, 1),
	    luaL_checkinteger(L, 2)));
	return 1;
}

static int
linux_getcwd(lua_State *L)
{
	char cwd[PATH_MAX];

	if (getcwd(cwd, PATH_MAX) != NULL)
		lua_pushstring(L, cwd);
	else
		lua_pushnil(L);
	return 1;
}

static int
linux_getpass(lua_State *L)
{
	lua_pushstring(L, getpass(luaL_checkstring(L, 1)));
	return 1;
}

static int
linux_getpid(lua_State *L)
{
	lua_pushinteger(L, getpid());
	return 1;
}

static int
linux_setpgid(lua_State *L)
{
	lua_pushinteger(L, setpgid(luaL_checkinteger(L, 1),
	    luaL_checkinteger(L, 2)));
	return 1;
}

static int
linux_sleep(lua_State *L)
{
	lua_pushinteger(L, sleep(luaL_checkinteger(L, 1)));
	return 1;
}

static int
linux_msleep(lua_State *L)
{
	struct timespec rqt;

	long msec = luaL_checkinteger(L, 1);
	if (msec >= 1000) {
		rqt.tv_sec = msec / 1000;
		rqt.tv_nsec = (msec % 1000) * 1000000;
	} else {
		rqt.tv_sec = 0;
		rqt.tv_nsec = msec * 1000000;
	}
	lua_pushinteger(L, nanosleep(&rqt, NULL));
	return 1;
}

static int
linux_unlink(lua_State *L)
{
	lua_pushinteger(L, unlink(luaL_checkstring(L, 1)));
	return 1;
}

static int
linux_getuid(lua_State *L)
{
	lua_pushinteger(L, getuid());
	return 1;
}

static int
linux_getgid(lua_State *L)
{
	lua_pushinteger(L, getgid());
	return 1;
}

static int
linux_setegid(lua_State *L)
{
	lua_pushboolean(L, setegid(luaL_checkinteger(L, 1)) == 0 ? 1 : 0);
	return 1;
}

static int
linux_seteuid(lua_State *L)
{
	lua_pushboolean(L, seteuid(luaL_checkinteger(L, 1)) == 0 ? 1 : 0);
	return 1;
}

static int
linux_setgid(lua_State *L)
{
	lua_pushboolean(L, setgid(luaL_checkinteger(L, 1)) == 0 ? 1 : 0);
	return 1;
}

static int
linux_setuid(lua_State *L)
{
	lua_pushboolean(L, setuid(luaL_checkinteger(L, 1)) == 0 ? 1 : 0);
	return 1;
}

static int
linux_chown(lua_State *L)
{
	lua_pushinteger(L, chown(luaL_checkstring(L, 1),
	    luaL_checkinteger(L, 2), luaL_checkinteger(L, 3)));
	return 1;
}

static int
linux_chmod(lua_State *L)
{
	lua_pushinteger(L, chmod(luaL_checkstring(L, 1),
	    luaL_checkinteger(L, 2)));
	return 1;
}

static int
linux_rename(lua_State *L)
{
	lua_pushinteger(L, rename(luaL_checkstring(L, 1),
	    luaL_checkstring(L, 2)));
	return 1;
}

static int
linux_mkdir(lua_State *L)
{
	lua_pushinteger(L, mkdir(luaL_checkstring(L, 1),
	    luaL_checkinteger(L, 2)));
	return 1;
}

static int
linux_mkstemp(lua_State *L)
{
	int fd;
	char *tmpnam;

	tmpnam = strdup(luaL_checkstring(L, 1));
	if (tmpnam == NULL) {
		lua_pushnil(L);
		lua_pushnil(L);
	} else {
		fd = mkstemp(tmpnam);
		if (fd == -1) {
			lua_pushnil(L);
			lua_pushnil(L);
		} else {
			lua_pushinteger(L, fd);
			lua_pushstring(L, tmpnam);
		}
		free(tmpnam);
	}
	return 2;
}

static int
linux_ftruncate(lua_State *L)
{
	lua_pushboolean(L,
	    ftruncate(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2))
	    ? 0 : 1);
	return 1;
}

static int
linux_getenv(lua_State *L)
{
	char *v;

	v = getenv(luaL_checkstring(L, 1));
	if (v)
		lua_pushstring(L, v);
	else
		lua_pushnil(L);
	return 1;
}

static int
linux_setenv(lua_State *L)
{
	lua_pushboolean(L, setenv(luaL_checkstring(L, 1),
	    luaL_checkstring(L, 2), lua_toboolean(L, 3)) ? 0 : 1);
	return 1;
}

static int
linux_unsetenv(lua_State *L)
{
	lua_pushboolean(L, unsetenv(luaL_checkstring(L, 1)) ? 0 : 1);
	return 1;
}

static int
linux_crypt(lua_State *L)
{
	lua_pushstring(L, crypt(luaL_checkstring(L, 1),
	    luaL_checkstring(L, 2)));
	return 1;
}

static int
linux_signal(lua_State *L)
{
	sighandler_t old, new;

	new = (sighandler_t)lua_tocfunction(L, 2);
	old = signal(luaL_checkinteger(L, 1), new);

	lua_pushcfunction(L, (lua_CFunction)old);
	return 1;
}

static int
linux_gethostname(lua_State *L)
{
	char name[128];

	if (!gethostname(name, sizeof name))
		lua_pushstring(L, name);
	else
		lua_pushnil(L);
	return 1;
}

static int
linux_sethostname(lua_State *L)
{
	const char *name;
	size_t len;

	name = luaL_checklstring(L, 1, &len);
	if (sethostname(name, len))
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
	return 1;
}

static void
linux_set_info(lua_State *L)
{
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2023 by "
	    "micro systems marc balmer");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "Linux binding for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "linux 1.1.0");
	lua_settable(L, -3);
}

static struct constant linux_constant[] = {
	/* file modes */
	CONSTANT(S_IRUSR),
	CONSTANT(S_IWUSR),
	CONSTANT(S_IXUSR),
	CONSTANT(S_IRGRP),
	CONSTANT(S_IWGRP),
	CONSTANT(S_IXGRP),
	CONSTANT(S_IROTH),
	CONSTANT(S_IWOTH),
	CONSTANT(S_IXOTH),

	/* signals */
	CONSTANT(SIGHUP),
	CONSTANT(SIGINT),
	CONSTANT(SIGQUIT),
	CONSTANT(SIGILL),
	CONSTANT(SIGTRAP),
	CONSTANT(SIGABRT),
	CONSTANT(SIGIOT),
	CONSTANT(SIGBUS),
	CONSTANT(SIGFPE),
	CONSTANT(SIGKILL),
	CONSTANT(SIGUSR1),
	CONSTANT(SIGSEGV),
	CONSTANT(SIGUSR2),
	CONSTANT(SIGPIPE),
	CONSTANT(SIGALRM),
	CONSTANT(SIGTERM),
	CONSTANT(SIGSTKFLT),
	CONSTANT(SIGCHLD),
	CONSTANT(SIGCONT),
	CONSTANT(SIGSTOP),
	CONSTANT(SIGTSTP),
	CONSTANT(SIGTTIN),
	CONSTANT(SIGTTOU),
	CONSTANT(SIGURG),
	CONSTANT(SIGXCPU),
	CONSTANT(SIGXFSZ),
	CONSTANT(SIGVTALRM),
	CONSTANT(SIGPROF),
	CONSTANT(SIGWINCH),
	CONSTANT(SIGPOLL),
	CONSTANT(SIGIO),
	CONSTANT(SIGPWR),
	CONSTANT(SIGSYS),
	{ NULL, 0 }
};

int
luaopen_linux(lua_State *L)
{
	int n;
	struct luaL_Reg lualinux[] = {
		{ "arc4random",	linux_arc4random },
		{ "chdir",	linux_chdir },
		{ "dup2",	linux_dup2 },
		{ "errno",	linux_errno },
		{ "strerror",	linux_strerror },
		{ "fork",	linux_fork },
		{ "kill",	linux_kill },
		{ "getcwd",	linux_getcwd },
		{ "getpass",	linux_getpass },
		{ "getpid",	linux_getpid },
		{ "setpgid",	linux_setpgid },
		{ "sleep",	linux_sleep },
		{ "msleep",	linux_msleep },
		{ "unlink",	linux_unlink },
		{ "getuid",	linux_getuid },
		{ "getgid",	linux_getgid },
		{ "setegid",	linux_setegid },
		{ "seteuid",	linux_seteuid },
		{ "setgid",	linux_setgid },
		{ "setuid",	linux_setuid },
		{ "chown",	linux_chown },
		{ "chmod",	linux_chmod },
		{ "rename",	linux_rename },
		{ "mkdir" ,	linux_mkdir },
		{ "mkstemp",	linux_mkstemp },
		{ "ftruncate",	linux_ftruncate },

		/* environment */
		{ "getenv",	linux_getenv },
		{ "setenv",	linux_setenv },
		{ "unsetenv",	linux_unsetenv },

		/* crypt */
		{ "crypt",	linux_crypt },

		/* signals */
		{ "signal",	linux_signal },

		/* hostname */
		{ "gethostname",	linux_gethostname },
		{ "sethostname",	linux_sethostname },

		{ NULL, NULL }
	};

	luaL_newlib(L, lualinux);
	linux_set_info(L);
	for (n = 0; linux_constant[n].name != NULL; n++) {
		lua_pushinteger(L, linux_constant[n].value);
		lua_setfield(L, -2, linux_constant[n].name);
	};
	lua_pushcfunction(L, (lua_CFunction)SIG_IGN);
	lua_setfield(L, -2, "SIG_IGN");
	lua_pushcfunction(L, (lua_CFunction)SIG_DFL);
	lua_setfield(L, -2, "SIG_DFL");
	lua_pushcfunction(L, (lua_CFunction)reaper);
	lua_setfield(L, -2, "SIG_REAPER");
	return 1;
}
