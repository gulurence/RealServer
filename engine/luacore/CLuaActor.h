#pragma once

#include "xActor/Actor.h"
#include "xBase/xDefine.h"

extern "C"
{
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
    CLuaActor(EActorType i32ActorType, ActorID i64ActorID, const std::string &strActorName);
    ~CLuaActor();

    // --- CActor lifecycle overrides ---
    // Each delegates to the corresponding Lua method if a delegate is set.
    // Returns true if no Lua handler is configured (default: success).

    virtual bool Init() override;
    virtual bool Install() override;
    virtual bool UnInstall() override;
    virtual CActor *Create(ActorID i64ActorID) override;

    // --- Lua delegate binding ---
    // Bind a Lua table (referred by registry reference) as the business
    // logic handler for this actor instance.
    // The Lua table should have methods: onInit, onInstall, onUnInstall.
    void SetLuaDelegate(lua_State *L, int32 iLuaTableRef);

    // Get the registry reference for this actor's Lua delegate.
    int32 GetLuaDelegateRef() const
    {
        return m_iLuaDelegateRef;
    }

    // Get the Lua state for this actor.
    lua_State *GetLuaState() const
    {
        return m_pL;
    }

    // Dispatch an RPC call from gRPC to the Lua delegate.
    // Called by si_actor_dispatch_rpc (SWIG wrapper) from external RPC handlers.
    bool DispatchRpc(const char *rpcName, const char *rpcData);

    // Dispatch an RPC call and capture a string reply from the Lua handler.
    // rpcData 为二进制安全 (dataLen 显式给出), on_rpc_call 返回字符串作为响应。
    // @return true 若 Lua handler 存在且调用成功 (outReply 已填充, 可能为空串)
    bool DispatchRpcReply(const char *rpcName, const char *rpcData, size_t dataLen,
                          std::string &outReply);

    // Dispatch a GM command to the Lua delegate's on_gm_cmd(cmdline), 捕获字符串应答。
    // @return true 若 on_gm_cmd 存在且调用成功 (outReply 已填充, 可能为空串)
    bool DispatchGmCommand(const std::string &strCmd, std::string &outReply);

    // 灰度迁移: 调 Lua delegate 的 data_save(saveType) 落 Redis (2=迁移保存/3=删除清理)。
    // 仅在调度线程 (DispatchSync/消息循环) 内调用, 与业务 on_msg 串行, 不并发触碰 lua_State。
    bool CallDataSave(int32 i32SaveType);

    // 灰度迁移: 调 Lua delegate 的 data_load() 从 Redis 恢复内存态。兼容
    // data_load(actor_data) 签名, 以空串作参数传入。
    bool CallDataLoad();

    // 收到一条消息 (msgId + raw body)，转发到 Lua delegate 的 on_msg。
    virtual void OnMessage(uint32 u32MsgId, const std::string &strBody) override;

protected:
    // 框架系统消息 (高 16 位 == 0) 在 C++ 侧处理, 不进 Lua。
    virtual bool HandleSystemMessage(uint32 u32MsgId, const std::string &strBody) override;

private:
    // Dispatch a lifecycle method call to the Lua delegate.
    bool CallLuaMethod(const char *pszMethodName);

    lua_State *m_pL = nullptr;
    int32 m_iLuaDelegateRef = LUA_NOREF;
};
