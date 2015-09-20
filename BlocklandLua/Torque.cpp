#include "Torque.h"
#include <Psapi.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Global variables

//Start of the Blockland.exe module in memory
static DWORD ImageBase;

//Length of the Blockland.exe module
static DWORD ImageSize;

//StringTable pointer
DWORD StringTable;

//Global Variable dictionary pointer
static DWORD GlobalVars;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Engine function declarations

//Con::printf
PrintfFn Printf;

//Con::lookupNamespace
LookupNamespaceFn LookupNamespace;

//StringTable::insert
StringTableInsertFn StringTableInsert;
Namespace__lookupFn Namespace__lookup;
CodeBlock__execFn CodeBlock__exec;

//Namespace::addCommand overloads
BLFUNC(void, __thiscall, AddStringCommand, DWORD* ns, const char* name, StringCallback cb, const char *usage, int minArgs, int maxArgs);
BLFUNC(void, __thiscall, AddIntCommand, DWORD* ns, const char* name, IntCallback cb, const char *usage, int minArgs, int maxArgs);
BLFUNC(void, __thiscall, AddFloatCommand, DWORD* ns, const char* name, FloatCallback cb, const char *usage, int minArgs, int maxArgs);
BLFUNC(void, __thiscall, AddVoidCommand, DWORD* ns, const char* name, VoidCallback cb, const char *usage, int minArgs, int maxArgs);
BLFUNC(void, __thiscall, AddBoolCommand, DWORD* ns, const char* name, BoolCallback cb, const char *usage, int minArgs, int maxArgs);

//Exposing variables to torquescript
BLFUNC(void, __thiscall, AddVariable, DWORD dictionaryPtr, const char* name, int type, void* data);

//Executing code and calling torquescript functions
BLFUNC(const char*, , Evaluate, const char* string, bool echo, const char* fileName);

SetGlobalVariableFn SetGlobalVariable;
GetGlobalVariableFn GetGlobalVariable;
//BLFUNC(void, , SetGlobalVariable, const char *name, const char *value);
//BLFUNC(char *, , GetGlobalVariable, const char *name);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions

//Set the module start and length
void InitScanner(char* moduleName)
{
	//Find the module
	HMODULE module = GetModuleHandleA(moduleName);

	if (module)
	{
		//Retrieve information about the module
		MODULEINFO info;
		GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(MODULEINFO));

		//Store relevant information
		ImageBase = (DWORD)info.lpBaseOfDll;
		ImageSize = info.SizeOfImage;
	}
}

//Compare data at two locations for equality
bool CompareData(PBYTE data, PBYTE pattern, char* mask)
{
	//Iterate over the data, pattern and mask in parallel
	for (; *mask; ++data, ++pattern, ++mask)
	{
		//And check for equality at each unmasked byte
		if (*mask == 'x' && *data != *pattern)
			return false;
	}

	return (*mask) == NULL;
}

//Find a pattern in memory
DWORD FindPattern(DWORD imageBase, DWORD imageSize, PBYTE pattern, char* mask)
{
	//Iterate over the image
	for (DWORD i = imageBase; i < imageBase + imageSize; i++)
	{
		//And check for matching pattern at every byte
		if (CompareData((PBYTE)i, pattern, mask))
			return i;
	}

	return 0;
}

//Scan the module for a pattern
DWORD ScanFunc(char* pattern, char* mask)
{
	//Just search for the pattern in the module
	return FindPattern(ImageBase, ImageSize - strlen(mask), (PBYTE)pattern, mask);
}

//Change a byte at a specific location in memory
void PatchByte(BYTE* location, BYTE value)
{
	//Remove protection
	DWORD oldProtection;
	VirtualProtect(location, 1, PAGE_EXECUTE_READWRITE, &oldProtection);

	//Change value
	*location = value;

	//Restore protection
	VirtualProtect(location, 1, oldProtection, &oldProtection);
}

//Register a torquescript function that returns a string. The function must look like this:
//const char* func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, StringCallback callBack, const char* usage, int minArgs, int maxArgs)
{
	AddStringCommand(LookupNamespace(nameSpace), StringTableInsert(StringTable, name, false), callBack, usage, minArgs, maxArgs);
}

//Register a torquescript function that returns an int. The function must look like this:
//int func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, IntCallback callBack, const char* usage, int minArgs, int maxArgs)
{
	AddIntCommand(LookupNamespace(nameSpace), StringTableInsert(StringTable, name, false), callBack, usage, minArgs, maxArgs);
}

//Register a torquescript function that returns a float. The function must look like this:
//float func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, FloatCallback callBack, const char* usage, int minArgs, int maxArgs)
{
	AddFloatCommand(LookupNamespace(nameSpace), StringTableInsert(StringTable, name, false), callBack, usage, minArgs, maxArgs);
}

//Register a torquescript function that returns nothing. The function must look like this:
//void func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, VoidCallback callBack, const char* usage, int minArgs, int maxArgs)
{
	AddVoidCommand(LookupNamespace(nameSpace), StringTableInsert(StringTable, name, false), callBack, usage, minArgs, maxArgs);
}

//Register a torquescript function that returns a bool. The function must look like this:
//bool func(DWORD* obj, int argc, const char* argv[])
void ConsoleFunction(const char* nameSpace, const char* name, BoolCallback callBack, const char* usage, int minArgs, int maxArgs)
{
	AddBoolCommand(LookupNamespace(nameSpace), StringTableInsert(StringTable, name, false), callBack, usage, minArgs, maxArgs);
}

//Expose an integer variable to torquescript
void ConsoleVariable(const char* name, int* data)
{
	AddVariable(GlobalVars, name, 4, data);
}

//Expose a boolean variable to torquescript
void ConsoleVariable(const char* name, bool* data)
{
	AddVariable(GlobalVars, name, 6, data);
}

//Expose a float variable to torquescript
void ConsoleVariable(const char* name, float* data)
{
	AddVariable(GlobalVars, name, 8, data);
}

//Expose a string variable to torquescript
void ConsoleVariable(const char* name, char* data)
{
	AddVariable(GlobalVars, name, 10, data);
}

//Evaluate a torquescript string in global scope
const char* Eval(const char* str)
{
	return Evaluate(str, false, NULL);
}

//Initialize the Torque Interface
bool InitTorqueStuff()
{
	//Init the scanner
	InitScanner("Blockland.exe");

	//Printf is required for debug output, so find it first
	Printf = (PrintfFn)ScanFunc("\x8B\x4C\x24\x04\x8D\x44\x24\x08\x50\x6A\x00\x6A\x00\xE8\x00\x00\x00\x00\x83\xC4\x0C\xC3", "xxxxxxxxxxxxxx????xxxx");

	//Do nothing if we don't find it :(
	if (!Printf)
		return false;

	ShapeNameHudOnRender = (ShapeNameHudOnRenderFn)ScanFunc("\x81\xec\x00\x00\x00\x00\x53\x8b\xd9\x8a\x83\xc9\x00\x00\x00\x84\xc0\x55\x56\x57\x89\x5c\x24\x14", "xx????xxxxxxxxxxxxxxxxxx");

	//First find all the functions
	BLSCAN(LookupNamespace, "\x8B\x44\x24\x04\x85\xC0\x75\x05", "xxxxxxxx");
	BLSCAN(StringTableInsert, "\x53\x8B\x5C\x24\x08\x55\x56\x57\x53", "xxxxxxxxx");
	BLSCAN(Namespace__lookup, "\x53\x56\x8B\xF1\x8B\x46\x24", "xxxxxxx");
	BLSCAN(CodeBlock__exec, "\x83\xEC\x44\x53\x55\x56\x8B\xE9", "xxxxxxxx");

	//These are almost identical. Long sigs required
	BLSCAN(AddStringCommand,
		"\x8B\x44\x24\x04\x56\x50\xE8\x00\x00\x00\x00\x8B\xF0\xA1\x00\x00\x00\x00\x40\xB9\x00\x00\x00\x00\xA3"
		"\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\x4C\x24\x10\x8B\x54\x24\x14\x8B\x44\x24\x18\x89\x4E\x18\x8B"
		"\x4C\x24\x0C\x89\x56\x10\x89\x46\x14\xC7\x46\x0C\x01\x00\x00\x00\x89\x4E\x28\x5E\xC2\x14\x00",
		"xxxxxxx????xxx????xx????x????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

	BLSCAN(AddIntCommand,
		"\x8B\x44\x24\x04\x56\x50\xE8\x00\x00\x00\x00\x8B\xF0\xA1\x00\x00\x00\x00\x40\xB9\x00\x00\x00\x00\xA3"
		"\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\x4C\x24\x10\x8B\x54\x24\x14\x8B\x44\x24\x18\x89\x4E\x18\x8B"
		"\x4C\x24\x0C\x89\x56\x10\x89\x46\x14\xC7\x46\x0C\x02\x00\x00\x00\x89\x4E\x28\x5E\xC2\x14\x00",
		"xxxxxxx????xxx????xx????x????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

	BLSCAN(AddFloatCommand,
		"\x8B\x44\x24\x04\x56\x50\xE8\x00\x00\x00\x00\x8B\xF0\xA1\x00\x00\x00\x00\x40\xB9\x00\x00\x00\x00\xA3"
		"\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\x4C\x24\x10\x8B\x54\x24\x14\x8B\x44\x24\x18\x89\x4E\x18\x8B"
		"\x4C\x24\x0C\x89\x56\x10\x89\x46\x14\xC7\x46\x0C\x03\x00\x00\x00\x89\x4E\x28\x5E\xC2\x14\x00",
		"xxxxxxx????xxx????xx????x????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

	BLSCAN(AddVoidCommand,
		"\x8B\x44\x24\x04\x56\x50\xE8\x00\x00\x00\x00\x8B\xF0\xA1\x00\x00\x00\x00\x40\xB9\x00\x00\x00\x00\xA3"
		"\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\x4C\x24\x10\x8B\x54\x24\x14\x8B\x44\x24\x18\x89\x4E\x18\x8B"
		"\x4C\x24\x0C\x89\x56\x10\x89\x46\x14\xC7\x46\x0C\x04\x00\x00\x00\x89\x4E\x28\x5E\xC2\x14\x00",
		"xxxxxxx????xxx????xx????x????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

	BLSCAN(AddBoolCommand,
		"\x8B\x44\x24\x04\x56\x50\xE8\x00\x00\x00\x00\x8B\xF0\xA1\x00\x00\x00\x00\x40\xB9\x00\x00\x00\x00\xA3"
		"\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\x4C\x24\x10\x8B\x54\x24\x14\x8B\x44\x24\x18\x89\x4E\x18\x8B"
		"\x4C\x24\x0C\x89\x56\x10\x89\x46\x14\xC7\x46\x0C\x05\x00\x00\x00\x89\x4E\x28\x5E\xC2\x14\x00",
		"xxxxxxx????xxx????xx????x????x????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

	BLSCAN(AddVariable, "\x8B\x44\x24\x04\x56\x8B\xF1\x80\x38\x24\x74\x1A", "xxxxxxxxxxxx");
	BLSCAN(Evaluate, "\x8A\x44\x24\x08\x84\xC0\x56\x57\x8B\x7C\x24\x0C", "xxxxxxxxxxxx");

	BLSCAN(GetGlobalVariable, "\x56\x8b\x74\x24\x08\x85\xf6\x74\x00\x8a\x06\x84\xc0\x75", "xxxxxxxx?xxxxx");
	BLSCAN(SetGlobalVariable, "\x56\x8b\x74\x24\x08\x80\x3e\x24\x8b\xc6\x74\x00\x56\xe8", "xxxxxxxxxxx?xx");

	//The string table is used in lookupnamespace so we can get it's location
	StringTable = *(DWORD*)(*(DWORD*)((DWORD)LookupNamespace + 15));

	//Get the global variable dictionary pointer
	GlobalVars = *(DWORD*)(ScanFunc("\x8B\x44\x24\x0C\x8B\x4C\x24\x04\x50\x6A\x06", "xxxxxxxxxxx") + 13);

	return true;
}