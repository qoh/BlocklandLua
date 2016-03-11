#pragma once
#include "torque.hpp"
#include "lua.hpp"

typedef struct {
	SimObject *obj;
	SimObjectId id;
} LuaSimObject;

inline LuaSimObject *newLuaSimObject(lua_State *L, SimObject *obj)
{
	LuaSimObject *ls = (LuaSimObject *)lua_newuserdata(L, sizeof LuaSimObject);
	luaL_getmetatable(L, "ts_object_mt");
	lua_setmetatable(L, -2);
	ls->obj = obj;
	ls->id = obj->id;
	return ls;
}

inline LuaSimObject *toNewLuaSimObject(lua_State *L, int i)
{
	SimObject *obj = NULL;

	if (lua_isuserdata(L, i))
		obj = ((LuaSimObject *)lua_touserdata(L, i))->obj;
	else if (lua_isstring(L, i))
		obj = Sim__findObject_name(StringTableEntry(lua_tostring(L, i)));
	else if (lua_isnumber(L, i))
		obj = Sim__findObject_id(lua_tointeger(L, i));
	else
		luaL_error(L, "expected `string' or `number' or `userdata'");

	LuaSimObject *ls = (LuaSimObject *)lua_newuserdata(L, sizeof LuaSimObject);
	ls->obj = obj;
	ls->id = obj->id;
	lua_replace(L, i);
	return ls;
}

inline LuaSimObject *checkLuaSimObject(lua_State *L, int i)
{
	LuaSimObject *ls = (LuaSimObject *)lua_touserdata(L, i);

	if (ls == NULL)
		luaL_error(L, "expected `userdata'");

	return ls;
}