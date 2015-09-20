#include <cstdlib>
#include "Torque.h"
#include "lua.hpp"

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
		Printf("Lua error: %s", lua_tostring(L, -1));
}

int checkluaerror(lua_State *L, int result)
{
	if (result)
		showluaerror(L);

	return result;
}

static const char *tsf_luaEval(DWORD* obj, int argc, const char** argv)
{
	if (luaL_loadbuffer(gL, argv[1], strlen(argv[1]), "input") || lua_pcall(gL, 0, 1, 0))
	{
		showluaerror(gL, argc >= 3 && istrue(argv[2]));
		return "";
	}

	return lua_tostring(gL, -1);
}

static const char *tsf_luaExec(DWORD *obj, int argc, const char** argv)
{
	// warning: does not use Blockland file system atm, allows execution outside game folder
	
	if (argc < 3 || !istrue(argv[2]))
		Printf("Executing %s.", argv[1]);

	if (luaL_loadfile(gL, argv[1]) || lua_pcall(gL, 0, 1, 0))
	{
		showluaerror(gL, argc >= 3 && istrue(argv[2]));
		return "";
	}

	return lua_tostring(gL, -1);
}

static const char *tsf_luaCall(DWORD *obj, int argc, const char** argv)
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

static int luaf_eval(lua_State *L)
{
	lua_pushstring(L, Eval(luaL_checkstring(L, 1)));
	return 1;
}

static int luaf_print(lua_State *L)
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

static int luaf_call(lua_State *L)
{
	const char *ns = luaL_checkstring(L, 1);
	const char *fn = luaL_checkstring(L, 2);

	if (!strcmp(ns, ""))
		ns = NULL;

	int argc = lua_gettop(L) - 2;
	if (argc > 20)
		argc = 20;

	const char *argv[20];
	argv[0] = fn; argc++;
	for (int i = 0; i < argc; i++)
		argv[i + 1] = lua_tostring(L, 3 + i);

	char *pNamespace = (char *)LookupNamespace(ns);
	if (pNamespace == 0)
	{
		luaL_error(L, "calling function in unknown namespace %s", ns); // Is this even possible?
		return 0;
	}

	char *pEntry = (char *)Namespace__lookup((int)pNamespace, (int)StringTableInsert(StringTable, fn, false));
	if (pEntry == 0)
	{
		luaL_error(L, "calling unknown function %s::%s", ns, fn);
		return 0;
	}

	NamespaceEntryType mType = *(NamespaceEntryType *)(pEntry + 12);
	if (mType == ScriptFunctionType)
	{
		unsigned int mFunctionOffset = *(unsigned int *)(pEntry + 36);

		if (mFunctionOffset)
		{
			char *mCode = *(char **)(pEntry + 32);
			char *mPackage = *(char **)(pEntry + 28);
			int setFrame = *(int *)(pEntry + 12);
			lua_pushstring(L, (const char *)CodeBlock__exec(mCode, mFunctionOffset, fn, pNamespace, argc, argv, false, mPackage, setFrame));
			//lua_pushstring(L, (const char *)CodeBlock__exec(mCode, mFunctionOffset, fn, pNamespace, argc, argv, false, mPackage, 0));
			return 1;
		}
		else
		{
			lua_pushstring(L, "");
			return 0;
		}
	}

	int mMinArgs = *(int *)(pEntry + 16);
	int mMaxArgs = *(int *)(pEntry + 20);
	if ((mMinArgs && argc < mMinArgs) || (mMaxArgs && argc > mMaxArgs))
	{
		luaL_error(L, "wrong number of arguments to %s::%s (expects %d-%d)", ns, fn, mMinArgs, mMaxArgs);
		return 0;
	}

	DWORD *obj = NULL;
	void *cb = (int *)(pEntry + 40);
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

	lua_pushcfunction(L, luaf_print);
	lua_setglobal(L, "print");

	lua_pushcfunction(L, luaf_eval);
	lua_setglobal(L, "ts_eval");

	lua_pushcfunction(L, luaf_call);
	lua_setglobal(L, "ts_call");

	if (!checkluaerror(L, luaL_loadstring(L, R"lua(
		local c = ts_call
		_G.ts = setmetatable({},{
		  __index=function(t,k)
		    t[k]=function(...)
			  return c('', k, ...)
		    end
		    return t[k]
		  end
		})
	)lua"))) {
		checkluaerror(L, lua_pcall(L, 0, 0, 0));
	}

	ConsoleFunction(NULL, "luaEval", tsf_luaEval, "luaEval(string code, bool silent=false) - Execute a chunk of code as Lua.", 2, 3);
	ConsoleFunction(NULL, "luaExec", tsf_luaExec, "luaExec(string filename, bool silent=false) - Execute a Lua code file.", 2, 3);
	ConsoleFunction(NULL, "luaCall", tsf_luaCall, "luaCall(string name, ...) - Call a Lua function.", 2, 20);

	Eval("if(isFunction(\"onLuaLoaded\"))onLuaLoaded();");

	return 0;
}

int WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Init, NULL, 0, NULL);

	return true;
}