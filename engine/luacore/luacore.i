// luacore.i — Main SWIG interface for RealServer Lua bindings
//
// Naming convention: all C++→Lua exported functions use the si_ prefix.
// Lua scripts access these as: luacore.si_log_info(), luacore.si_actor_create(), etc.
//
// Export categories:
//   1. Log functions           — si_log_*
//   2. Time/Timer utilities    — si_time_*, si_timer_*
//   3. String tools            — si_str_*
//   4. General tools           — si_rand_*, si_select_*
//   5. Unique ID manager       — si_uid_*
//   6. Actor common data       — si_get_*, si_set_*
//   7. Actor registration      — si_actor_*
//   8. ActorMgr instance mgmt  — si_actor_mgr_*
//   9. Redis/DB/RPC stubs      — si_redis_*, si_db_*, si_rpc_*

%module luacore

%include <typemaps.i>
%include <std_string.i>
%include <stdint.i>

// ================================================================
// Real C++ headers (SWIG does NOT parse these)
// MUST come before %include "base/xDefine.i" so that int32/int64
// are #included before the toint32/toint64 %inline functions.
// ================================================================
%{
#include "xBase/xDefine.h"
#include "xBase/xTime.h"
#include "xBase/xStringTool.h"
#include "xBase/xTools.h"
#include "xBase/xUniqueIDManager.h"
#include "xActor/Actor.h"
#include "xActor/ActorCreator.h"
#include "xActor/ActorMgr.h"
#include "xActor/ActorScheduler.h"
#include "luacore/CLuaActor.h"
#include "xRedis/RedisMgr.h"
#include "xDatabase/xOperatorMgr.h"
#include "xRecord/xRecordMgr.h"
#include "xRecord/xDatabaseRecord.h"
#include "xRpc/CRpcTcpClient.h"
#include "xRpc/RpcFrame.h"
#include "xCluster/xLinkManager.h"
#include "xCluster/CActorLoadManager.h"
#include "xGm/GmCommandMgr.h"
#include "rpc/actor_mgr.pb.h"
#include "luacore/LuaVM.h"
#include "xNet/CNetGateway.h"
#include "msg/login.pb.h"
#include "msg/account.pb.h"
#include "msg/msg_id.pb.h"
#include <google/protobuf/io/coded_stream.h>
#include <vector>
#include <map>
#include <cstdint>
#include <cstdlib>
%}

// SWIG 辅助: 跳过 protobuf 字段 (供通用 pb 读取函数使用)
// 置于 %{ %} 逐字块内, 直接拷入生成 .cxx, 供下方 %inline 编解码函数调用。
%{
// 跳过 CodedInputStream 当前 tag 对应的字段 (proto3 wire 格式, 兼容新版 protobuf
// 的 ReadTag()/Skip(int) API, 替代已移除的 SkipField()).
static bool pb_skip_field(google::protobuf::io::CodedInputStream& in, uint32_t tag) {
    switch (tag & 7) {
        case 0: { uint64_t v = 0; return in.ReadVarint64(&v); }                 // VARINT
        case 1: return in.Skip(8);                                              // FIXED64
        case 2: { uint32_t len = 0; return in.ReadVarint32(&len) && in.Skip((int)len); } // LEN
        case 5: return in.Skip(4);                                              // FIXED32
        default: return false;                                                  // GROUP/未知
    }
}
%}

// ================================================================
// Base type aliases, typemaps, and conversion helpers
// (from base/xDefine.i — typemaps must be applied before any
//  %inline blocks below that use int32/int64/EActorType/etc.)
// ================================================================
%include "base/xDefine.i"

// ================================================================
// 1. Log functions — si_log_*
// Lua: luacore.si_log_info("msg")
// ================================================================
%inline %{
void si_log_trace(const char* msg) { XTRC("[Lua] %s", msg); }
void si_log_debug(const char* msg) { XDBG("[Lua] %s", msg); }
void si_log_info(const char* msg)  { XLOG("[Lua] %s", msg); }
void si_log_warn(const char* msg)  { XWRN("[Lua] %s", msg); }
void si_log_error(const char* msg) { XERR("[Lua] %s", msg); }
%}

// ================================================================
// 2. Time utilities — si_time_*, si_timer_*
// ================================================================
%inline %{
int64 si_time_cur_sec()   { return (int64)xTime::getCurSec(); }
int64 si_time_cur_usec()  { return (int64)xTime::getCurUSec(); }
int64 si_time_day_start(int64 t)   { return (int64)xTime::getDayStart((time_t)t); }
bool  si_time_is_same_day(int64 a, int64 b) { return xTime::isSameDay((time_t)a, (time_t)b); }
bool  si_time_is_leap_year(int32 year) { return xTime::isLeapYear(year); }
%}

// si_timer_wrap — second-resolution timer
%nodefaultctor si_timer_wrap;
%inline %{
struct si_timer_wrap {
    xTimer m_t;
    si_timer_wrap(uint32 threshold) : m_t(threshold) {}
    bool timeUp() { return m_t.timeUp(); }
    void reset()  { m_t.reset(); }
};
%}

// si_milli_timer_wrap — millisecond-resolution timer
%nodefaultctor si_milli_timer_wrap;
%inline %{
struct si_milli_timer_wrap {
    xMilliTimer m_t;
    si_milli_timer_wrap(uint32 threshold) : m_t(threshold) {}
    bool timeUp() { return m_t.timeUp(); }
    void reset()  { m_t.reset(); }
};
%}

// ================================================================
// 3. StringTool wrappers — si_str_*
// ================================================================
%inline %{
std::string si_str_trim(const std::string& s)      { return StringTool::trim(s); }
std::string si_str_trimbegin(const std::string& s) { return StringTool::trimbegin(s); }
std::string si_str_trimend(const std::string& s)   { return StringTool::trimend(s); }
bool si_str_contains(const std::string& s, const std::string& sub) { return StringTool::contains(s, sub); }
bool si_str_startswith(const std::string& s, const std::string& pre) { return StringTool::startswith(s, pre); }
bool si_str_endswith(const std::string& s, const std::string& suf)   { return StringTool::endswith(s, suf); }
std::string si_str_tolower(const std::string& s)   { return StringTool::tolower(s); }
std::string si_str_toupper(const std::string& s)   { return StringTool::toupper(s); }
std::string si_str_replace(const std::string& s, const std::string& o, const std::string& r) {
    std::string result(s); StringTool::replace(result, o, r); return result;
}
%}

// ================================================================
// 4. xTools wrappers — si_rand_*, si_select_*
// ================================================================
%inline %{
int32  si_rand_between(int32 min, int32 max)  { return ::randBetween(min, max); }
bool   si_select_by_percent(int32 pct)         { return ::selectByPercent(pct); }
bool   si_select_by_thousand(int32 thousand)    { return ::selectByThousand(thousand); }
std::string si_get_readable_ip(uint32 ip)       { return ::getReadableIP(ip); }
%}

// ================================================================
// 5. UniqueIDManager — si_uid_*
// ================================================================
%nodefaultctor si_uid32_mgr;
%inline %{
struct si_uid32_mgr {
    UniqueIDManager<uint32> m_mgr;
    si_uid32_mgr(uint32 minVal, uint32 maxVal) : m_mgr(minVal, maxVal) {}
    uint32 getUniqueID() { uint32 v; m_mgr.getUniqueID(v); return v; }
    void   putUniqueID(uint32 val) { m_mgr.putUniqueID(val); }
};
%}

// ================================================================
// 6. Actor common data — si_get_*, si_set_*
// Lua: local state = luacore.si_get_state(actorPtr)
// ================================================================
%inline %{
// Opaque pointer type for actors. Lua only passes pointers, never inspects.
typedef void* ActorPtr;

// --- Actor 通用数据 Getter ---
int32 si_get_type(ActorPtr actor)       { return ((CActor*)actor)->GetActorType(); }
int64 si_get_id(ActorPtr actor)         { return ((CActor*)actor)->GetActorID(); }
int32 si_get_load_score(ActorPtr actor) { return ((CActor*)actor)->GetLoadScore(); }
int64 si_get_create_time(ActorPtr actor){ return ((CActor*)actor)->GetCreateTime(); }
int32 si_get_state(ActorPtr actor)      { return ((CActor*)actor)->GetState(); }
int64 si_get_owner_id(ActorPtr actor)   { return ((CActor*)actor)->GetOwnerID(); }

// --- Actor 通用数据 Setter ---
void si_set_load_score(ActorPtr actor, int32 v)  { ((CActor*)actor)->SetLoadScore(v); }
void si_set_create_time(ActorPtr actor, int64 v) { ((CActor*)actor)->SetCreateTime(v); }
void si_set_state(ActorPtr actor, int32 v)       { ((CActor*)actor)->SetState(v); }
void si_set_owner_id(ActorPtr actor, int64 v)    { ((CActor*)actor)->SetOwnerID(v); }

// --- Actor 帧级脏读写状态框架 (P7) ---
// 业务脚本用 si_set_state_field / si_get_state_field 做脏读写; 框架按心跳
// FlushState 落盘到 actor:state:{type}:{id}, 下线/宕机后 Install 时 LoadState 恢复。
void si_set_state_field(ActorPtr actor, const std::string& key, const std::string& value) {
    if (!actor) return;
    ((CActor*)actor)->SetStateField(key, value);
}
std::string si_get_state_field(ActorPtr actor, const std::string& key) {
    if (!actor) return "";
    std::string out;
    if (((CActor*)actor)->GetStateField(key, out)) return out;
    return "";
}
bool si_flush_state(ActorPtr actor) {
    if (!actor) return false;
    return ((CActor*)actor)->FlushState(CGmCommandMgr::instance()->GetCli());
}
bool si_load_state(ActorPtr actor) {
    if (!actor) return false;
    return ((CActor*)actor)->LoadState(CGmCommandMgr::instance()->GetCli());
}

// --- Actor 监控上报扩展 (业务在调度线程写入, 心跳线程 Sweep 读) ---
// BattleActor 用 si_battlecore_report_json 生成本场核心状态 JSON, 经此缓存到 CActor;
// CActor::BuildReportJson 把它作为 "battle" 子对象拼入 cluster:actor:report:{type}:{id}。
void si_actor_set_report_extra(int32 actorType, int64 actorID, const std::string& json) {
    CActor* actor = CActorMgr::instance()->Find((EActorType)actorType, actorID);
    if (actor) actor->SetReportExtraJson(json);
}
%}

// ================================================================
// 7. Actor registration & creation — si_actor_*
// ================================================================
%inline %{
// Register an actor prototype with ActorCreator
bool si_actor_regist(int32 actorType, ActorPtr actor) {
    return ActorCreator::RegistActor((CActor*)actor);
}

// Create an actor instance from prototype
ActorPtr si_actor_create(int32 actorType, int64 actorID) {
    return (ActorPtr)ActorCreator::Create((EActorType)actorType, actorID);
}

// Create a CLuaActor — the C++ to Lua bridge for actor business logic.
// 若 _G.__actor_delegate 已设置 (Lua 表), 则 luaL_ref 并绑定为业务委托,
// 使 CLuaActor::OnMessage/CallLuaMethod 能回调到该表的 on_msg/onInit 等。
ActorPtr si_lua_actor_create(int32 actorType, int64 actorID, const char* actorName) {
    CLuaActor* pActor = new CLuaActor((EActorType)actorType, actorID, std::string(actorName));

    lua_State* L = LuaVM::instance()->GetState();
    if (L) {
        lua_getglobal(L, "__actor_delegate");           // [delegate]
        if (lua_istable(L, -1)) {
            lua_pushvalue(L, -1);                        // [delegate, delegate]
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);    // ref 持有 delegate 副本
            pActor->SetLuaDelegate(L, ref);
            lua_pop(L, 1);                               // 弹出 global 原始值
        } else {
            lua_pop(L, 1);                               // 弹出 nil/非表
        }
    }
    return (ActorPtr)pActor;
}

// Dispatch an RPC call to a Lua actor.
// actorPtr  — target actor (opaque ActorPtr)
// rpcName   — first gRPC param, used to locate on_rpc_<name> handler in Lua
// rpcData   — second gRPC param, Lua table data transparently passed
bool si_actor_dispatch_rpc(ActorPtr actorPtr, const char* rpcName, const char* rpcData) {
    if (!actorPtr) return false;
    return ((CLuaActor*)actorPtr)->DispatchRpc(rpcName, rpcData);
}

// 向 Actor 投递一条消息 (进入 Actor 消息队列，由协程消息循环消费)
bool si_actor_post_message(ActorPtr actor, int32 msgId, const std::string& body) {
    if (!actor) return false;
    ((CActor*)actor)->PostMessage((uint32)msgId, body);
    return true;
}
%}

// ================================================================
// 8. ActorMgr instance management — si_actor_mgr_*
// ================================================================
%inline %{
// Declare that this process can host actors of the given type.
void si_actor_mgr_add_hosted_type(int32 actorType) {
    CActorMgr::instance()->AddHostedType((EActorType)actorType);
}

// Check whether this process hosts the given actor type.
bool si_actor_mgr_can_host_type(int32 actorType) {
    return CActorMgr::instance()->CanHostType((EActorType)actorType);
}

// Add an already-created actor instance to the manager.
bool si_actor_mgr_add_instance(ActorPtr actor) {
    return CActorMgr::instance()->AddInstance((CActor*)actor);
}

// Remove an actor instance from the manager (does NOT delete).
ActorPtr si_actor_mgr_remove_instance(int32 actorType, int64 actorID) {
    return (ActorPtr)CActorMgr::instance()->RemoveInstance((EActorType)actorType, actorID);
}

// Install the actor instance (with connection ref counting).
bool si_actor_mgr_install(int32 actorType, int64 actorID) {
    return CActorMgr::instance()->Install((EActorType)actorType, actorID);
}

// Uninstall the actor instance (with connection ref counting).
bool si_actor_mgr_uninstall(int32 actorType, int64 actorID) {
    return CActorMgr::instance()->UnInstall((EActorType)actorType, actorID);
}

// 本地卸载 + 延迟释放: UnInstall 移除注册表后, 投递停止信号并延迟 delete。
// 供 Actor 在自己消息循环内卸载自身 (BattleActor 的 BATTLE_FINALIZE) 时使用,
// 避免在消息处理器返回前 delete this 造成 use-after-free; delete 与 ~CLuaActor
// 的 luaL_unref 同在调度线程, 不并发触碰进程唯一 lua_State。
bool si_actor_delete_local(int32 actorType, int64 actorID) {
    CActor *pActor = CActorMgr::instance()->Find((EActorType)actorType, actorID);
    if (!pActor) return false;
    if (!CActorMgr::instance()->UnInstall((EActorType)actorType, actorID)) return false;
    if (actorID != 0) {
        pActor->RequestStop();   // 停止信号 → 消息循环 break → 协程帧自毁 (先于 delete)
        asio::post(CActorScheduler::instance()->GetContext(), [pActor] { delete pActor; });
    }
    return true;
}

// Find an actor instance by type and id (local only).
ActorPtr si_actor_mgr_find(int32 actorType, int64 actorID) {
    return (ActorPtr)CActorMgr::instance()->Find((EActorType)actorType, actorID);
}

// 延迟调度: delayMs 毫秒后向 actor(type,id) 投递 msgId (BATTLE_FINALIZE 延迟回收等)。
// 目标 actor 必须在本地进程 (BattleActor 在 Game 进程本地); 非本地则到点 Find 不到, 静默丢弃。
void si_schedule_after(int32 actorType, int64 actorID, int32 delayMs, int32 msgId, const std::string& body) {
    CActorScheduler::instance()->ScheduleAfter(actorType, actorID, (uint32)delayMs, (uint32)msgId, body);
}

// Get the total count of managed actor instances.
int32 si_actor_mgr_get_instance_count() {
    return (int32)CActorMgr::instance()->GetInstanceCount();
}
%}

// ================================================================
// 9. ActorCreator — global RPC interfaces (si_actor_creator_*)
// Used by xEngine.ActorCreator (Lua routing layer) for cross-process
// actor create/delete/query and load balancer integration.
// ================================================================
%inline %{
// Create an actor instance locally (called by RPC handler on target process).
ActorPtr si_actor_creator_create(int32 actorType, int64 actorID) {
    return (ActorPtr)ActorCreator::Create((EActorType)actorType, actorID);
}

// Delete an actor instance locally.
bool si_actor_creator_delete(int32 actorType, int64 actorID) {
    return ActorCreator::DeleteActor((EActorType)actorType, actorID);
}

// Get this process's total actor count (used as load score).
int32 si_actor_creator_get_process_load() {
    return ActorCreator::GetLocalInstanceCount();
}

// Get count of actors of a specific type on this process.
int32 si_actor_creator_get_local_count(int32 actorType) {
    return ActorCreator::GetLocalTypeCount((EActorType)actorType);
}
%}

// ================================================================
// 10. Redis / DB / Record — si_redis_*, si_db_*, si_record_*, si_rpc_*
//
// Redis: synchronous key-value (CRedisPoolMgr → CRedisCli), pool "actor_cache".
// DB:    async queue via COperatorMgr (fire-and-forget for writes).
// Record: CRecordMgr bridge for Load/Update/Remove actor data.
// ================================================================
%inline %{
// --- Redis Get/Set/Del (synchronous, pool "actor_cache") ---

std::string si_redis_get(const std::string& key) {
    auto* pCli = CRedisPoolMgr::getMe().GetRedisCli("actor_cache");
    if (!pCli) {
        XERR("[si_redis_get] no Redis client for pool 'actor_cache'");
        return "";
    }
    RecordDataST data(1, "");  // minimal init
    data.Resize(MAX_BINDATA_SIZE);
    if (pCli->Get(key, &data) != REDIS_OK) {
        return "";
    }
    if (data.Size() <= 0) {
        return "";
    }
    return std::string(data.Data(), data.Size());
}

bool si_redis_set(const std::string& key, const std::string& value) {
    auto* pCli = CRedisPoolMgr::getMe().GetRedisCli("actor_cache");
    if (!pCli) {
        XERR("[si_redis_set] no Redis client for pool 'actor_cache'");
        return false;
    }
    RecordDataST data((int32)value.size(), value.data());
    return pCli->Set(key, &data) == REDIS_OK;
}

bool si_redis_del(const std::string& key) {
    auto* pCli = CRedisPoolMgr::getMe().GetRedisCli("actor_cache");
    if (!pCli) {
        XERR("[si_redis_del] no Redis client for pool 'actor_cache'");
        return false;
    }
    return pCli->Del(key.c_str()) == REDIS_OK;
}

// ZADD — 有序集合插入 (战斗战报索引 battle:index 用)
bool si_redis_zadd(const std::string& key, int64 score, const std::string& member) {
    auto* pCli = CRedisPoolMgr::getMe().GetRedisCli("actor_cache");
    if (!pCli) {
        XERR("[si_redis_zadd] no Redis client for pool 'actor_cache'");
        return false;
    }
    return pCli->ZAdd(key, score, member) == REDIS_OK;
}

// --- DB operations (async fire-and-forget via COperatorMgr) ---

// Simple column descriptor for generic actor data tables.
// Each actor type maps to a MySQL table "actor_<type>" with columns:
//   id   BIGINT PRIMARY KEY
//   data BLOB
static dbCol s_si_dbCols[] = {
    { "id",   DBDATA_TYPE_UInt64, 8 },
    { "data", DBDATA_TYPE_BIN,    0 },
    { NULL,   0,                  0 }
};

std::string si_db_select(const std::string& table, const std::string& where) {
    // DB SELECT is async; return empty immediately.
    // Results arrive via the DB operator callback (not exposed to Lua yet).
    auto* pNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pNode) {
        XERR("[si_db_select] no available operator node");
        return "";
    }
    pNode->m_stTitle     = "actor_db";
    pNode->m_enOpType    = ODOT_SELECT;
    pNode->m_strTableName = table;
    pNode->m_strWhere    = where;
    pNode->m_pCol        = s_si_dbCols;
    COperatorMgr::getMe().PushOperator(pNode);
    XLOG("[si_db_select] queued table=%s where=%s", table.c_str(), where.c_str());
    return "";
}

bool si_db_insert(const std::string& table, const std::string& data) {
    auto* pNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pNode) {
        XERR("[si_db_insert] no available operator node");
        return false;
    }
    pNode->m_stTitle     = "actor_db";
    pNode->m_enOpType    = ODOT_INSERT;
    pNode->m_strTableName = table;
    pNode->m_pCol        = s_si_dbCols;
    pNode->m_stData.Init(
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data())),
        (unsigned short)data.size());
    COperatorMgr::getMe().PushOperator(pNode);
    return true;
}

int32 si_db_update(const std::string& table, const std::string& data, const std::string& where) {
    auto* pNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pNode) {
        XERR("[si_db_update] no available operator node");
        return 0;
    }
    pNode->m_stTitle     = "actor_db";
    pNode->m_enOpType    = ODOT_UPDATE;
    pNode->m_strTableName = table;
    pNode->m_strWhere    = where;
    pNode->m_pCol        = s_si_dbCols;
    pNode->m_stData.Init(
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data())),
        (unsigned short)data.size());
    COperatorMgr::getMe().PushOperator(pNode);
    return 0;  // async — affected rows not known yet
}

int32 si_db_delete(const std::string& table, const std::string& where) {
    auto* pNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pNode) {
        XERR("[si_db_delete] no available operator node");
        return 0;
    }
    pNode->m_stTitle     = "actor_db";
    pNode->m_enOpType    = ODOT_DELETE;
    pNode->m_strTableName = table;
    pNode->m_strWhere    = where;
    COperatorMgr::getMe().PushOperator(pNode);
    return 0;
}

// --- Record notification (bridge to CRecordMgr for async DB persistence) ---

// Notify the record manager to persist actor data asynchronously.
// saveType: 1=insert, 2=update, 3=delete
bool si_record_notify(int32 actorType, int64 actorID, int32 saveType) {
    // Create a minimal RecordDataST for the CRecordMgr call.
    // CRecordMgr uses m_i32Type and m_i64Id for routing.
    RecordDataST recordData(1, "");  // 1-byte dummy allocation (safe memcpy)
    recordData.m_i32Type = actorType;
    recordData.m_i64Id   = actorID;

    if (saveType == 3) {
        return CRecordMgr::getMe().RemoveActor(&recordData);
    } else {
        return CRecordMgr::getMe().UpdateActor(&recordData);
    }
}

// RPC: Send a message to an actor (local or remote via gRPC).
bool si_rpc_send(int64 targetActorID, const std::string& msgData) {
    // TODO: integrate with engine RPC/gRPC subsystem
    XLOG("[si_rpc_send] target=%lld (stub)", targetActorID);
    return true;
}

// ================================================================
// 11. ActorCreator 自建 asio-TCP RPC — si_actor_creator_rpc_*
// 走统一 Actor 负载管理器 CActorLoadManager (均衡负载挑进程 + 自建 RPC),
// gRPC 退役。供 Lua xEngine.ActorCreator 跨进程创建/查询/卸载 Actor。
// ================================================================

// RPC CreateActor — 统一 Actor 负载管理器挑最低负载承载进程, 远程创建 Actor
bool si_actor_creator_rpc_create(int32 actorType, int64 actorID) {
    return CActorLoadManager::instance()->CreateActorRemote((EActorType)actorType, actorID);
}

// RPC FindActor — 查询承载进程是否存在该 Actor
// outFound/outRemoteType 始终被赋值
bool si_actor_creator_rpc_find(int32 actorType, int64 actorID,
                               bool* outFound, int32* outRemoteType) {
    return CActorLoadManager::instance()->FindActorRemote(
        (EActorType)actorType, actorID, outFound, outRemoteType);
}

// RPC UninstallActor — 统一 Actor 负载管理器挑承载进程, 远程卸载 Actor
bool si_actor_creator_rpc_uninstall(int32 actorType, int64 actorID) {
    return CActorLoadManager::instance()->UninstallActorRemote((EActorType)actorType, actorID);
}

// RPC LoginForward — 挑 Login 进程转发登录/选角 (op=1 登录, op=2 选角)
// 返回 Login 进程生成的客户端回包 protobuf (失败/超时返回空串)
std::string si_login_rpc(int32 op, const std::string& payload) {
    return CActorLoadManager::instance()->LoginRemote(op, payload);
}
%}

// ================================================================
// 12. 客户端网络发送通道 + 帧解包 — si_net_* (Gate 进程)
//
// 客户端帧: [uint32 len][protobuf body]
// CNetGateway 路由时把 CID 以小端 8 字节前置到 body 投给 Gate Actor 的
// on_msg(self, msgId, body), 故 body = [8 字节 CID][protobuf body]。
// 回包时用 si_net_body_cid 取出 CID, si_net_send 把 protobuf 字节回发给客户端。
// ================================================================
%inline %{
// 向客户端连接发送一帧 (自动加 [uint32 len] 前缀)
bool si_net_send(int64 cid, const std::string& bytes) {
    return CNetGateway::instance()->SendToCID((NetCID)cid, bytes);
}

// 从 on_msg 收到的 body 取连接 CID (前 8 字节, 小端)
int64 si_net_body_cid(const std::string& body) {
    if (body.size() < 8) return 0;
    const unsigned char* p = (const unsigned char*)body.data();
    int64 cid = 0;
    for (int i = 0; i < 8; ++i) cid |= ((int64)p[i]) << (8 * i);
    return cid;
}

// 从 on_msg 收到的 body 取 protobuf 负载 (8 字节之后)
std::string si_net_body_payload(const std::string& body) {
    if (body.size() <= 8) return std::string();
    return body.substr(8);
}

// 组装 [8 字节小端 id][payload] 帧体 (与 si_net_body_cid/si_net_body_payload 对称)
// 用途: Gate 向业务 Actor 转发时把发送方 player_id 前置, 目标 Actor 用
// si_net_body_cid/si_net_body_payload 还原发送方身份与 protobuf 负载。
std::string si_net_body_wrap(int64 id, const std::string& payload) {
    std::string out;
    out.reserve(8 + payload.size());
    for (int i = 0; i < 8; ++i) out.push_back((char)((id >> (8 * i)) & 0xFF));
    out.append(payload);
    return out;
}
%}

// ================================================================
// 13. protobuf 编解码 — si_pb_* (登录链路)
// 解码 PBMsgLoginReq / PBMsgSelectRoleReq 的字段, 编码 PBMsgLoginRsp / PBMsgSelectRoleRsp。
// Lua 侧拿到的 body 已是去掉 CID 前缀后的 protobuf 负载。
// ================================================================
%inline %{
// --- PBMsgLoginReq 解码 (token=field3, account_id=field4) ---
std::string si_pb_login_req_token(const std::string& body) {
    PbLogin::PBMsgLoginReq req;
    if (!req.ParseFromString(body)) return "";
    return req.token();
}
int64 si_pb_login_req_account_id(const std::string& body) {
    PbLogin::PBMsgLoginReq req;
    if (!req.ParseFromString(body)) return 0;
    return req.account_id();
}

// --- PBMsgSelectRoleReq 解码 (account_id=field3, role_id=field4) ---
int64 si_pb_select_role_account_id(const std::string& body) {
    PbLogin::PBMsgSelectRoleReq req;
    if (!req.ParseFromString(body)) return 0;
    return req.account_id();
}
int64 si_pb_select_role_role_id(const std::string& body) {
    PbLogin::PBMsgSelectRoleReq req;
    if (!req.ParseFromString(body)) return 0;
    return req.role_id();
}

// --- PBMsgCreateRoleReq 解码 (account_id=field3, role_name=field4, job=field5) ---
int64 si_pb_create_role_account_id(const std::string& body) {
    PbLogin::PBMsgCreateRoleReq req;
    if (!req.ParseFromString(body)) return 0;
    return req.account_id();
}
std::string si_pb_create_role_role_name(const std::string& body) {
    PbLogin::PBMsgCreateRoleReq req;
    if (!req.ParseFromString(body)) return "";
    return req.role_name();
}
int32 si_pb_create_role_job(const std::string& body) {
    PbLogin::PBMsgCreateRoleReq req;
    if (!req.ParseFromString(body)) return 0;
    return req.job();
}

// --- PBMsgLoginRsp 编码 (error_code + account 基础信息) ---
std::string si_pb_login_rsp(int32 errorCode, int64 accountId, const std::string& username) {
    PbLogin::PBMsgLoginRsp rsp;
    rsp.set_big_type(PbID::AT_LOGIN);
    rsp.set_small_type(PbLogin::LOGIN_MSG_LOGIN_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    if (accountId > 0) {
        rsp.mutable_account()->set_account_id(accountId);
        rsp.mutable_account()->set_username(username);
    }
    std::string out;
    rsp.SerializeToString(&out);
    return out;
}

// --- PBMsgLoginRsp 编码 (含角色列表) ---
// rolesStr = "roleId,roleName,level,job;roleId,roleName,level,job;..." (空串=无角色)
std::string si_pb_login_rsp_roles(int32 errorCode, int64 accountId, const std::string& username,
                                  const std::string& rolesStr) {
    PbLogin::PBMsgLoginRsp rsp;
    rsp.set_big_type(PbID::AT_LOGIN);
    rsp.set_small_type(PbLogin::LOGIN_MSG_LOGIN_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    if (accountId > 0) {
        rsp.mutable_account()->set_account_id(accountId);
        rsp.mutable_account()->set_username(username);
    }
    if (!rolesStr.empty()) {
        auto toI64 = [](const std::string& s) -> int64 {
            try { return std::stoll(s); } catch (...) { return 0; }
        };
        auto toI32 = [](const std::string& s) -> int32 {
            try { return std::stoi(s); } catch (...) { return 0; }
        };
        size_t begin = 0;
        while (begin < rolesStr.size()) {
            size_t semi = rolesStr.find(';', begin);
            std::string item = rolesStr.substr(begin, semi == std::string::npos ? std::string::npos : semi - begin);
            if (!item.empty()) {
                size_t c1 = item.find(',');
                if (c1 != std::string::npos) {
                    size_t c2 = item.find(',', c1 + 1);
                    size_t c3 = (c2 == std::string::npos) ? std::string::npos : item.find(',', c2 + 1);
                    PbAccount::RoleInfo* role = rsp.add_roles();
                    role->set_role_id(toI64(item.substr(0, c1)));
                    role->set_role_name(item.substr(c1 + 1, c2 == std::string::npos ? std::string::npos : c2 - c1 - 1));
                    if (c2 != std::string::npos) {
                        role->set_level(toI32(item.substr(c2 + 1, c3 == std::string::npos ? std::string::npos : c3 - c2 - 1)));
                        if (c3 != std::string::npos) {
                            role->set_job(toI32(item.substr(c3 + 1)));
                        }
                    }
                }
            }
            if (semi == std::string::npos) break;
            begin = semi + 1;
        }
    }
    std::string out;
    rsp.SerializeToString(&out);
    return out;
}

// --- PBMsgSelectRoleRsp 编码 (error_code + player 信息) ---
std::string si_pb_select_role_rsp(int32 errorCode, int64 playerId, int64 roleId,
                                  const std::string& roleName) {
    PbLogin::PBMsgSelectRoleRsp rsp;
    rsp.set_big_type(PbID::AT_LOGIN);
    rsp.set_small_type(PbLogin::LOGIN_MSG_SELECT_ROLE_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.mutable_player()->set_player_id(playerId);
    rsp.mutable_player()->set_role_id(roleId);
    rsp.mutable_player()->set_role_name(roleName);
    std::string out;
    rsp.SerializeToString(&out);
    return out;
}

// --- PBMsgCreateRoleRsp 编码 (error_code + 创建成功的角色摘要) ---
std::string si_pb_create_role_rsp(int32 errorCode, int64 roleId, const std::string& roleName,
                                  int32 level, int32 job) {
    PbLogin::PBMsgCreateRoleRsp rsp;
    rsp.set_big_type(PbID::AT_LOGIN);
    rsp.set_small_type(PbLogin::LOGIN_MSG_CREATE_ROLE_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.mutable_role()->set_role_id(roleId);
    rsp.mutable_role()->set_role_name(roleName);
    rsp.mutable_role()->set_level(level);
    rsp.mutable_role()->set_job(job);
    std::string out;
    rsp.SerializeToString(&out);
    return out;
}
%}

// ================================================================
// 14. 通用 protobuf 字段读取 — si_pb_* (proto3 wire 格式)
// 所有业务消息前两个字段固定为 big_type(1)/small_type(2), 其余按字段号读取。
// 用 CodedInputStream 通用解析, 免去为每个消息单独写解码函数。
// ================================================================
%inline %{
// 读字段号 fieldNumber 的首个 varint 字段值 (uint32/int64/bool 通用)
int64 si_pb_varint_field(const std::string& body, int32 fieldNumber) {
    google::protobuf::io::CodedInputStream in(
        reinterpret_cast<const uint8_t*>(body.data()), static_cast<int>(body.size()));
    uint32_t tag = in.ReadTag();
    while (tag != 0) {
        if ((tag >> 3) == (uint32_t)fieldNumber && (tag & 7) == 0) {
            uint64_t v = 0;
            return in.ReadVarint64(&v) ? (int64_t)v : 0;
        }
        if (!pb_skip_field(in, tag)) break;
        tag = in.ReadTag();
    }
    return 0;
}

// 读字段号 fieldNumber 的首个 sint32 字段值 (proto3 zigzag 编码 → 有符号 int32)
int32 si_pb_sint32_field(const std::string& body, int32 fieldNumber) {
    google::protobuf::io::CodedInputStream in(
        reinterpret_cast<const uint8_t*>(body.data()), static_cast<int>(body.size()));
    uint32_t tag = in.ReadTag();
    while (tag != 0) {
        if ((tag >> 3) == (uint32_t)fieldNumber && (tag & 7) == 0) {
            uint64_t v = 0;
            if (!in.ReadVarint64(&v)) return 0;
            uint32_t n = (uint32_t)v;   // sint32 zigzag: (n >> 1) ^ -(n & 1)
            return (int32_t)((n >> 1) ^ (uint32_t)(-(int32_t)(n & 1)));
        }
        if (!pb_skip_field(in, tag)) break;
        tag = in.ReadTag();
    }
    return 0;
}

// 读字段号 fieldNumber 的首个 length-delimited (string/bytes) 字段值
std::string si_pb_string_field(const std::string& body, int32 fieldNumber) {
    google::protobuf::io::CodedInputStream in(
        reinterpret_cast<const uint8_t*>(body.data()), static_cast<int>(body.size()));
    uint32_t tag = in.ReadTag();
    while (tag != 0) {
        if ((tag >> 3) == (uint32_t)fieldNumber && (tag & 7) == 2) {
            uint32_t len = 0;
            if (!in.ReadVarint32(&len)) return std::string();
            std::string v;
            return in.ReadString(&v, (int)len) ? v : std::string();
        }
        if (!pb_skip_field(in, tag)) break;
        tag = in.ReadTag();
    }
    return std::string();
}

int64 si_pb_big_type(const std::string& body)   { return si_pb_varint_field(body, 1); }
int64 si_pb_small_type(const std::string& body) { return si_pb_varint_field(body, 2); }
bool  si_pb_bool_field(const std::string& body, int32 fieldNumber) {
    return si_pb_varint_field(body, fieldNumber) != 0;
}
%}

// ================================================================
// 15. 通用业务转发 — si_forward_rpc (Gate → 任意业务 Actor)
// ================================================================
%inline %{
// 把客户端业务消息转发给 (actorType, actorID) 对应 Actor 的 on_rpc_forward,
// 返回其生成的客户端回包 protobuf (失败/超时返回空串)。
std::string si_forward_rpc(int32 actorType, int64 actorID, const std::string& payload) {
    return CActorLoadManager::instance()->ForwardToActor((EActorType)actorType, actorID, payload);
}

// 服务器主动推送: 把 payload (客户端可接收的业务 protobuf) 推给指定玩家的连接。
// 内部异步发 RPC_METHOD_PUSH 到 Gate 进程, 无回包、不阻塞 Actor/调度线程。
void si_push_rpc(int64 playerId, const std::string& payload) {
    CActorLoadManager::instance()->PushToGate(playerId, payload);
}
%}

// ================================================================
// 16. GM 命令模块 — si_gm_* (WebPlatform 控制台 / Lua 侧应答)
//
// CGmCommandMgr 是 Redis pub/sub 控制面: 订阅 cluster:gm:all/type:{t}/pid:{p},
// 框架命令在 C++ 执行, Actor 命令经 PostMessage(SYS_GM_CMD) → on_gm_cmd 串行执行。
// 应答统一写 Redis 列表 cluster:gm:replies (供 gm.php 读取)。
// ================================================================
%inline %{
// 写一条 GM 应答日志到 cluster:gm:replies (任意线程, 内部加锁 + 时间戳/进程前缀)
void si_gm_reply(const std::string& text) {
    CGmCommandMgr::instance()->LogReply(text);
}

// 发布消息到任意 GM 频道 (扩展用)
bool si_gm_publish(const std::string& channel, const std::string& text) {
    return CGmCommandMgr::instance()->Publish(channel, text);
}
%}
