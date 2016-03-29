#pragma once
#include "torque.hpp"
#include "lua.hpp"

inline SimObject **tryLuaSimObjectRef(lua_State *L, int i)
{
	void *p = lua_touserdata(L, i);
	if (p != NULL) {
		if (lua_getmetatable(L, i)) {
			luaL_getmetatable(L, "ts_object_mt");
			if (!lua_rawequal(L, -1, -2))
				p = NULL;
			lua_pop(L, 2);
			return (SimObject **)p;
		}
	}
	return NULL;
}

inline SimObject *assumeLuaSimObject(lua_State *L, int i)
{
	SimObject **ref = (SimObject **)lua_touserdata(L, i);

	if (ref == NULL)
		luaL_error(L, "expected `userdata'");

	if (*ref == NULL)
		luaL_error(L, "object does not exist");

	return *ref;
}

inline SimObject **checkLuaSimObjectRef(lua_State *L, int i)
{
	return (SimObject **)luaL_checkudata(L, i, "ts_object_mt");
}

inline SimObject *checkLuaSimObject(lua_State *L, int i)
{
	SimObject **ref = checkLuaSimObjectRef(L, i);

	if (*ref == NULL)
		luaL_error(L, "object does not exist");

	return *ref;
}

inline SimObject **newLuaSimObject(lua_State *L, SimObject *obj)
{
	SimObject **ref = (SimObject **)lua_newuserdata(L, sizeof (SimObject *));
	*ref = obj;
	luaL_getmetatable(L, "ts_object_mt");
	lua_setmetatable(L, -2);

	if (obj != NULL)
		SimObject__registerReference(obj, ref);

	return ref;
}

inline SimObject **toNewLuaSimObject(lua_State *L, int i)
{
	SimObject *obj = NULL;

	if (lua_isuserdata(L, i))
		obj = checkLuaSimObject(L, i);
	else if (lua_isstring(L, i))
		obj = Sim__findObject_name(StringTableEntry(lua_tostring(L, i)));
	else if (lua_isnumber(L, i))
		obj = Sim__findObject_id(lua_tointeger(L, i));
	else
		luaL_error(L, "expected `string' or `number' or `userdata'");

	SimObject **ref = newLuaSimObject(L, obj);
	lua_replace(L, i);
	return ref;
}