#include <cmath>
#include "vec3.hpp"

int vec3_new(lua_State *L, lua_Number x, lua_Number y, lua_Number z)
{
	vec3_t *v = (vec3_t *)lua_newuserdata(L, sizeof vec3_t);
	v->x = x;
	v->y = y;
	v->z = z;
	lua_getfield(L, LUA_REGISTRYINDEX, "vec3");
	lua_setmetatable(L, -2);
	return 1;
}

static int vec3_new(lua_State *L)
{
	lua_Number x, y, z;
	if (lua_gettop(L) == 0) {
		x = y = z = 0;
	} else {
		x = luaL_checknumber(L, 1);
		y = luaL_checknumber(L, 2);
		z = luaL_checknumber(L, 3);
	}
	return vec3_new(L, x, y, z);
}

static int vec3_mt_tostring(lua_State *L)
{
	vec3_t *v = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	lua_pushnumber(L, v->x);
	lua_tostring(L, -1);
	lua_pushstring(L, " ");
	lua_pushnumber(L, v->y);
	lua_tostring(L, -1);
	lua_pushstring(L, " ");
	lua_pushnumber(L, v->z);
	lua_tostring(L, -1);
	lua_concat(L, 5);
	return 1;
}

static int vec3_mt_len(lua_State *L)
{
	vec3_t *v = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	lua_pushnumber(L, sqrt(v->x*v->x + v->y*v->y + v->z*v->z));
	return 1;
}

static int vec3_mt_eq(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	lua_pushboolean(L, a->x == b->x && a->y == b->y && a->z == b->z);
	return 1;
}

/* static int vec3_mt_lt(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	lua_Number la = a->x*a->x + a->y*a->y + a->z*a->z;
	lua_Number lb = b->x*b->x + b->y*b->y + b->z*b->z;
	lua_pushboolean(L, la < lb);
	return 1;
}

static int vec3_mt_le(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	lua_Number la = a->x*a->x + a->y*a->y + a->z*a->z;
	lua_Number lb = b->x*b->x + b->y*b->y + b->z*b->z;
	lua_pushboolean(L, la <= lb);
	return 1;
} */

static int vec3_mt_unm(lua_State *L)
{
	vec3_t *v = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	return vec3_new(L, -v->x, -v->y, -v->z);
}

static int vec3_mt_add(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	return vec3_new(L, a->x + b->x, a->y + b->y, a->z + b->z);
}

static int vec3_mt_sub(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	return vec3_new(L, a->x - b->x, a->y - b->y, a->z - b->z);
}

static int vec3_mt_mul(lua_State *L)
{
	if (lua_isnumber(L, 1)) {
		lua_Number a = lua_tonumber(L, 1);
		vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
		return vec3_new(L, a * b->x, a * b->y, a * b->z);
	} else if (lua_isnumber(L, 2)) {
		vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
		lua_Number b = lua_tonumber(L, 2);
		return vec3_new(L, a->x * b, a->y * b, a->z * b);
	} else {
		return luaL_error(L, "expected arg #1 or #2 to be `number'");
	}
}

static int vec3_mt_div(lua_State *L)
{
	if (lua_isnumber(L, 1)) {
		lua_Number a = lua_tonumber(L, 1);
		vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
		return vec3_new(L, a / b->x, a / b->y, a / b->z);
	} else if (lua_isnumber(L, 2)) {
		vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
		lua_Number b = lua_tonumber(L, 2);
		return vec3_new(L, a->x / b, a->y / b, a->z / b);
	} else {
		return luaL_error(L, "expected arg #1 or #2 to be `number'");
	}
}

static int vec3_index_x(lua_State *L)
{
	vec3_t *v = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	if (lua_gettop(L) >= 2)
		v->x = luaL_checknumber(L, 2);
	lua_pushnumber(L, v->x);
	return 1;
}

static int vec3_index_y(lua_State *L)
{
	vec3_t *v = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	if (lua_gettop(L) >= 2)
		v->y = luaL_checknumber(L, 2);
	lua_pushnumber(L, v->y);
	return 1;
}

static int vec3_index_z(lua_State *L)
{
	vec3_t *v = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	if (lua_gettop(L) >= 2)
		v->z = luaL_checknumber(L, 2);
	lua_pushnumber(L, v->z);
	return 1;
}

static int vec3_index_unit(lua_State *L)
{
	vec3_t *v = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	lua_Number l = sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
	return vec3_new(L, v->x / l, v->y / l, v->z / l);
}

static int vec3_index_dist(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	lua_Number x = b->x - a->x;
	lua_Number y = b->y - a->y;
	lua_Number z = b->z - a->z;
	lua_pushnumber(L, sqrt(x*x + y*y + z*z));
	return 1;
}

static int vec3_index_cross(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	return vec3_new(L,
		a->y * b->z - a->z * b->y,
		a->z * b->x - a->x * b->z,
		a->x * b->y - a->y * b->x);
}

static int vec3_index_dot(lua_State *L)
{
	vec3_t *a = (vec3_t *)luaL_checkudata(L, 1, "vec3");
	vec3_t *b = (vec3_t *)luaL_checkudata(L, 2, "vec3");
	lua_pushnumber(L, a->x * b->x + a->y * b->y + a->z * b->z);
	return 1;
}

static luaL_Reg metatable[] = {
	{"__tostring", vec3_mt_tostring},
	{"__len", vec3_mt_len},
	{"__eq", vec3_mt_eq},
	/* {"__lt", vec3_mt_lt},
	{"__le", vec3_mt_le}, */
	{"__unm", vec3_mt_unm},
	{"__add", vec3_mt_add},
	{"__sub", vec3_mt_sub},
	{"__mul", vec3_mt_mul},
	{"__div", vec3_mt_div},
	{NULL, NULL}
};

static luaL_Reg index[] = {
	{"x", vec3_index_x},
	{"y", vec3_index_y},
	{"z", vec3_index_z},
	{"unit", vec3_index_unit},
	{"dist", vec3_index_dist},
	{"cross", vec3_index_cross},
	{"dot", vec3_index_dot},
	{NULL, NULL}
};

void LuaInitvec3(lua_State *L)
{
	luaL_newmetatable(L, "vec3");
	luaL_register(L, NULL, metatable);
	lua_newtable(L);
	luaL_register(L, NULL, index);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	lua_pushcfunction(L, vec3_new);
	lua_setglobal(L, "vec3");
}