#pragma once

#include "xActor/Actor.h"
#include "xBase/xDefine.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

// CLuaActor is a concrete CActor subclass that delegates all business
// logic to a Lua script via a Lua registry reference.
//
// Pattern:
//   1. Lua script creates a table with callback methods (onInit, onInstall, etc.)
//   2. Lua calls CLuaActor constructor to create a C++ instance
//   3. Lua calls SetLuaDelegate() to bind the Lua table
//   4. C++ lifecycle calls (Init/Install/UnInstall) are forwarded to Lua
//   5. Create() clones a new CLuaActor of the same type
//
// The C++ side handles only:
//   - Registration of the actor type in ActorCreator
//   - Lifecycle transitions triggered by the server framework
//   - Routing between C++ and Lua

class CLuaActor : public CActor
{
public:
    CLuaActor(ActorType i32ActorType, ActorID i64ActorID,
              const std::string& strActorName);

    // --- CActor lifecycle overrides ---
    // Each delegates to the corresponding Lua method if a delegate is set.
    // Returns true if no Lua handler is configured (default: success).

    virtual bool Init() override;
    virtual bool Install() override;
    virtual bool UnInstall() override;
    virtual CActor* Create(ActorID i64ActorID) override;

    // --- Lua delegate binding ---
    // Bind a Lua table (referred by registry reference) as the business
    // logic handler for this actor instance.
    // The Lua table should have methods: onInit, onInstall, onUnInstall.
    void SetLuaDelegate(lua_State* L, int iLuaTableRef);

    // Get the registry reference for this actor's Lua delegate.
    int GetLuaDelegateRef() const { return m_iLuaDelegateRef; }

    // Get the Lua state for this actor.
    lua_State* GetLuaState() const { return m_pL; }

    // Dispatch an RPC call from gRPC to the Lua delegate.
    // Called by si_actor_dispatch_rpc (SWIG wrapper) from external RPC handlers.
    bool DispatchRpc(const char* rpcName, const char* rpcData);

private:
    // Dispatch a lifecycle method call to the Lua delegate.
    bool CallLuaMethod(const char* pszMethodName);

    lua_State* m_pL = nullptr;
    int m_iLuaDelegateRef = LUA_NOREF;
};
