#include <cstdlib>
#include "Torque.h"
#include "lua.hpp"

lua_State *gL;

typedef struct {
	DWORD *obj;
	unsigned int id;
} ts_object_t;

bool istrue(const char *arg)
{
	return !_stricmp(arg, "true") || !_stricmp(arg, "1") || (0 != atoi(arg));
}

void showluaerror(lua_State *L, bool silent=false)
{
	if (silent)
		lua_pop(L, -1);
	else
		Printf("Lua error: %s", lua_tostring(L, -1));
}

void runlua(lua_State *L, const char *code, const char *name="input")
{
	if (luaL_loadbuffer(L, code, strlen(code), name) || lua_pcall(L, 0, 0, 0))
		showluaerror(L);
}

// TorqueScript ConsoleFunction callbacks
static const char *ts_luaEval(DWORD* obj, int argc, const char** argv)
{
	if (luaL_loadbuffer(gL, argv[1], strlen(argv[1]), "input") || lua_pcall(gL, 0, 1, 0))
	{
		showluaerror(gL, argc >= 3 && istrue(argv[2]));
		return "";
	}

	return lua_tostring(gL, -1);
}

static const char *ts_luaExec(DWORD *obj, int argc, const char** argv)
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

static const char *ts_luaCall(DWORD *obj, int argc, const char** argv)
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

	if (n == 1)
		Printf("%s", lua_tostring(L, 1));
	else if (n == 0)
		Printf("");
	else
	{
		char message[1024];
		message[0] = 0;

		for (int i = 1; i <= n; i++)
		{
			if (i > 1) strcat_s(message, " ");
			strcat_s(message, lua_tostring(L, i));
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

static ts_object_t *check_ts_object(lua_State *L, int n)
{
	void *ud = luaL_checkudata(L, n, "ts_object_mt");
	luaL_argcheck(L, ud != NULL, 1, "`ts_object' expected");
	return (ts_object_t *)ud;
}

static int lu_ts_func_call(lua_State *L)
{
	char *nsEntry = (char *)lua_touserdata(L, lua_upvalueindex(1));

	int argc = 0;
	const char *argv[21];

	argv[argc++] = *(char **)(nsEntry + 8);

	DWORD *obj;
	if (!lua_isnil(L, 1))
	{
		ts_object_t *tso = check_ts_object(L, 1);
		obj = tso->obj;
		argv[argc++] = ""; // should be the object ID in a string
	}
	else
		obj = NULL;

	int n = lua_gettop(L) - 1;
	if (n > 19)
		n = 19;

	for (int i = 0; i < n; i++)
		argv[argc++] = lua_tostring(L, 2 + i);

	NamespaceEntryType mType = *(NamespaceEntryType *)(nsEntry + 12);
	if (mType == ScriptFunctionType)
	{
		unsigned int mFunctionOffset = *(unsigned int *)(nsEntry + 36);

		if (mFunctionOffset)
		{
			char *mNamespace = (char *)(*nsEntry);
			char *mFunctionName = *(char **)(nsEntry + 8);

			char *mCode = *(char **)(nsEntry + 32);
			char *mPackage = *(char **)(nsEntry + 28);
			//int setFrame = *(int *)(pEntry + 12);
			//lua_pushstring(L, (const char *)CodeBlock__exec(mCode, mFunctionOffset, fn, pNamespace, argc, argv, false, mPackage, setFrame));
			lua_pushstring(L, (const char *)CodeBlock__exec(mCode, mFunctionOffset, mNamespace, mFunctionName, argc, argv, false, mPackage, 0));
		}
		else
			lua_pushstring(L, "");

		return 1;
	}

	int mMinArgs = *(int *)(nsEntry + 16);
	int mMaxArgs = *(int *)(nsEntry + 20);
	if ((mMinArgs && argc < mMinArgs) || (mMaxArgs && argc > mMaxArgs))
	{
		luaL_error(L, "wrong number of arguments (expects %d-%d)", mMinArgs, mMaxArgs);
		return 0;
	}

	void *cb = (int *)(nsEntry + 40);
	switch (mType)
	{
	case StringCallbackType: lua_pushstring(L, (*(StringCallback *)cb)(obj, argc, argv)); return 1;
	case IntCallbackType: lua_pushnumber(L, (*(IntCallback *)cb)(obj, argc, argv)); return 1;
	case FloatCallbackType: lua_pushnumber(L, (*(FloatCallback *)cb)(obj, argc, argv)); return 1;
	case BoolCallbackType: lua_pushboolean(L, (*(BoolCallback *)cb)(obj, argc, argv)); return 1;
	case VoidCallbackType: (*(VoidCallback *)cb)(obj, argc, argv); return 0;
	}

	luaL_error(L, "invalid function call");
	return 0;
}

static int lu_ts_func(lua_State *L)
{
	const char *nsName = luaL_checkstring(L, 1);
	const char *fnName = luaL_checkstring(L, 2);

	if (!strcmp(nsName, ""))
		nsName = NULL;

	char *ns = (char *)LookupNamespace(nsName);
	char *nsEntry = (char *)Namespace__lookup((int)ns, (int)StringTableInsert(StringTable, fnName, false));

	if (nsEntry == NULL)
	{
		luaL_error(L, "unknown function %s::%s", nsName, fnName);
		return 0;
	}

	lua_pushlightuserdata(L, nsEntry);
	lua_pushcclosure(L, lu_ts_func_call, 1);
	return 1;
}

static int lu_ts_obj(lua_State *L)
{
	int t = lua_type(L, 1);
	DWORD *obj;

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

	ts_object_t *tso = (ts_object_t *)lua_newuserdata(L, sizeof ts_object_t);
	luaL_getmetatable(L, "ts_object_mt");
	lua_setmetatable(L, -2);

	tso->obj = obj;
	tso->id = *(unsigned int *)((char *)obj + 32);
	
	return 1;
}

static int lu_ts_obj_index(lua_State *L)
{
	ts_object_t *tso = check_ts_object(L, 1);
	const char *k = luaL_checkstring(L, 2);

	if (strcmp(k, "id") == 0)
	{
		lua_pushinteger(L, *(int *)((char *)tso->obj + 32));
		return 1;
	}
	else if (strcmp(k, "name") == 0)
	{
		char *name = *((char **)tso->obj + 4);
		lua_pushstring(L, name);
		return 1;
	}
	else if (strcmp(k, "exists") == 0)
	{
		lua_pushboolean(L, Sim__findObject_id(tso->id) != NULL);
		return 1;
	}
	else
		return luaL_error(L, "unknown key %s", k);
}

static int lu_ts_obj_newindex(lua_State *L)
{
	ts_object_t *tso = check_ts_object(L, 1);
	const char *k = luaL_checkstring(L, 2);

	if (strcmp(k, "name") == 0)
	{
		return luaL_error(L, "can't do this yet");
	}
	else
		return luaL_error(L, "unknown key %s", k);
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

	// set up ts.global get/set
	lua_pushstring(L, "global");
	lua_newtable(L);
	lua_newtable(L);

	lua_pushstring(L, "__index"); lua_pushcfunction(L, lu_ts_global_index); lua_rawset(L, -3);
	lua_pushstring(L, "__newindex"); lua_pushcfunction(L, lu_ts_global_newindex); lua_rawset(L, -3);

	lua_setmetatable(L, -2);
	lua_rawset(L, -3);

	lua_setglobal(L, "ts");
	
	// set up the con table
	runlua(L, R"lua(
		local func = ts.func
		_G.con = setmetatable({}, {
		  __index = function(t, k)
			local f = func("", k)
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
