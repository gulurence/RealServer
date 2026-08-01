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
#include "luacore/CLuaActor.h"
#include "xRedis/RedisMgr.h"
#include "xDatabase/xOperatorMgr.h"
#include "xRecord/xRecordMgr.h"
#include "xRecord/xDatabaseRecord.h"
#include "xRpc/xRpc.h"
#include "rpc/actor_mgr.grpc.pb.h"
%}

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
    return (ActorPtr)ActorCreator::Create(actorType, actorID);
}

// Create a CLuaActor — the C++ to Lua bridge for actor business logic
ActorPtr si_lua_actor_create(int32 actorType, int64 actorID, const char* actorName) {
    return (ActorPtr)(new CLuaActor(actorType, actorID, std::string(actorName)));
}

// Dispatch an RPC call to a Lua actor.
// actorPtr  — target actor (opaque ActorPtr)
// rpcName   — first gRPC param, used to locate on_rpc_<name> handler in Lua
// rpcData   — second gRPC param, Lua table data transparently passed
bool si_actor_dispatch_rpc(ActorPtr actorPtr, const char* rpcName, const char* rpcData) {
    if (!actorPtr) return false;
    return ((CLuaActor*)actorPtr)->DispatchRpc(rpcName, rpcData);
}
%}

// ================================================================
// 8. ActorMgr instance management — si_actor_mgr_*
// ================================================================
%inline %{
// Declare that this process can host actors of the given type.
void si_actor_mgr_add_hosted_type(int32 actorType) {
    CActorMgr::instance()->AddHostedType(actorType);
}

// Check whether this process hosts the given actor type.
bool si_actor_mgr_can_host_type(int32 actorType) {
    return CActorMgr::instance()->CanHostType(actorType);
}

// Add an already-created actor instance to the manager.
bool si_actor_mgr_add_instance(ActorPtr actor) {
    return CActorMgr::instance()->AddInstance((CActor*)actor);
}

// Remove an actor instance from the manager (does NOT delete).
ActorPtr si_actor_mgr_remove_instance(int32 actorType, int64 actorID) {
    return (ActorPtr)CActorMgr::instance()->RemoveInstance(actorType, actorID);
}

// Install the actor instance (with connection ref counting).
bool si_actor_mgr_install(int32 actorType, int64 actorID) {
    return CActorMgr::instance()->Install(actorType, actorID);
}

// Uninstall the actor instance (with connection ref counting).
bool si_actor_mgr_uninstall(int32 actorType, int64 actorID) {
    return CActorMgr::instance()->UnInstall(actorType, actorID);
}

// Find an actor instance by type and id (local only).
ActorPtr si_actor_mgr_find(int32 actorType, int64 actorID) {
    return (ActorPtr)CActorMgr::instance()->Find(actorType, actorID);
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
    return (ActorPtr)ActorCreator::Create(actorType, actorID);
}

// Delete an actor instance locally.
bool si_actor_creator_delete(int32 actorType, int64 actorID) {
    return ActorCreator::DeleteActor(actorType, actorID);
}

// Get this process's total actor count (used as load score).
int32 si_actor_creator_get_process_load() {
    return ActorCreator::GetLocalInstanceCount();
}

// Get count of actors of a specific type on this process.
int32 si_actor_creator_get_local_count(int32 actorType) {
    return ActorCreator::GetLocalTypeCount(actorType);
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
// 11. ActorCreator gRPC forwarding — si_actor_creator_grpc_*
// Used by Lua xEngine.ActorCreator for cross-process actor ops.
// ================================================================

// gRPC CreateActor — request remote process to create an actor.
// serverName must match a channel registered with CRpcService.
bool si_actor_creator_grpc_create(const std::string& serverName,
                                   int32 actorType, int64 actorID) {
    auto channel = CRpcService::getMe().GetChannel(serverName);
    if (!channel) {
        XERR("[si_actor_creator_grpc_create] no channel for '%s'", serverName.c_str());
        return false;
    }
    actormgr::CreateActorRequest req;
    actormgr::CreateActorReply reply;
    req.set_actor_type(actorType);
    req.set_actor_id(actorID);

    auto stub = actormgr::ActorMgrService::NewStub(channel);
    grpc::ClientContext ctx;
    auto status = stub->CreateActor(&ctx, req, &reply);
    if (!status.ok()) {
        XERR("[si_actor_creator_grpc_create] gRPC error: %s", status.error_message().c_str());
        return false;
    }
    return reply.success();
}

// gRPC FindActor — query whether a remote process has an actor.
// Returns true if found; outFound and outRemoteType are always set.
bool si_actor_creator_grpc_find(const std::string& serverName,
                                 int32 actorType, int64 actorID,
                                 bool* outFound, int32* outRemoteType) {
    if (outFound)     *outFound    = false;
    if (outRemoteType) *outRemoteType = 0;

    auto channel = CRpcService::getMe().GetChannel(serverName);
    if (!channel) {
        XERR("[si_actor_creator_grpc_find] no channel for '%s'", serverName.c_str());
        return false;
    }
    actormgr::FindActorRequest req;
    actormgr::FindActorReply reply;
    req.set_actor_type(actorType);
    req.set_actor_id(actorID);

    auto stub = actormgr::ActorMgrService::NewStub(channel);
    grpc::ClientContext ctx;
    auto status = stub->FindActor(&ctx, req, &reply);
    if (!status.ok()) {
        XERR("[si_actor_creator_grpc_find] gRPC error: %s", status.error_message().c_str());
        return false;
    }
    if (outFound)     *outFound     = reply.found();
    if (outRemoteType) *outRemoteType = reply.remote_type();
    return reply.found();
}

// gRPC UninstallActor — request remote process to uninstall an actor.
bool si_actor_creator_grpc_uninstall(const std::string& serverName,
                                      int32 actorType, int64 actorID) {
    auto channel = CRpcService::getMe().GetChannel(serverName);
    if (!channel) {
        XERR("[si_actor_creator_grpc_uninstall] no channel for '%s'", serverName.c_str());
        return false;
    }
    actormgr::UninstallActorRequest req;
    actormgr::UninstallActorReply reply;
    req.set_actor_type(actorType);
    req.set_actor_id(actorID);

    auto stub = actormgr::ActorMgrService::NewStub(channel);
    grpc::ClientContext ctx;
    auto status = stub->UninstallActor(&ctx, req, &reply);
    if (!status.ok()) {
        XERR("[si_actor_creator_grpc_uninstall] gRPC error: %s", status.error_message().c_str());
        return false;
    }
    return reply.success();
}
%}


