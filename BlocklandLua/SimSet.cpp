#include "misc.hpp"

struct SimSet
{
	char _padding1[52];
	U32 mElementCount;
	U32 mArraySize;
	SimObject **mArray;
};

static int lu_SimSet(lua_State *L)
{
	toNewLuaSimObject(L, 1);
	lua_getfield(L, LUA_REGISTRYINDEX, "SimSet");
	lua_setmetatable(L, 1);
	lua_pushvalue(L, 1);
	return 1;
}

static int lu_SimSet__getCount(lua_State *L)
{
	SimSet *p = (SimSet *)assumeLuaSimObject(L, 1);
	lua_pushinteger(L, p->mElementCount);
	return 1;
}

static int lu_SimSet__getObject(lua_State *L)
{
	SimSet *p = (SimSet *)assumeLuaSimObject(L, 1);
	U32 i = luaL_checkinteger(L, 2);
	if (i < 0 || i >= p->mElementCount)
		return luaL_error(L, "index %d out of range", i);
	newLuaSimObject(L, p->mArray[i]);
	return 1;
}

static int lu_SimSet__iternext(lua_State *L)
{
	SimSet *p = (SimSet *)assumeLuaSimObject(L, 1);
	U32 i = luaL_checkinteger(L, 2);

	if (i < 0 || i >= p->mElementCount)
		return 0;

	lua_pushinteger(L, i + 1);
	newLuaSimObject(L, p->mArray[i]);
	return 2;
}

static int lu_SimSet__iter(lua_State *L)
{
	checkLuaSimObject(L, 1);
	lua_pushcfunction(L, lu_SimSet__iternext);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 0);
	return 3;
}

static const luaL_Reg funcs[] =
{
	{ "getCount", lu_SimSet__getCount },
	{ "getObject", lu_SimSet__getObject },
	{ "iter", lu_SimSet__iter },
	{ NULL, NULL }
};

void LuaInitSimSet(lua_State *L)
{
	luaL_newmetatable(L, "SimSet");
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	luaL_register(L, NULL, funcs);
	lua_pop(L, 1);
	lua_register(L, "SimSet", lu_SimSet);
}