#include "misc.hpp"
#include "vec3.hpp"

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
	Player *p = (Player *)assumeLuaSimObject(L, 1);
	return vec3_new(L, p->x, p->y, p->z);
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