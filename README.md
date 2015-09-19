# BlocklandLua

Exposes LuaJIT to TorqueScript. From the TorqueScript side, you can use `luaEval` to execute arbitrary Lua code (given as a string), or `luaExec` to execute a file as Lua code.

From the Lua side, there is a new global table named `ts` containing:

* `function eval(string code)` - Run code as TorqueScript (also available through `eval` global)
* `function echo(string text)` - Add a line to the Blockland console (also available through `echo` global)

FFI is available.
