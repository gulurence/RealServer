#pragma once

#include "xBase/xDefine.h"
#include "xBase/xSingleton.h"

#include <string>
#include <utility>
#include <vector>

extern "C"
{
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
    bool Init(const std::string &strScriptRoot);

    // Shut down the Lua VM and release all resources.
    void Release();

    // Execute a Lua file from disk.
    // Returns true on success, false on error (error logged).
    bool DoFile(const std::string &strFilePath);

    // Execute a Lua code string.
    bool DoString(const std::string &strCode);

    // Load init.lua under script/<platform>/<actorDir>/ for the actor types this
    // process hosts (passed in hostedTypes, derived from GetHostedActorTypes).
    // Each directory is a complete actor implementation.
    // Returns the count of successfully loaded init files.
    int32 LoadAllActors(const std::string &strPlatform, const std::vector<EActorType> &hostedTypes);

    // 脚本热更: 运行时重载 script/actors/** 业务脚本, 不停机、不丢业务状态。
    // 清空 package.loaded 里的 actors.* 缓存使业务脚本重跑, 再复用最近一次
    // LoadAllActors 的 platform + hostedTypes 重跑各 init.lua。
    // 框架模块 (common.class / common.actor.actor_mgr 等) 不清缓存, 其 file-local
    // 注册表 (ClassRegistry / protoRegistry / localInstances) 与单例内存态跨重载保留,
    // class.lua define() 据此原地更新旧类表, 既有实例自动获得新方法。
    // 必须由调度线程调用 (进程唯一 lua_State 约束)。
    bool HotReload();

    // 定向脚本热更 (数据驱动): 对 (relPath, 内存源码) 列表逐条调
    // xEngine.HotFix.UpdateFilesData 用内存内容热更 (load, 非 loadfile 读盘)。
    // 返回「成功热更」的 relPath 列表 (调用方据此只把成功文件写回本地磁盘)。
    // 必须由调度线程调用 (进程唯一 lua_State 约束)。
    std::vector<std::string> HotReloadFiles(const std::vector<std::pair<std::string, std::string>> &files);

    // 热更预检查: 对 (relPath, 内存源码) 列表逐条 load(..., "t") 做语法校验 (不执行)。
    // 返回 (全部通过?, 错误摘要字符串 "rel: err\n..." 换行分隔)。
    // 必须由调度线程调用 (进程唯一 lua_State 约束)。
    std::pair<bool, std::string> HotReloadPrecheck(const std::vector<std::pair<std::string, std::string>> &files);

    // 脚本根目录 (script/ 路径), 供 GM 热更命令写回拉取到的文件。
    const std::string &GetScriptRoot() const
    {
        return m_strScriptRoot;
    }

    // Get the raw lua_State* for use by SWIG wrappers and bridge classes.
    lua_State *GetState()
    {
        return m_pL;
    }

    // Load the SWIG-generated luacore module.
    bool InitLuacoreModule();

private:
    lua_State *m_pL = nullptr;
    std::string m_strScriptRoot;
    std::string m_strPlatform;                 // 最近一次 LoadAllActors 的平台 (热更复用)
    std::vector<EActorType> m_vecHostedTypes;  // 最近一次 LoadAllActors 的托管类型 (热更复用)
};
