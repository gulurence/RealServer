#pragma once

#include "xNetDefine.h"
#include "xBase/xSingleton.h"
#include "CTokenBucketLimiter.h"

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class xNetAgent;
class CRedisCli;

// =============================================================================
// CNetBattleGateway — 战斗进程网络接入层 (客户端直连 Battle 进程, 不经 Gate)
//
// 与 CNetGateway 同构 (复用 xNetAgent), 但路由目标是 BattleActor (多实例):
//   - 连接建立后首帧必须是 LOGIN_REQ (battle.proto BATTLE_LOGIN_REQ), 网关在 C++
//     传输层完成握手: 读 battle:auth:{battleId} 比对 authKey、读 battle:roster:{battleId}
//     校验 playerId 在花名册内; 成功绑定 CID→(battleId, playerId) 并回 LOGIN_RSP。
//   - 后续帧按 msgId = (BigType<<16)|SmallType 投递给对应 BattleActor 的 on_msg,
//     body 沿用 [8 字节 playerId][protobuf body] 前置约定 (playerId 即发送方身份)。
//   - 断线时按会话表合成 DISCONNECT 通知投递给 BattleActor。
//   - SendToPlayer(playerId, body) 由会话表反查 CID 直接下发 (替换旧 si_push_rpc 链路)。
//
// 数据流:
//   客户端 TCP → xNetAgent::Accept → PackageCheck → CNetBattleGateway::OnPackage
//     → (握手后) CActorMgr::Find(EActorType_ABattle, battleId) → PostMessage → on_msg
// =============================================================================
class CNetBattleGateway : public xSingleton<CNetBattleGateway>
{
public:
    CNetBattleGateway();
    ~CNetBattleGateway();

    // 启动监听端口，绑定网络包回调
    bool Start(uint16 u16Port);
    void Stop();

    // --- 网络回调 (由 xNetAgent 内部线程调用) ---
    NetCID OnAccept(void *pParam, tcp_socket &socket);
    void OnPackage(void *pParam, tcp_socket &socket, uint64 u64CID, NetPackageSharedPtr pPackage);
    void OnDisconnect(void *pParam, NetCID u64CID);
    void OnSocketReady(void *pParam, NetCID u64CID, std::shared_ptr<tcp_socket> pSocket);

    // 发送通道: 向指定玩家 (直连本战斗进程) 下发一帧 [uint32 len][body]。
    // 线程安全 (内部投递到 socket 的 executor 异步写)。无会话时返回 false。
    bool SendToPlayer(int64 playerId, const std::string &strBody);

private:
    // 每连接会话状态
    struct Session
    {
        int64 battleId = 0;
        int64 playerId = 0;
        bool loggedIn = false;
        bool spectate = false;   // 观战登录 (改验 battle:spectator:{battleId} 名单, 不验 auth/roster)
    };

    // 向指定连接发送一帧 (自动加 [uint32 len] 前缀)
    bool SendToCID(NetCID u64CID, const std::string &strBody);

    // 限流阈值 (5s TTL 缓存读 cluster:config:cluster 的 battle_rate_limit_per_sec)
    int32 GetRateLimitPerSec();

    // 主动断开连接 (限流超限 / 登录失败)
    void Kick(NetCID u64CID);

    // 登录握手: 解析 LOGIN_REQ → 校验 auth/roster → 绑定会话 → 回 LOGIN_RSP
    void HandleLogin(NetCID u64CID, const std::string &strBody);

    // 校验 playerId 是否在 battle:roster:{battleId} 花名册内
    static bool RosterContains(const std::string &roster, int64 playerId);

    // 校验 playerId 是否在 battle:spectator:{battleId} 观战名单内 (逗号分隔, 与 RosterContains 同构)
    static bool SpectatorContains(const std::string &spectators, int64 playerId);

private:
    NetAgentConfigST m_stConfig;
    std::unique_ptr<xNetAgent> m_pAgent;
    std::atomic<NetCID> m_nextCID{1};

    // CID → socket (发送通道) / CID → 会话 / playerId → CID (反查)
    std::map<NetCID, std::shared_ptr<tcp_socket>> m_mapSockets;
    std::map<NetCID, Session> m_mapSessions;
    std::map<int64, NetCID> m_mapPlayerCid;
    std::mutex m_mutex;

    // 本进程 Redis 连接 (读 battle:auth/roster + 限流阈值; actor_cache 池, Start 时一次取得)
    CRedisCli *m_pRedis = nullptr;

    // 单连接限流
    CTokenBucketLimiter m_limiter;
    int32 m_rateLimitPerSec = 0;
    std::chrono::steady_clock::time_point m_lastRateRead{};
    std::mutex m_rateMutex;
};
