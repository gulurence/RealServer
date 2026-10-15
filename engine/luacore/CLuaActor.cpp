#include "CLuaActor.h"
#include "LuaVM.h"
#include "xActor/ActorMsgId.h"
#include "xCluster/xActorId.h"
#include "xCluster/xProcessManager.h"
#include "xGm/GmCommandMgr.h"
#include "xLog/xLog.h"

#include <atomic>
#include <cstdio>

namespace
{
// 进程内 ActorIndex 自增计数器 (从 1 起), 与 machineId/osPid 组合铸全局唯一 ActorID。
// 所有 Actor 的唯一构造点都在 CLuaActor 构造函数 (原型经 si_lua_actor_create、实例经
// CLuaActor::Create), 各自铸一枚, 互不复用。
std::atomic<uint32> g_actorIndex{0};

uint64 MintGlobalActorId()
{
    const ProcessInfo &self = CProcessManager::getMe().GetSelfInfo();
    uint32 idx = g_actorIndex.fetch_add(1) + 1; // ActorIndex 从 1 起 (0 = 非法, 见 xActorId)
    return xActorId::MakeActorId(self.machineId, xActorId::GetOsPid(), idx);
}
} // namespace

CLuaActor::CLuaActor(EActorType i32ActorType, ActorID i64ActorID, const std::string &strActorName)
    : CActor(i32ActorType, i64ActorID, strActorName)
{
    // Get the global Lua state from the VM singleton
    m_pL = LuaVM::instance()->GetState();
    // 铸全局唯一 ActorID (展示/上报用; 业务键 m_i64ActorID 保持不动)
    SetGlobalId(MintGlobalActorId());
    XLOG("[INIT] CLuaActor created: type=%d id=%lld gid=%llu name=%s", i32ActorType,
         (int64)i64ActorID, (uint64)GetGlobalId(), strActorName.c_str());
}

CLuaActor::~CLuaActor()
{
    // 释放 Lua 委托引用, 避免 registry ref 泄漏 (实例经 DeleteActor delete 时触发;
    // 原型 id=0 进程生命周期内不删除, 此析构不会执行)。
    if (m_pL && m_iLuaDelegateRef != LUA_NOREF)
    {
        luaL_unref(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef);
        m_iLuaDelegateRef = LUA_NOREF;
    }
}

// -------------------------------------------------------------------
// Lifecycle Delegation
// -------------------------------------------------------------------

bool CLuaActor::Init()
{
    XLOG("[INIT] CLuaActor::Init type=%d id=%lld", GetActorType(), GetActorID());
    return CallLuaMethod("onInit");
}

bool CLuaActor::Install()
{
    XLOG("[INIT] CLuaActor::Install type=%d id=%lld name=%s", GetActorType(), GetActorID(),
         GetActorName().c_str());
    return CallLuaMethod("onInstall");
}

bool CLuaActor::UnInstall()
{
    XLOG("[INIT] CLuaActor::UnInstall type=%d id=%lld", GetActorType(), GetActorID());
    return CallLuaMethod("onUnInstall");
}

CActor *CLuaActor::Create(ActorID i64ActorID)
{
    // Clone this actor type with a new ActorID, and spawn a fresh Lua instance
    // bound as the clone's delegate. 这样每个实例都持有独立 Lua 状态
    // (Player/Battle/Spectator 等多实例 Actor 各自维护业务数据), 而不是共享
    // 原型的 delegate 表 —— 否则 RPC 转发会落到原型实例上。
    std::string instanceName = GetActorName() + "_" + std::to_string((int64)i64ActorID);
    auto *pNew = new CLuaActor(GetActorType(), i64ActorID, instanceName);
    if (!pNew->CreateIsEnable())
    {
        XERR("[INIT] CLuaActor::Create failed — create disabled for type=%d", GetActorType());
        delete pNew;
        return nullptr;
    }

    pNew->m_pL = m_pL;

    // 从原型 Lua 表取出 class 表, 用 class:new(type, id, name) 生成独立实例
    if (m_pL && m_iLuaDelegateRef != LUA_NOREF)
    {
        lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [proto]
        lua_getfield(m_pL, -1, "_class");                        // [proto, class]
        if (lua_istable(m_pL, -1))
        {
            lua_getfield(m_pL, -1, "new");                      // [proto, class, new]
            lua_pushvalue(m_pL, -2);                            // [proto, class, new, class]
            lua_pushinteger(m_pL, (lua_Integer)GetActorType()); // [.., class, type]
            lua_pushinteger(m_pL, (lua_Integer)i64ActorID);     // [.., class, type, id]
            lua_pushstring(m_pL, instanceName.c_str());         // [.., class, type, id, name]
            if (lua_pcall(m_pL, 4, 1, 0) == LUA_OK)
            { // [proto, class, instance]
                if (lua_istable(m_pL, -1))
                {
                    lua_pushvalue(m_pL, -1);
                    int32 ref = luaL_ref(m_pL, LUA_REGISTRYINDEX);
                    pNew->SetLuaDelegate(m_pL, ref);
                }
                lua_pop(m_pL, 1); // pop instance
            }
            else
            {
                XERR("[INIT] CLuaActor::Create spawn Lua instance failed: %s",
                     lua_tostring(m_pL, -1));
                lua_pop(m_pL, 1); // pop error
            }
            lua_pop(m_pL, 2); // pop class, proto
        }
        else
        {
            lua_pop(m_pL, 2); // pop class(nil), proto
        }
    }

    XLOG("[INIT] CLuaActor::Create cloned: type=%d new_id=%lld name=%s delegate=%s", GetActorType(),
         i64ActorID, instanceName.c_str(), pNew->m_iLuaDelegateRef == LUA_NOREF ? "none" : "bound");
    return pNew;
}

// -------------------------------------------------------------------
// Lua Delegate Binding
// -------------------------------------------------------------------

void CLuaActor::SetLuaDelegate(lua_State *L, int32 iLuaTableRef)
{
    m_pL = L;

    // Release previous delegate if one exists
    if (m_iLuaDelegateRef != LUA_NOREF)
    {
        luaL_unref(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef);
    }

    m_iLuaDelegateRef = iLuaTableRef;
}

// -------------------------------------------------------------------
// RPC Dispatch — gRPC → Lua actor
// -------------------------------------------------------------------

bool CLuaActor::DispatchRpc(const char *rpcName, const char *rpcData)
{
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF)
    {
        return true; // no Lua handler — no-op
    }

    // Push the delegate table from the registry
    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [table]

    // Get on_rpc_call method
    lua_getfield(m_pL, -1, "on_rpc_call"); // [table, method]

    if (!lua_isfunction(m_pL, -1))
    {
        // on_rpc_call not defined — default to success
        lua_pop(m_pL, 2);
        return true;
    }

    // Push self (delegate table)
    lua_pushvalue(m_pL, -2); // [table, method, self]

    // Push rpc_name (arg 1)
    lua_pushstring(m_pL, rpcName); // [table, method, self, rpcName]

    // Push rpc_data (arg 2)
    lua_pushstring(m_pL, rpcData); // [table, method, self, rpcName, rpcData]

    // Call on_rpc_call(self, rpcName, rpcData) — 3 args, 1 boolean result
    if (lua_pcall(m_pL, 3, 1, 0) != LUA_OK)
    {
        XERR("[CLuaActor:%s] Lua error in RPC dispatch '%s': %s", GetActorName().c_str(), rpcName,
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2);
        return false;
    }

    bool bResult = lua_toboolean(m_pL, -1);
    lua_pop(m_pL, 2);
    return bResult;
}

// -------------------------------------------------------------------
// RPC Dispatch — 带字符串响应 (二进制安全)
// -------------------------------------------------------------------

bool CLuaActor::DispatchRpcReply(const char *rpcName, const char *rpcData, size_t dataLen,
                                 std::string &outReply)
{
    outReply.clear();
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF)
    {
        return false;
    }

    // Push the delegate table from the registry
    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [table]

    // Get on_rpc_call method
    lua_getfield(m_pL, -1, "on_rpc_call"); // [table, method]

    if (!lua_isfunction(m_pL, -1))
    {
        lua_pop(m_pL, 2);
        return false;
    }

    lua_pushvalue(m_pL, -2);                 // [table, method, self]
    lua_pushstring(m_pL, rpcName);           // [.., rpcName]
    lua_pushlstring(m_pL, rpcData, dataLen); // [.., rpcData]

    // Call on_rpc_call(self, rpcName, rpcData) — 3 args, 1 string result
    if (lua_pcall(m_pL, 3, 1, 0) != LUA_OK)
    {
        XERR("[CLuaActor:%s] Lua error in RPC dispatch '%s': %s", GetActorName().c_str(), rpcName,
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2);
        return false;
    }

    // 捕获字符串响应 (二进制安全, 可能为空串)
    if (lua_isstring(m_pL, -1))
    {
        size_t len = 0;
        const char *s = lua_tolstring(m_pL, -1, &len);
        outReply.assign(s, len);
    }
    lua_pop(m_pL, 2);
    return true;
}

// -------------------------------------------------------------------
// GM Command Dispatch — 调度线程串行调用 Lua on_gm_cmd
// -------------------------------------------------------------------

bool CLuaActor::DispatchGmCommand(const std::string &strCmd, std::string &outReply)
{
    outReply.clear();
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF)
    {
        return false;
    }

    // Push the delegate table from the registry
    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [table]

    // Get on_gm_cmd method
    lua_getfield(m_pL, -1, "on_gm_cmd"); // [table, method]

    if (!lua_isfunction(m_pL, -1))
    {
        lua_pop(m_pL, 2);
        return false;
    }

    lua_pushvalue(m_pL, -2);                             // [table, method, self]
    lua_pushlstring(m_pL, strCmd.data(), strCmd.size()); // [.., cmdline]

    // Call on_gm_cmd(self, cmdline) — 2 args, 1 string result
    if (lua_pcall(m_pL, 2, 1, 0) != LUA_OK)
    {
        XERR("[CLuaActor:%s] Lua error in GM command '%s': %s", GetActorName().c_str(),
             strCmd.c_str(), lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2);
        return false;
    }

    if (lua_isstring(m_pL, -1))
    {
        size_t len = 0;
        const char *s = lua_tolstring(m_pL, -1, &len);
        outReply.assign(s, len);
    }
    lua_pop(m_pL, 2);
    return true;
}

// -------------------------------------------------------------------
// Message Dispatch — 协程消息循环 → Lua actor on_msg
// -------------------------------------------------------------------

void CLuaActor::OnMessage(uint32 u32MsgId, const std::string &strBody)
{
    // 框架系统消息 (高 16 位 == 0) 由 C++ 处理, 不转发 Lua
    if (xMsgId::IsSystemMsg(u32MsgId))
    {
        if (!HandleSystemMessage(u32MsgId, strBody))
        {
            XWRN("[CLuaActor:%s] unhandled system msg id=%u", GetActorName().c_str(), u32MsgId);
        }
        return;
    }

    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF)
    {
        return; // 无 Lua 委托 — no-op
    }

    // 从注册表取出 delegate 表
    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [table]

    // 取 on_msg 方法
    lua_getfield(m_pL, -1, "on_msg"); // [table, method]

    if (!lua_isfunction(m_pL, -1))
    {
        // 未定义 on_msg — 忽略
        lua_pop(m_pL, 2);
        return;
    }

    // 压入 self + msgId + body
    lua_pushvalue(m_pL, -2);                               // [table, method, self]
    lua_pushinteger(m_pL, (lua_Integer)u32MsgId);          // [.., msgId]
    lua_pushlstring(m_pL, strBody.data(), strBody.size()); // [.., body]

    // 调用 on_msg(self, msgId, body) — 3 参数，0 返回值
    if (lua_pcall(m_pL, 3, 0, 0) != LUA_OK)
    {
        XERR("[CLuaActor:%s] Lua error in on_msg: %s", GetActorName().c_str(),
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2); // 弹错误 + table
        return;
    }
    lua_pop(m_pL, 1); // 弹 table
}

// -------------------------------------------------------------------
// System Message Dispatch — 框架系统消息在 C++ 侧处理
// -------------------------------------------------------------------

bool CLuaActor::HandleSystemMessage(uint32 u32MsgId, const std::string &strBody)
{
    switch (u32MsgId)
    {
    case xMsgId::SYS_PING:
        XDBG("[CLuaActor:%s] SYS_PING", GetActorName().c_str());
        return true;
    case xMsgId::SYS_PONG:
        XDBG("[CLuaActor:%s] SYS_PONG", GetActorName().c_str());
        return true;
    case xMsgId::SYS_DISCONNECT:
    {
        // 客户端断线: 投递到本 Actor 的 Lua (on_rpc_call("disconnect", cidBody)),
        // 由 Gate 的 on_rpc_disconnect 统一收尾 (连接映射清理 / 房间按离开处理 /
        // Player Actor 状态置离线)。与 SYS_RPC_PUSH 同理, 在调度线程内串行执行,
        // 避免与 on_msg 并发触碰进程唯一 lua_State。
        std::string reply;
        return DispatchRpcReply("disconnect", strBody.data(), strBody.size(), reply);
    }
    case xMsgId::SYS_RPC_PUSH:
    {
        // 服务器主动推送 (RPC_METHOD_PUSH handler 投递): 在此 —— 本 Actor 的
        // 调度线程内 —— 串行执行 on_rpc_call("push", body)。与 on_msg 同线程,
        // 避免并发触碰进程唯一 lua_State (RPC 服务线程不得直接调 Lua)。
        std::string reply;
        return DispatchRpcReply("push", strBody.data(), strBody.size(), reply);
    }
    case xMsgId::SYS_GM_CMD:
    {
        // GM 命令 (CGmCommandMgr 投递): 在调度线程内串行调用 Lua on_gm_cmd,
        // 应答经 CGmCommandMgr::LogReply 回写 Redis 日志列表 (供 WebPlatform 读)。
        std::string reply;
        bool handled = DispatchGmCommand(strBody, reply);
        char tag[96] = {0};
        snprintf(tag, sizeof(tag), "actor type=%d id=%lld: %s", (int32)GetActorType(),
                 (int64)GetActorID(), handled ? reply.c_str() : "(no on_gm_cmd handler)");
        CGmCommandMgr::instance()->LogReply(tag);
        return true;
    }
    default:
        return false;
    }
}

// -------------------------------------------------------------------
// Lua Method Dispatch
// -------------------------------------------------------------------

bool CLuaActor::CallLuaMethod(const char *pszMethodName)
{
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF)
    {
        // No Lua handler set — treat as success (no-op)
        return true;
    }

    // Push the delegate table from the registry
    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [table]

    // Get the named method from the table
    lua_getfield(m_pL, -1, pszMethodName); // [table, method]

    if (!lua_isfunction(m_pL, -1))
    {
        // Method not defined in Lua — default to success
        lua_pop(m_pL, 2); // pop method and table
        return true;
    }

    // Push the table as self (the first argument to the method call)
    lua_pushvalue(m_pL, -2); // [table, method, table(self)]

    // Call method with 1 argument (self), expect 1 boolean result
    if (lua_pcall(m_pL, 1, 1, 0) != LUA_OK)
    {
        XERR("[CLuaActor:%s] Lua error in '%s': %s", GetActorName().c_str(), pszMethodName,
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2); // pop error and table
        return false;
    }

    // Get the boolean result
    bool bResult = lua_toboolean(m_pL, -1);
    lua_pop(m_pL, 2); // pop result and table
    return bResult;
}

// -------------------------------------------------------------------
// 灰度迁移 — 数据落盘/恢复 (灰度更新「先迁后杀」用)
// -------------------------------------------------------------------
// Player/Battle 经 RPC 路径创建 (ActorCreator::Create → CLuaActor::Create),
// 其权威运行态都挂在 delegate 表 (instance B) 上, 不在 Lua localInstances 里。
// 迁移保存/恢复必须经 C++ CActorMgr 定位 delegate 后调用 data_save/data_load,
// 与业务 on_msg 同线程串行执行 (由迁移脚本经 DispatchSync 进入调度线程)。

bool CLuaActor::CallDataSave(int32 i32SaveType)
{
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF)
    {
        return true; // 无 Lua 委托 — 无状态可保存, 视为成功
    }

    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [table]
    lua_getfield(m_pL, -1, "data_save");                     // [table, method]

    if (!lua_isfunction(m_pL, -1))
    {
        lua_pop(m_pL, 2); // pop method, table
        return true;      // 未定义 data_save — 视为成功
    }

    lua_pushvalue(m_pL, -2);                         // [table, method, self]
    lua_pushinteger(m_pL, (lua_Integer)i32SaveType); // [.., saveType]

    // 调 data_save(self, saveType) — 2 参数, 1 布尔结果
    if (lua_pcall(m_pL, 2, 1, 0) != LUA_OK)
    {
        XERR("[CLuaActor:%s] Lua error in data_save: %s", GetActorName().c_str(),
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2); // pop error, table
        return false;
    }

    bool bResult = lua_toboolean(m_pL, -1);
    lua_pop(m_pL, 2); // pop result, table
    return bResult;
}

bool CLuaActor::CallDataLoad()
{
    if (!m_pL || m_iLuaDelegateRef == LUA_NOREF)
    {
        return true; // 无 Lua 委托 — 无状态可恢复, 视为成功
    }

    lua_rawgeti(m_pL, LUA_REGISTRYINDEX, m_iLuaDelegateRef); // [table]
    lua_getfield(m_pL, -1, "data_load");                     // [table, method]

    if (!lua_isfunction(m_pL, -1))
    {
        lua_pop(m_pL, 2); // pop method, table
        return true;      // 未定义 data_load — 视为成功
    }

    lua_pushvalue(m_pL, -2);   // [table, method, self]
    lua_pushstring(m_pL, "");  // [.., actor_data] (兼容 data_load(actor_data) 签名)

    // 调 data_load(self, "") — 2 参数, 1 布尔结果
    if (lua_pcall(m_pL, 2, 1, 0) != LUA_OK)
    {
        XERR("[CLuaActor:%s] Lua error in data_load: %s", GetActorName().c_str(),
             lua_tostring(m_pL, -1));
        lua_pop(m_pL, 2); // pop error, table
        return false;
    }

    bool bResult = lua_toboolean(m_pL, -1);
    lua_pop(m_pL, 2); // pop result, table
    return bResult;
}
