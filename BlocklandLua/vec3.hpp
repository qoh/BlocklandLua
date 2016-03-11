#pragma once
#include "lua.hpp"

typedef struct {
	lua_Number x, y, z;
} vec3_t;

int vec3_new(lua_State *L, lua_Number x, lua_Number y, lua_Number z);