#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

void
proxy_map(lua_State *L, lua_State *R, int t)
{
	int top, istable = 0;
	char nam[64];

	luaL_checkstack(R, 32, "out of stack space");
	switch (lua_type(L, -2)) {
	case LUA_TNUMBER:
		lua_pushnumber(R, lua_tonumber(L, -2));
		snprintf(nam, sizeof nam, "%d", (int)lua_tonumber(L, -2));
		break;
	case LUA_TSTRING:
		snprintf(nam, sizeof nam, "%s", lua_tostring(L, -2));
		lua_pushstring(R, lua_tostring(L, -2));
		break;
	case LUA_TTABLE:
	case LUA_TNIL:
		istable = 1;
		break;
	default:
		luaL_error(L, "proxy: data type '%s' is not "
		    "supported as an index value", luaL_typename(L, -2));
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
			proxy_map(L, R, lua_gettop(R));
			lua_pop(L, 1);
		}
		break;
	default:
		printf("unknown type %s\n", nam);
	}
	if (!istable)
		lua_settable(R, t);
}
