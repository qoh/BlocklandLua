# BlocklandLua

Exposes LuaJIT to TorqueScript.

Defines the following TorqueScript functions:

* `function luaEval(string code)` - Run code as Lua
* `function luaExec(string filename)` - Execute a Lua source file
* `function luaCall(string name, ...string arguments)` - Call a global Lua function by name with string arguments

From the Lua side, there is a new global table named `ts` containing:

* `function eval(string code)` - Run code as TorqueScript (also available through `eval` global)
* `function echo(string text)` - Add a line to the Blockland console (also available through `echo` global)

FFI is available.
