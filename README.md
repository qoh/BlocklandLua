# BlocklandLua

Exposes LuaJIT to TorqueScript.

From the TorqueScript side, you can use `luaEval` to execute arbitrary Lua code (given as a string), `luaExec` to execute a file as Lua code, and `luaCall` to call global Lua functions by name.

    luaEval("print(5)"); // -> 5
    echo(luaEval("return 5")); // -> 5
    luaExec("config/test.lua");
    luaCall("foo", "bar", 1, 2, 3); // calls function foo

From the Lua side, there's a new global table called `ts` with the following keys:

* `ts.eval(string code)` - Run TorqueScript code in the global scope
* `ts.func(string namespace, string function)` - Get a Lua function which calls a TorqueScript function (mind the leading `this` argument)
* `ts.obj(string name)` *or* `ts.obj(number id)` - Get a handle to a TorqueScript object that can be passed as the `this` argument to a function resolved with `ts.func`. (there's also an alias of `ts.obj` called `ts.grab`).
* `ts.global` - Get/set keys of this table to work with global TorqueScript variables.

To work with object methods, first resolve the function (once) and later on call it with your object.

    local getCount = ts.func("SimSet", "getCount")
    local getObject = ts.func("SimSet", "getObject")

    local ClientGroup = ts.obj "ClientGroup"

    for i=0, getCount(ClientGroup)-1 do
      local client = ts.obj(getObject(ClientGroup, i))
      -- ...
    end

There is also a global `con` table which provides simple access to TorqueScript functions in the global namespace. Just call functions on it.

    con.getRandom(1, 6) ---> 4
    con.getSubStr("hello", 0, 2) ---> "he"

FFI is available.
