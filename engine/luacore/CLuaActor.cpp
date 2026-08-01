#include "CLuaActor.h"
#include "LuaVM.h"
#include "xLog/xLog.h"

CLuaActor::CLuaActor(ActorType i32ActorType, ActorID i64ActorID,
                     const std::string& strActorName)
    : CActor(i32ActorType, i64ActorID, strActorName)
{
    // Get the global Lua state from the VM singleton
    m_pL = LuaVM::instance()->GetState();
    XLOG("[INIT] CLuaActor created: type=%d id=%lld name=%s",
         i32ActorType, i64ActorID, strActorName.c_str());
}

// -------------------------------------------------------------------
// Lifecycle Delegation
// -------------------------------------------------------------------

bool CLuaActor::Init() {
    XLOG("[INIT] CLuaActor::Init type=%d id=%lld", GetActorType(), GetActorID());
    return CallLuaMethod("onInit");
}

bool CLuaActor::Install() {
    XLOG("[INIT] CLuaActor::Install type=%d id=%lld name=%s",
         GetActorType(), GetActorID(), GetActorName().c_str());
    return CallLuaMethod("onInstall");
}

bool CLuaActor::UnInstall() {
    XLOG("[INIT] CLuaActor::UnInstall type=%d id=%lld", GetActorType(), GetActorID());
    return CallLuaMethod("onUnInstall");
}

CActor* CLuaActor::Create(ActorID i64ActorID) {
    // Clone this actor type with a new ActorID.
    auto* pNew = new CLuaActor(GetActorType(), i64ActorID, GetActorName());
    if (!pNew->CreateIsEnable()) {
        XERR("[INIT] CLuaActor::Create failed — create disabled for type=%d", GetActorType());
        delete pNew;
        return nullptr;
    }
    XLOG("[INIT] CLuaActor::Create cloned: type=%d new_id=%lld",
         GetActorType(), i64ActorID);
    return pNew;
}

// -------------------------------------------------------------------
// Lua Delegate Binding
// -------------------------------------------------------------------

void CLuaActor::SetLuaDelegate(lua_State* L, int iLuaTableRef) {
    m_pL = L;

    // Release previous delegate if one exists
    if (m_iLuaDelegateRef != LUA_NOREF) {
        luaL_unref(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef);
    }

    m_iLuaDelegateRef = iLuaTableRef;
}

// -------------------------------------------------------------------
// RPC Dispatch — gRPC → Lua actor
// -------------------------------------------------------------------

bool CLuaActor::DispatchRpc(const char* rpcName, const char* rpcData) {
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF) {
        return true;  // no Lua handler — no-op
    }

    // Push the delegate table from the registry
    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef);  // [table]

    // Get on_rpc_call method
    lua_getfield(m_pL, -1, "on_rpc_call");                     // [table, method]

    if (!lua_isfunction(m_pL, -1)) {
        // on_rpc_call not defined — default to success
        lua_pop(m_pL, 2);
        return true;
    }

    // Push self (delegate table)
    lua_pushvalue(m_pL, -2);                                   // [table, method, self]

    // Push rpc_name (arg 1)
    lua_pushstring(m_pL, rpcName);                             // [table, method, self, rpcName]

    // Push rpc_data (arg 2)
    lua_pushstring(m_pL, rpcData);                             // [table, method, self, rpcName, rpcData]

    // Call on_rpc_call(self, rpcName, rpcData) — 2 args, 1 boolean result
    if (lua_pcall(m_pL, 2, 1, 0) != LUA_OK) {
        XERR("[CLuaActor:%s] Lua error in RPC dispatch '%s': %s",
             GetActorName().c_str(),
             rpcName,
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2);
        return false;
    }

    bool bResult = lua_toboolean(m_pL, -1);
    lua_pop(m_pL, 2);
    return bResult;
}

// -------------------------------------------------------------------
// Lua Method Dispatch
// -------------------------------------------------------------------

bool CLuaActor::CallLuaMethod(const char* pszMethodName) {
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF) {
        // No Lua handler set — treat as success (no-op)
        return true;
    }

    // Push the delegate table from the registry
    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef);  // [table]

    // Get the named method from the table
    lua_getfield(m_pL, -1, pszMethodName);                     // [table, method]

    if (!lua_isfunction(m_pL, -1)) {
        // Method not defined in Lua — default to success
        lua_pop(m_pL, 2);  // pop method and table
        return true;
    }

    // Push the table as self (the first argument to the method call)
    lua_pushvalue(m_pL, -2);  // [table, method, table(self)]

    // Call method with 1 argument (self), expect 1 boolean result
    if (lua_pcall(m_pL, 1, 1, 0) != LUA_OK) {
        XERR("[CLuaActor:%s] Lua error in '%s': %s",
             GetActorName().c_str(),
             pszMethodName,
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2);  // pop error and table
        return false;
    }

    // Get the boolean result
    bool bResult = lua_toboolean(m_pL, -1);
    lua_pop(m_pL, 2);  // pop result and table
    return bResult;
}
