#include <cstdlib>
#include "detours/detours.h"
#include "misc.hpp"

lua_State *gL;

bool istrue(const char *arg)
{
	return !_stricmp(arg, "true") || !_stricmp(arg, "1") || (0 != atoi(arg));
}

void showluaerror(lua_State *L, bool silent=false)
{
	if (silent)
		lua_pop(L, -1);
	else
		Printf("\x03%s", lua_tostring(L, -1));
}

void runlua(lua_State *L, const char *code, const char *name="input")
{
	if (luaL_loadbuffer(L, code, strlen(code), name) || lua_pcall(L, 0, 0, 0))
		showluaerror(L);
}

// TorqueScript ConsoleFunction callbacks
static const char *ts_luaEval(SimObject *obj, int argc, const char** argv)
{
	if (luaL_loadbuffer(gL, argv[1], strlen(argv[1]), "input") || lua_pcall(gL, 0, 1, 0))
	{
		showluaerror(gL, argc >= 3 && istrue(argv[2]));
		return "";
	}

	const char *s = lua_tostring(gL, -1);
	lua_pop(gL, 1);
	if(s == nullptr || s == NULL) {
		return "";
	}
	return s;
}

// note: does not use Blockland file system atm, allows execution outside game folder, does not support zip
static const char *ts_luaExec(SimObject *obj, int argc, const char** argv)
{
	if (argc < 3 || !istrue(argv[2]))
		Printf("Executing %s.", argv[1]);

	if (luaL_loadfile(gL, argv[1]) || lua_pcall(gL, 0, 1, 0))
	{
		showluaerror(gL, argc >= 3 && istrue(argv[2]));
		return "";
	}

	const char *s = lua_tostring(gL, -1);
	lua_pop(gL, 1);
	if(s == nullptr || s == NULL) {
		return "";
	}
	return s;
}

static const char *ts_luaCall(SimObject *obj, int argc, const char *argv[])
{
	lua_getglobal(gL, argv[1]);

	for (int i = 2; i < argc; i++)
		lua_pushstring(gL, argv[i]);

	if (lua_pcall(gL, argc - 2, 1, 0))
	{
		showluaerror(gL);
		return "";
	}

	const char *s = lua_tostring(gL, -1);
	lua_pop(gL, 1);
	if(s == nullptr || s == NULL) {
		return "";
	}
	return s;
}

static const char *ts_luaCallAuto(SimObject *obj, int argc, const char *argv[])
{
	lua_getglobal(gL, argv[0]);

	if (lua_type(gL, -1) == LUA_TNIL)
	{
		Printf("\x03Missing Lua global for \"%s\"", argv[0]);
		lua_pop(gL, -1);
		return "";
	}

	for (int i = 1; i < argc; i++)
		lua_pushstring(gL, argv[i]);

	if (lua_pcall(gL, argc - 1, 1, 0))
	{
		showluaerror(gL);
		return "";
	}

	const char *s = lua_tostring(gL, -1);
	lua_pop(gL, 1);
	if(s == nullptr || s == NULL) {
		return "";
	}
	return s;
}

// Lua CFunction callbacks
static int lu_print(lua_State *L)
{
	int nargs = lua_gettop(L);

	lua_getglobal(L, "tostring");
		
	char outstring[1024] = "";

	for (int i = 1; i <= nargs; i++)
	{
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);

		const char *s = lua_tostring(L, -1);
		if (s == NULL)
			return luaL_error(L, "'tostring' must return a string to 'print'");
		
		if (i > 1)
			strcat_s(outstring, " ");

		strcat_s(outstring, s);
		lua_pop(L, 1);
	}

	Printf("%s", outstring);
	return 0;
}

static int lu_ts_eval(lua_State *L)
{
	lua_pushstring(L, Eval(luaL_checkstring(L, 1)));
	return 1;
}

static int lu_ts_func_call(lua_State *L)
{
	int n = lua_gettop(L) - 1;
	if (n > 19)
		return luaL_error(L, "too many arguments for TorqueScript");

	Namespace::Entry *nsEntry = (Namespace::Entry *)lua_touserdata(L, lua_upvalueindex(1));
	
	int argc = 0;
	const char *argv[21];

	argv[argc++] = nsEntry->mFunctionName;
	SimObject *obj;

	if (!lua_isnil(L, 1))
	{
		SimObject **ref = checkLuaSimObjectRef(L, 1);
		obj = *ref;
	}
	else
		obj = NULL;

	if (obj != NULL)
	{
		char idbuf[sizeof(int) * 3 + 2];
		snprintf(idbuf, sizeof idbuf, "%d", obj->id);
		argv[argc++] = StringTableEntry(idbuf);
	}

	for (int i = 0; i < n; i++)
	{
		int t = lua_type(L, 2 + i);
		
		switch (t)
		{
		case LUA_TSTRING:
		case LUA_TNUMBER:
			argv[argc++] = lua_tostring(L, 2 + i);
			break;
		case LUA_TBOOLEAN:
			argv[argc++] = lua_toboolean(L, 2 + i) ? "1" : "0";
			break;
		case LUA_TNIL:
		case LUA_TNONE:
			argv[argc++] = "";
			break;
		case LUA_TUSERDATA:
		{
			SimObjectId id;
			SimObject **ref = checkLuaSimObjectRef(L, 2 + i);

			if (*ref != NULL)
				id = (*ref)->id;
			else
				id = 0;

			char idbuf[sizeof(int) * 3 + 2];
			snprintf(idbuf, sizeof idbuf, "%d", id);
			argv[argc++] = StringTableEntry(idbuf);

			break;
		}
		default:
			luaL_error(L, "cannot pass `%s' to TorqueScript", lua_typename(L, t));
			break;
		}
	}

	if (nsEntry->mType == Namespace::Entry::ScriptFunctionType)
	{
		if (nsEntry->mFunctionOffset)
		{
			lua_pushstring(L, CodeBlock__exec(
				nsEntry->mCode, nsEntry->mFunctionOffset,
				nsEntry->mNamespace, nsEntry->mFunctionName, argc, argv,
				false, nsEntry->mPackage, 0));
		}
		else
			lua_pushstring(L, "");

		return 1;
	}

	S32 mMinArgs = nsEntry->mMinArgs;
	S32 mMaxArgs = nsEntry->mMaxArgs;

	if ((mMinArgs && argc < mMinArgs) || (mMaxArgs && argc > mMaxArgs))
	{
		luaL_error(L, "wrong number of arguments (expects %d-%d)", nsEntry->mMinArgs, nsEntry->mMaxArgs);
		return 0;
	}

	void *cb = nsEntry->cb;
	switch (nsEntry->mType)
	{
	case Namespace::Entry::StringCallbackType:
		lua_pushstring(L, ((StringCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::IntCallbackType:
		lua_pushnumber(L, ((IntCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::FloatCallbackType:
		lua_pushnumber(L, ((FloatCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::BoolCallbackType:
		lua_pushboolean(L, ((BoolCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::VoidCallbackType:
		((VoidCallback)cb)(obj, argc, argv);
		return 0;
	}

	return luaL_error(L, "invalid function call");
}

/* FIXME FIXME: horrendous code duplication */
static int lu_ts_func_call_plain(lua_State *L)
{
	int n = lua_gettop(L);
	if (n > 19)
		return luaL_error(L, "too many arguments for TorqueScript");

	Namespace::Entry *nsEntry = (Namespace::Entry *)lua_touserdata(L, lua_upvalueindex(1));

	int argc = 0;
	const char *argv[21];

	argv[argc++] = nsEntry->mFunctionName;
	SimObject *obj = NULL;

	for (int i = 0; i < n; i++)
	{
		int t = lua_type(L, 1 + i);

		switch (t)
		{
		case LUA_TSTRING:
		case LUA_TNUMBER:
			argv[argc++] = lua_tostring(L, 1 + i);
			break;
		case LUA_TBOOLEAN:
			argv[argc++] = lua_toboolean(L, 1 + i) ? "1" : "0";
			break;
		case LUA_TNIL:
		case LUA_TNONE:
			argv[argc++] = "";
			break;
		case LUA_TUSERDATA:
		{
			SimObjectId id;
			SimObject **ref = checkLuaSimObjectRef(L, 1 + i);

			if (*ref != NULL)
				id = (*ref)->id;
			else
				id = 0;

			char idbuf[sizeof(int) * 3 + 2];
			snprintf(idbuf, sizeof idbuf, "%d", id);
			argv[argc++] = StringTableEntry(idbuf);

			break;
		}
		default:
			luaL_error(L, "cannot pass `%s' to TorqueScript", lua_typename(L, t));
			break;
		}
	}

	if (nsEntry->mType == Namespace::Entry::ScriptFunctionType)
	{
		if (nsEntry->mFunctionOffset)
		{
			lua_pushstring(L, CodeBlock__exec(
				nsEntry->mCode, nsEntry->mFunctionOffset,
				nsEntry->mNamespace, nsEntry->mFunctionName, argc, argv,
				false, nsEntry->mPackage, 0));
		}
		else
			lua_pushstring(L, "");

		return 1;
	}

	S32 mMinArgs = nsEntry->mMinArgs;
	S32 mMaxArgs = nsEntry->mMaxArgs;

	if ((mMinArgs && argc < mMinArgs) || (mMaxArgs && argc > mMaxArgs))
	{
		luaL_error(L, "wrong number of arguments (expects %d-%d)", nsEntry->mMinArgs, nsEntry->mMaxArgs);
		return 0;
	}

	void *cb = nsEntry->cb;
	switch (nsEntry->mType)
	{
	case Namespace::Entry::StringCallbackType:
		lua_pushstring(L, ((StringCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::IntCallbackType:
		lua_pushnumber(L, ((IntCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::FloatCallbackType:
		lua_pushnumber(L, ((FloatCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::BoolCallbackType:
		lua_pushboolean(L, ((BoolCallback)cb)(obj, argc, argv));
		return 1;
	case Namespace::Entry::VoidCallbackType:
		((VoidCallback)cb)(obj, argc, argv);
		return 0;
	}

	return luaL_error(L, "invalid function call");
}

static int lu_ts_func(lua_State *L)
{
	int n = lua_gettop(L);
	Namespace *ns;

	SimObject **ref;

	if (n == 1)
		ns = LookupNamespace(NULL);
	else if (lua_isstring(L, 1))
		ns = LookupNamespace(luaL_checkstring(L, 1));
	else if (ref = tryLuaSimObjectRef(L, 1))
		ns = (*ref)->mNameSpace;
	else
		return luaL_argerror(L, 1, "expected `string', `ts.obj'");

	const char *fnName = luaL_checkstring(L, n);
	Namespace::Entry *nsEntry = Namespace__lookup(ns, StringTableEntry(fnName));

	if (nsEntry == NULL)
	{
		luaL_error(L, "unknown function %s", fnName);
		return 0;
	}

	lua_pushlightuserdata(L, nsEntry);

	if (n == 1)
		lua_pushcclosure(L, lu_ts_func_call_plain, 1);
	else
		lua_pushcclosure(L, lu_ts_func_call, 1);

	return 1;
}

static int lu_ts_expose(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	const char *desc = name;

	if (lua_gettop(L) >= 2)
		desc = luaL_checkstring(L, 2);

	ConsoleFunction(NULL, name, ts_luaCallAuto, desc, 1, 20);
	return 0;
}

static int lu_ts_obj(lua_State *L)
{
	int t = lua_type(L, 1);
	SimObject *obj;

	switch (t) {
	case LUA_TNUMBER:
		obj = Sim__findObject_id(luaL_checkint(L, 1));
		break;
	case LUA_TSTRING:
		obj = Sim__findObject_name(luaL_checkstring(L, 1));
		break;
	default:
		return luaL_error(L, "`number' or `string' expected");
	}

	newLuaSimObject(L, obj);
	return 1;
}

static int lu_ts_obj_field_index(lua_State *L)
{
	SimObject *obj = checkLuaSimObject(L, lua_upvalueindex(1));
	const char *k = luaL_checkstring(L, 2);
	lua_pushstring(L, SimObject__getDataField(obj, k, StringTableEntry("")));
	return 1;
}

static int lu_ts_obj_field_newindex(lua_State *L)
{
	SimObject *obj = checkLuaSimObject(L, lua_upvalueindex(1));
	const char *k = luaL_checkstring(L, 2);
	const char *v = lua_tostring(L, 3);
	SimObject__setDataField(obj, k, StringTableEntry(""), v);
	return 0;
}

static int lu_ts_obj_gc(lua_State *L)
{
	SimObject **ref = checkLuaSimObjectRef(L, 1);

	if (*ref != NULL)
		SimObject__unregisterReference(*ref, ref);

	return 0;
}

static int lu_ts_obj_index(lua_State *L)
{
	SimObject *obj = *checkLuaSimObjectRef(L, 1);
	const char *k = luaL_checkstring(L, 2);

	if (strcmp(k, "_exists") == 0)
	{
		lua_pushboolean(L, (int)(obj != NULL));
		return 1;
	}

	if (obj == NULL)
		return luaL_error(L, "object does not exist");

	if (strcmp(k, "_id") == 0)
	{
		lua_pushinteger(L, obj->id);
		return 1;
	}
	else if (strcmp(k, "_name") == 0)
	{
		lua_pushstring(L, obj->objectName);
		return 1;
	}
	else
	{
		lua_pushstring(L, SimObject__getDataField(obj, k, StringTableEntry("")));
		return 1;
	}
}

static int lu_ts_obj_newindex(lua_State *L)
{
	SimObject *obj = checkLuaSimObject(L, 1);
	const char *k = luaL_checkstring(L, 2);
	const char *v = lua_tostring(L, 3);

	/* Printf("mFlags = %i", tso->obj->mFlags);
	Printf("mFlags & ModStaticFields = %i", tso->obj->mFlags & SimObject::ModStaticFields);
	Printf("mFlags & ModDynamicFields = %i", tso->obj->mFlags & SimObject::ModDynamicFields); */

	if (strcmp(k, "_name") == 0)
		return luaL_error(L, "not implemented yet");
	else
		SimObject__setDataField(obj, StringTableEntry(k), StringTableEntry(""), v);

	return 0;
}

static int lu_ts_new(lua_State *L)
{
	const char *className = luaL_checkstring(L, 1);

	SimObject *object = (SimObject *)AbstractClassRep_create_className(className);
	if (object == NULL)
		return luaL_error(L, "failed to create object");

	object->mFlags |= SimObject::ModStaticFields;
	object->mFlags |= SimObject::ModDynamicFields;

	newLuaSimObject(L, object);
	return 1;
}

static int lu_ts_register(lua_State *L)
{
	SimObject *object = checkLuaSimObject(L, 1);

	if (object->id != 0)
		return luaL_error(L, "this object is already registered");
	
	if (!SimObject__registerObject(object))
		return luaL_error(L, "failed to register object");

	return 0;
}

static int lu_ts_schedule(lua_State *L)
{
	unsigned int timeDelta = static_cast<unsigned int>(luaL_checknumber(L, 1));
	/* SimObject *refObject = Sim__getRootGroup();
	SimConsoleEvent *evt = new SimConsoleEvent(argc, func and argv, false);

	signed int ret = Sim__postEvent(refObject, evt, Sim__getCurrentTime() + timeDelta);
	lua_pushnumber(L, ret);
	return 1; */
	return luaL_error(L, "not implemented");
}

static int lu_ts_cancel(lua_State *L)
{
	Sim__cancelEvent(static_cast<unsigned int>(luaL_checknumber(L, 1)));
	return 0;
}

/* MologieDetours::Detour<ShapeNameHudOnRenderFn> *detour_ShapeNameHudOnRender = NULL;

// typedef void(__fastcall *ShapeNameHudOnRenderFn)(SimObject *this_, int, int arg1, int arg2);
// void __stdcall hook_ShapeNameHudOnRender(DWORD* obj, DWORD arg1, DWORD arg2, DWORD arg3)
void __fastcall hook_ShapeNameHudOnRender(DWORD *this_, int dummy, int arg1, int arg2)
{
	// detour_ShapeNameHudOnRender->GetOriginalFunction()(obj, arg1, arg2, arg3);
	detour_ShapeNameHudOnRender->GetOriginalFunction()(this_, dummy, arg1, arg2);

	lua_pushstring(gL, "hook:render2d");
	lua_gettable(gL, LUA_REGISTRYINDEX);

	if (lua_pcall(gL, 0, 0, 0))
		showluaerror(gL);
}

static int lu_ts_hook(lua_State *L)
{
	const char *ev = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (detour_ShapeNameHudOnRender == NULL)
		detour_ShapeNameHudOnRender = new MologieDetours::Detour<ShapeNameHudOnRenderFn>(ShapeNameHudOnRender, (ShapeNameHudOnRenderFn)hook_ShapeNameHudOnRender);

	lua_pushvalue(L, 2);
	lua_pushstring(L, "hook:");
	lua_pushvalue(L, 1);
	lua_concat(L, 2);
	lua_settable(gL, LUA_REGISTRYINDEX);

	return 0;
} */

static int lu_ts_global_index(lua_State *L)
{
	lua_pushstring(L, GetGlobalVariable(luaL_checkstring(L, 2)));
	return 1;
}

static int lu_ts_global_newindex(lua_State *L)
{
	SetGlobalVariable(luaL_checkstring(L, 2), lua_tostring(L, 3));
	return 0;
}

static luaL_reg ts_obj_reg[] = {
	{"__gc", lu_ts_obj_gc},
	{"__index", lu_ts_obj_index},
	{"__newindex", lu_ts_obj_newindex},
	{NULL, NULL}
};

static luaL_Reg lua_ts_global_reg[] = {
	{"__index", lu_ts_global_index},
	{"__newindex", lu_ts_global_newindex},
	{NULL, NULL}
};

static luaL_Reg lua_ts_reg[] = {
	{"eval", lu_ts_eval},
	{"func", lu_ts_func},
	{"expose", lu_ts_expose},
	{"obj", lu_ts_obj},
	{"new", lu_ts_new},
	{"register", lu_ts_register},
	{"schedule", lu_ts_schedule},
	{"cancel", lu_ts_cancel},
	// {"hook", lu_ts_hook},
	{NULL, NULL}
};

bool init()
{
	if (!torque_init())
		return false;

	Printf("   Using %s", LUA_VERSION);

	lua_State *L = luaL_newstate();
	gL = L;

	if (L == NULL)
	{
		Printf("   \x03" "Failed to create Lua state");
		return false;
	}

	luaL_openlibs(L);

	// set up ts_object metatable
	luaL_newmetatable(L, "ts_object_mt");
	luaL_register(L, NULL, ts_obj_reg);
	lua_pop(L, -1);

	// Replace print (use Blockland's echo directly)
	lua_pushcfunction(L, lu_print);
	lua_setglobal(L, "print");

	// Set up the global `ts` table
	lua_newtable(L);
	luaL_register(L, NULL, lua_ts_reg);
	lua_pushstring(L, "global"); // for setting the key on `ts`
	lua_newtable(L); // the `ts.global` table
	lua_newtable(L); // the metatable of the `ts.global` table
	luaL_register(L, NULL, lua_ts_global_reg);
	lua_setmetatable(L, -2);
	lua_rawset(L, -3); // set the `global` key on `ts`
	lua_setglobal(L, "ts");

	// Load all the Lua types into the Lua state
#define BLUA_INIT(name) extern void LuaInit##name(lua_State *L); LuaInit##name(L);

	BLUA_INIT(Player);
	BLUA_INIT(SimSet);
	BLUA_INIT(vec3);

	// set up the con table
	runlua(L, R"lua(
		local func = ts.func
		_G.con = setmetatable({}, {
		  __index = function(t, k)
		    t[k] = func(k)
		    return t[k]
		  end
		})
	)lua");

	ConsoleFunction(NULL, "luaEval", ts_luaEval, "luaEval(string code, bool silent=false) - Execute a chunk of code as Lua.", 2, 3);
	ConsoleFunction(NULL, "luaExec", ts_luaExec, "luaExec(string filename, bool silent=false) - Execute a Lua code file.", 2, 3);
	ConsoleFunction(NULL, "luaCall", ts_luaCall, "luaCall(string name, ...) - Call a Lua function.", 2, 20);

	Printf("   Ready for action");
	return true;
}

bool deinit()
{
	if (gL != NULL)
	{
		lua_close(gL);
		gL = NULL;
	}

	return true;
}

int __stdcall DllMain(HINSTANCE hInstance, unsigned long reason, void *reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		return init();

	case DLL_PROCESS_DETACH:
		return deinit();

	default:
		return true;
	}
}

extern "C" void __declspec(dllexport) __cdecl placeholder(void) { }
