#include <Windows.h>

enum NamespaceEntryType {
	GroupMarker = -3,
	OverloadMarker = -2,
	InvalidFunctionType = -1,
	ScriptFunctionType,
	StringCallbackType,
	IntCallbackType,
	FloatCallbackType,
	VoidCallbackType,
	BoolCallbackType
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Macros

//Typedef an engine function to use it later
#define BLFUNC(returnType, convention, name, ...)         \
	typedef returnType (convention*name##Fn)(__VA_ARGS__); \
	static name##Fn name;

//Typedef an exported engine function to use it later
#define BLFUNC_EXTERN(returnType, convention, name, ...)  \
	typedef returnType (convention*name##Fn)(__VA_ARGS__); \
	extern name##Fn name;

//Search for an engine function in blockland
#define BLSCAN(target, pattern, mask)            \
	target = (target##Fn)ScanFunc(pattern, mask); \
	if(target == NULL)                             \
		Printf("torquedll | Cannot find function "#target"!");

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Engine function declarations

//Con::printf
BLFUNC_EXTERN(void, , Printf, const char* format, ...);

extern const char *StringTableEntry(const char *str, bool caseSensitive=false);
extern DWORD StringTable;
BLFUNC_EXTERN(DWORD*, , LookupNamespace, const char* ns);
BLFUNC_EXTERN(const char*, __thiscall, StringTableInsert, DWORD stringTablePtr, const char* val, const bool caseSensitive)
BLFUNC_EXTERN(void*, __thiscall, Namespace__lookup, int namespacePtr, int name)
BLFUNC_EXTERN(void*, __thiscall, CodeBlock__exec, void *this_, unsigned int ip, const char *functionName, void *thisNamespace, unsigned int argc, const char **argv, bool noCalls, void *packageName, int setFrame)
BLFUNC_EXTERN(DWORD*, , Sim__findObject_name, const char *name);
BLFUNC_EXTERN(DWORD*, , Sim__findObject_id, unsigned int id);

//Callback types for exposing methods to torquescript
typedef const char* (*StringCallback)(DWORD* obj, int argc, const char* argv[]);
typedef int(*IntCallback)(DWORD* obj, int argc, const char* argv[]);
typedef float(*FloatCallback)(DWORD* obj, int argc, const char* argv[]);
typedef void(*VoidCallback)(DWORD* obj, int argc, const char* argv[]);
typedef bool(*BoolCallback)(DWORD* obj, int argc, const char* argv[]);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Public functions

//Scan the module for a pattern
DWORD ScanFunc(char* pattern, char* mask);

//Change a byte at a specific location in memory
void PatchByte(BYTE* location, BYTE value);

//Register a torquescript function that returns a string. The function must look like this:
//const char* func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, StringCallback callBack, const char* usage, int minArgs, int maxArgs);

//Register a torquescript function that returns an int. The function must look like this:
//int func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, IntCallback callBack, const char* usage, int minArgs, int maxArgs);

//Register a torquescript function that returns a float. The function must look like this:
//float func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, FloatCallback callBack, const char* usage, int minArgs, int maxArgs);

//Register a torquescript function that returns nothing. The function must look like this:
//void func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, VoidCallback callBack, const char* usage, int minArgs, int maxArgs);

//Register a torquescript function that returns a bool. The function must look like this:
//bool func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, BoolCallback callBack, const char* usage, int minArgs, int maxArgs);

//Expose an integer variable to torquescript
void ConsoleVariable(const char* name, int* data);

//Expose a boolean variable to torquescript
void ConsoleVariable(const char* name, bool* data);

//Expose a float variable to torquescript
void ConsoleVariable(const char* name, float* data);

//Expose a string variable to torquescript
void ConsoleVariable(const char* name, char* data);

//Evaluate a torquescript string in global scope
const char* Eval(const char* str);

BLFUNC_EXTERN(void, , SetGlobalVariable, const char *name, const char *value);
BLFUNC_EXTERN(char *, , GetGlobalVariable, const char *name);

typedef void(__thiscall* ShapeNameHudOnRenderFn)(DWORD* obj, DWORD arg1, DWORD arg2, DWORD arg3);
static ShapeNameHudOnRenderFn ShapeNameHudOnRender;

//Initialize the Torque Interface
bool InitTorqueStuff();