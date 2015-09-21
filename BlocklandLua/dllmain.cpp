#include <cstdlib>
#include "misc.h"

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

	return lua_tostring(gL, -1);
}

static const char *ts_luaExec(SimObject *obj, int argc, const char** argv)
{
	// note: does not use Blockland file system atm, allows execution outside game folder, does not support zip
	
	if (argc < 3 || !istrue(argv[2]))
		Printf("Executing %s.", argv[1]);

	if (luaL_loadfile(gL, argv[1]) || lua_pcall(gL, 0, 1, 0))
	{
		showluaerror(gL, argc >= 3 && istrue(argv[2]));
		return "";
	}

	return lua_tostring(gL, -1);
}

static const char *ts_luaCall(SimObject *obj, int argc, const char** argv)
{
	lua_getglobal(gL, argv[1]);

	for (int i = 2; i < argc; i++)
		lua_pushstring(gL, argv[i]);

	if (lua_pcall(gL, argc - 2, 1, 0))
	{
		showluaerror(gL);
		return "";
	}

	return lua_tostring(gL, -1);
}

// Lua CFunction callbacks
static int lu_print(lua_State *L)
{
	int n = lua_gettop(L);

	/* if (n == 1)
		Printf("%s", lua_tostring(L, 1));
	else if (n == 0)
		Printf("");
	else */
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "blua_tostring");
		int tostring = lua_gettop(L);

		char message[1024];
		message[0] = 0;

		for (int i = 1; i <= n; i++)
		{
			if (i > 1) strcat_s(message, " ");

			lua_pushvalue(L, tostring);
			lua_pushvalue(L, i);
			lua_call(L, 1, 1);
			strcat_s(message, lua_tostring(L, -1));
			lua_pop(L, 1);
		}

		Printf("%s", message);
	}

	return 0;
}

static int lu_ts_eval(lua_State *L)
{
	lua_pushstring(L, Eval(luaL_checkstring(L, 1)));
	return 1;
}

static LuaSimObject *check_ts_object(lua_State *L, int n)
{
	void *ud = luaL_checkudata(L, n, "ts_object_mt");
	luaL_argcheck(L, ud != NULL, 1, "`ts_object' expected");
	return (LuaSimObject *)ud;
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
		LuaSimObject *tso = check_ts_object(L, 1);
		obj = tso->obj;
		
		char idbuf[sizeof(int) * 3 + 2];
		snprintf(idbuf, sizeof idbuf, "%d", obj->mId);

		argv[argc++] = StringTableEntry(idbuf);
	}
	else
		obj = NULL;

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
			LuaSimObject *tso = (LuaSimObject *)lua_touserdata(L, 2 + i);

			// this is a mess! :(
			if (!lua_getmetatable(L, 2 + i))
				return luaL_error(L, "can only pass `ts.obj' userdata to TorqueScript");

			lua_getfield(L, LUA_REGISTRYINDEX, "ts_object_mt");
			int eq = lua_rawequal(L, -2, -1);
			lua_pop(L, 2);

			if (!eq)
				return luaL_error(L, "can only pass `ts.obj' userdata to TorqueScript");

			char idbuf[sizeof(int) * 3 + 2];
			snprintf(idbuf, sizeof idbuf, "%d", tso->obj->mId);

			// alright fine
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

	if (n == 1)
		ns = LookupNamespace(NULL);
	else if (lua_isstring(L, 1))
		ns = LookupNamespace(luaL_checkstring(L, 1));
	else if (luaL_checkudata(L, 1, "ts_object_mt"))
		return luaL_error(L, "not implemented yet");
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
	lua_pushcclosure(L, lu_ts_func_call, 1);
	return 1;
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

	if (obj == NULL)
		return luaL_error(L, "object does not exist");

	newLuaSimObject(L, obj);
	return 1;
}

static int lu_ts_obj_field_index(lua_State *L)
{
	LuaSimObject *tso = check_ts_object(L, lua_upvalueindex(1));
	const char *k = luaL_checkstring(L, 2);
	lua_pushstring(L, SimObject__getDataField(tso->obj, k, StringTableEntry("")));
	return 1;
}

static int lu_ts_obj_field_newindex(lua_State *L)
{
	LuaSimObject *tso = check_ts_object(L, lua_upvalueindex(1));
	const char *k = luaL_checkstring(L, 2);
	const char *v = lua_tostring(L, 3);
	SimObject__setDataField(tso->obj, k, StringTableEntry(""), v);
	return 0;
}

static int lu_ts_obj_index(lua_State *L)
{
	LuaSimObject *tso = check_ts_object(L, 1);
	const char *k = luaL_checkstring(L, 2);

	if (strcmp(k, "_id") == 0)
	{
		lua_pushinteger(L, tso->id);
		return 1;
	}
	else if (strcmp(k, "_name") == 0)
	{
		lua_pushstring(L, tso->obj->objectName);
		return 1;
	}
	else if (strcmp(k, "_exists") == 0)
	{
		SimObject *find = Sim__findObject_id(tso->id);
		lua_pushboolean(L, (int)(find != NULL));
		return 1;
	}
	else
	{
		lua_pushstring(L, SimObject__getDataField(tso->obj, k, StringTableEntry("")));
		return 1;
	}
}

static int lu_ts_obj_newindex(lua_State *L)
{
	LuaSimObject *tso = check_ts_object(L, 1);
	const char *k = luaL_checkstring(L, 2);
	const char *v = lua_tostring(L, 3);

	if (strcmp(k, "_name") == 0)
		return luaL_error(L, "not implemented yet");
	else
		SimObject__setDataField(tso->obj, k, StringTableEntry(""), v);

	return 0;
}

static int lu_ts_new(lua_State *L)
{
	const char *className = luaL_checkstring(L, 1);
	return luaL_error(L, "not implemented yet");
}

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

#define BLUA_INIT(name) extern void LuaInit##name(lua_State *L); LuaInit##name(L);

void LuaInitClasses(lua_State *L)
{
	BLUA_INIT(Player);
	BLUA_INIT(SimSet);
}

// Module setup stuff
DWORD WINAPI Init(LPVOID args)
{
	if (!InitTorqueStuff())
		return 0;

	lua_State *L = luaL_newstate();
	gL = L;

	if (L == NULL)
	{
		Printf("Failed to initialize Lua");
		return 1;
	}

	luaL_openlibs(L);

	lua_getglobal(L, "tostring");
	lua_setfield(L, LUA_REGISTRYINDEX, "blua_tostring");

	// set up ts_object metatable
	luaL_newmetatable(L, "ts_object_mt");
	lua_pushstring(L, "__index"); lua_pushcfunction(L, lu_ts_obj_index); lua_rawset(L, -3);
	lua_pushstring(L, "__newindex"); lua_pushcfunction(L, lu_ts_obj_newindex); lua_rawset(L, -3);
	lua_pop(L, -1);

	// set up global functions
	lua_pushcfunction(L, lu_print); lua_setglobal(L, "print");

	lua_newtable(L);
	lua_pushstring(L, "eval"); lua_pushcfunction(L, lu_ts_eval); lua_rawset(L, -3);
	lua_pushstring(L, "func"); lua_pushcfunction(L, lu_ts_func); lua_rawset(L, -3);
	lua_pushstring(L, "obj"); lua_pushcfunction(L, lu_ts_obj); lua_rawset(L, -3);
	lua_pushstring(L, "grab"); lua_pushstring(L, "obj"); lua_rawget(L, -3); lua_rawset(L, -3);
	lua_pushstring(L, "new"); lua_pushcfunction(L, lu_ts_new); lua_rawset(L, -3);

	// set up ts.global get/set
	lua_pushstring(L, "global");
	lua_newtable(L);
	lua_newtable(L);

	lua_pushstring(L, "__index"); lua_pushcfunction(L, lu_ts_global_index); lua_rawset(L, -3);
	lua_pushstring(L, "__newindex"); lua_pushcfunction(L, lu_ts_global_newindex); lua_rawset(L, -3);

	lua_setmetatable(L, -2);
	lua_rawset(L, -3);

	lua_setglobal(L, "ts");

	LuaInitClasses(L);
	
	// set up the con table
	runlua(L, R"lua(
		local func = ts.func
		_G.con = setmetatable({}, {
		  __index = function(t, k)
			local f = func(k)
		    t[k] = function(...) return f(nil, ...) end
		    return t[k]
		  end
		})
	)lua");

	ConsoleFunction(NULL, "luaEval", ts_luaEval, "luaEval(string code, bool silent=false) - Execute a chunk of code as Lua.", 2, 3);
	ConsoleFunction(NULL, "luaExec", ts_luaExec, "luaExec(string filename, bool silent=false) - Execute a Lua code file.", 2, 3);
	ConsoleFunction(NULL, "luaCall", ts_luaCall, "luaCall(string name, ...) - Call a Lua function.", 2, 20);

	Printf("Lua DLL loaded");
	Sleep(100);

	//runlua(L, "pcall(function()require('autorun')end)");
	Eval("$luaLoaded=true;if(isFunction(\"onLuaLoaded\"))onLuaLoaded();for($i=1;$i<=$luaLoadFunc;$i++)call($luaLoadFunc[$i]);");

	return 0;
}

int WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Init, NULL, 0, NULL);

	return true;
}
