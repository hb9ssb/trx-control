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

#include <grp.h>
#include <lua.h>
#include <lauxlib.h>
#include <pwd.h>
#include <shadow.h>

static int linux_setpwent(lua_State *);
static int linux_endpwent(lua_State *);
static int linux_getpwent(lua_State *);
static int linux_getpwnam(lua_State *);
static int linux_getpwuid(lua_State *);
static int linux_getspnam(lua_State *);
static int linux_getgrnam(lua_State *);
static int linux_getgrgid(lua_State *);

static int
linux_setpwent(lua_State *L)
{
	setpwent();
	return 0;
}

static int
linux_endpwent(lua_State *L)
{
	endpwent();
	return 0;
}

static void
linux_pushpasswd(lua_State *L, struct passwd *pwd)
{
	lua_newtable(L);
	lua_pushstring(L, pwd->pw_name);
	lua_setfield(L, -2, "pw_name");
	lua_pushstring(L, pwd->pw_passwd);
	lua_setfield(L, -2, "pw_passwd");
	lua_pushinteger(L, pwd->pw_uid);
	lua_setfield(L, -2, "pw_uid");
	lua_pushinteger(L, pwd->pw_gid);
	lua_setfield(L, -2, "pw_gid");
	lua_pushstring(L, pwd->pw_gecos);
	lua_setfield(L, -2, "pw_gecos");
	lua_pushstring(L, pwd->pw_dir);
	lua_setfield(L, -2, "pw_dir");
	lua_pushstring(L, pwd->pw_shell);
	lua_setfield(L, -2, "pw_shell");
}

static int
linux_getpwent(lua_State *L)
{
	struct passwd *pwd;

	pwd = getpwent();
	if (pwd != NULL)
		linux_pushpasswd(L, pwd);
	else
		lua_pushnil(L);
	return 1;
}

static int
linux_getpwnam(lua_State *L)
{
	struct passwd *pwd;

	pwd = getpwnam(luaL_checkstring(L, 1));
	if (pwd != NULL)
		linux_pushpasswd(L, pwd);
	else
		lua_pushnil(L);
	return 1;
}

static int
linux_getpwuid(lua_State *L)
{
	struct passwd *pwd;

	pwd = getpwuid(luaL_checkinteger(L, 1));
	if (pwd != NULL)
		linux_pushpasswd(L, pwd);
	else
		lua_pushnil(L);
	return 1;
}

static void
linux_pushspasswd(lua_State *L, struct spwd *spwd)
{
	lua_newtable(L);
	lua_pushstring(L, spwd->sp_namp);
	lua_setfield(L, -2, "sp_namp");
	lua_pushstring(L, spwd->sp_pwdp);
	lua_setfield(L, -2, "sp_pwdp");
	lua_pushinteger(L, spwd->sp_lstchg);
	lua_setfield(L, -2, "sp_lstchg");
	lua_pushinteger(L, spwd->sp_min);
	lua_setfield(L, -2, "sp_min");
	lua_pushinteger(L, spwd->sp_max);
	lua_setfield(L, -2, "sp_max");
	lua_pushinteger(L, spwd->sp_warn);
	lua_setfield(L, -2, "sp_warn");
	lua_pushinteger(L, spwd->sp_inact);
	lua_setfield(L, -2, "sp_inact");
	lua_pushinteger(L, spwd->sp_expire);
	lua_setfield(L, -2, "sp_expire");
}

static int
linux_getspnam(lua_State *L)
{
	struct spwd *spwd;

	spwd = getspnam(luaL_checkstring(L, 1));
	if (spwd != NULL)
		linux_pushspasswd(L, spwd);
	else
		lua_pushnil(L);
	return 1;
}

static void
linux_pushgroup(lua_State *L, struct group *grp)
{
	int n;
	char **mem;

	lua_newtable(L);
	lua_pushstring(L, grp->gr_name);
	lua_setfield(L, -2, "gr_name");
	lua_pushstring(L, grp->gr_passwd);
	lua_setfield(L, -2, "gr_passwd");
	lua_pushinteger(L, grp->gr_gid);
	lua_setfield(L, -2, "gr_gid");
	lua_newtable(L);

	for (n = 1, mem = grp->gr_mem; *mem != NULL; mem++, n++) {
		lua_pushinteger(L, n);
		lua_pushstring(L, *mem);
		lua_settable(L, -3);
	}
	lua_setfield(L, -2, "gr_mem");
}

static int
linux_getgrnam(lua_State *L)
{
	struct group *grp;

	grp = getgrnam(luaL_checkstring(L, 1));
	if (grp != NULL)
		linux_pushgroup(L, grp);
	else
		lua_pushnil(L);
	return 1;
}

static int
linux_getgrgid(lua_State *L)
{
	struct group *grp;

	grp = getgrgid(luaL_checkinteger(L, 1));
	if (grp != NULL)
		linux_pushgroup(L, grp);
	else
		lua_pushnil(L);
	return 1;
}

int
luaopen_linux_pwd(lua_State *L)
{
	struct luaL_Reg pwd[] = {
		{ "setpwent",	linux_setpwent },
		{ "endpwent",	linux_endpwent },
		{ "getpwent",	linux_getpwent },
		{ "getpwnam",	linux_getpwnam },
		{ "getpwuid",	linux_getpwuid },

		/* shadow password */
		{ "getspnam",	linux_getspnam },

		{ "getgrnam",	linux_getgrnam },
		{ "getgrgid",	linux_getgrgid },

		{ NULL, NULL }
	};

	luaL_newlib(L, pwd);
	return 1;
}
