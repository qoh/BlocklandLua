# BlocklandLua

Exposes LuaJIT to TorqueScript.

From the TorqueScript side, you can use `luaEval` to execute arbitrary Lua code (given as a string), `luaExec` to execute a file as Lua code, and `luaCall` to call global Lua functions by name.

From the Lua side, there are the following new globals:

* `function ts_eval(string code)` - Run code as TorqueScript
* `function ts_call(string namespace, string function, ...)` - Call a TorqueScript function by name (for non-namespaced functions, use an empty string for `namespace`). Arguments will be converted to strings.
* `table ts` - Provides shortcuts to `ts_call`. `call('', 'getRandom')` can be replaced with `ts.getRandom()`.

FFI is available.
