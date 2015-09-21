#include "misc.h"

struct Player
{
	char _padding1[104];
	float x;
	char _padding2[12];
	float y;
	char _padding3[12];
	float z;
};

static int lu_Player(lua_State *L)
{
	toNewLuaSimObject(L, 1);
	lua_getfield(L, LUA_REGISTRYINDEX, "Player");
	lua_setmetatable(L, 1);
	lua_pushvalue(L, 1);
	return 1;
}

static int lu_Player__getPosition(lua_State *L)
{
	Player *p = (Player *)checkLuaSimObject(L, 1)->obj;
	lua_createtable(L, 3, 0);
	lua_pushnumber(L, p->x); lua_rawseti(L, -2, 1);
	lua_pushnumber(L, p->y); lua_rawseti(L, -2, 2);
	lua_pushnumber(L, p->z); lua_rawseti(L, -2, 3);
	return 1;
}

static const luaL_Reg funcs[] =
{
	{"getPosition", lu_Player__getPosition},
	{NULL, NULL}
};

void LuaInitPlayer(lua_State *L)
{
	luaL_newmetatable(L, "Player");
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	luaL_register(L, NULL, funcs);
	lua_pop(L, 1);
	lua_register(L, "Player", lu_Player);
}