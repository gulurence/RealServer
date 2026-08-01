#pragma once

#include "xBase/xDefine.h"
#include "xBase/xSingleton.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

// LuaVM manages a single Lua state for the entire server process.
// It is a singleton (xSingleton<LuaVM>) following the engine pattern.
// Initializes the Lua VM, loads the SWIG-generated luacore module,
// and provides DoFile/DoString for executing Lua code.

class LuaVM : public xSingleton<LuaVM>
{
public:
    LuaVM();
    ~LuaVM();

    // Initialize the Lua VM with the script root directory.
    // strScriptRoot is the path to the "script/" folder.
    // Sets package.path, loads standard Lua libraries, and initializes
    // the SWIG-generated luacore module plus C-side logging helpers.
    bool Init(const std::string& strScriptRoot);

    // Shut down the Lua VM and release all resources.
    void Release();

    // Execute a Lua file from disk.
    // Returns true on success, false on error (error logged).
    bool DoFile(const std::string& strFilePath);

    // Execute a Lua code string.
    bool DoString(const std::string& strCode);

    // Traverse and load all init.lua files under script/<platform>/*/
    // Each directory is a complete actor implementation.
    // Returns the count of successfully loaded init files.
    int32 LoadAllActors(const std::string& strPlatform);

    // Get the raw lua_State* for use by SWIG wrappers and bridge classes.
    lua_State* GetState() { return m_pL; }

    // Load the SWIG-generated luacore module.
    bool InitLuacoreModule();

private:
    lua_State* m_pL = nullptr;
    std::string m_strScriptRoot;
};
