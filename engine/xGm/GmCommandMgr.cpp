#include "GmCommandMgr.h"

#include "xActor/Actor.h"
#include "xActor/ActorMgr.h"
#include "xActor/ActorMsgId.h"
#include "xActor/ActorScheduler.h"
#include "xCluster/xProcessManager.h"
#include "xCluster/xServiceDiscovery.h"
#include "xRedis/RedisMgr.h"
#include "luacore/LuaVM.h"
#include "xLog/xLog.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h> // getpid()
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <utility>

// =============================================================================
// 常量与本地工具
// =============================================================================
namespace
{

// GM 应答日志列表 (封顶条数)
const char *kGmReplyKey = "cluster:gm:replies";
const int32 kGmReplyMax = 1000;

// Actor 注册键 TTL (秒): 30s ≈ 6 次心跳 (心跳间隔 5s)。进程每 5s 心跳续期,
// 进程宕机后由 Redis 过期自动清理, 无需进程主动扫描 (僵尸 Actor 防护)。
constexpr int64 kActorRegTtlSeconds = 30;

// FNV-1a 64-bit 内容散列 → 16 位小写 hex (仅用于「内容是否变更」判断, 非密码学)。
// 与 WebPlatform 侧 md5 用途不同: 此处只做进程内「本次拉取内容 vs 上次已应用内容」的门控。
std::string Fnv1a64Hex(const std::string &data)
{
    uint64 h = 1469598103934665603ULL; // FNV offset basis
    for (unsigned char c : data)
    {
        h ^= (uint64)c;
        h *= 1099511628211ULL; // FNV prime
    }
    static const char kHex[] = "0123456789abcdef";
    char buf[17] = {0};
    for (int32 i = 15; i >= 0; --i)
    {
        buf[i] = kHex[h & 0xF];
        h >>= 4;
    }
    return buf;
}

// 把内容写回本地脚本文件 (热更成功后覆盖), 自动建父目录。返回是否成功。
bool WriteScriptFile(const std::string &root, const std::string &rel, const std::string &content)
{
    std::filesystem::path dst = std::filesystem::path(root) / rel;
    std::error_code ec;
    if (dst.has_parent_path())
    {
        std::filesystem::create_directories(dst.parent_path(), ec);
    }
    std::ofstream ofs(dst, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        return false;
    }
    ofs.write(content.data(), (std::streamsize)content.size());
    ofs.close();
    return true;
}

// 解包 WebPlatform 打包的脚本热更数据 (二进制):
//   "HFX1" + version(u32 LE) + count(u32 LE) + 每文件[pathLen(u32 LE)+path+contentLen(u32 LE)+content]...
// 返回 (relpath, content) 列表; 格式错误返回 false。
bool UnpackHotfixPackage(const std::string &blob,
                         std::vector<std::pair<std::string, std::string>> &files)
{
    files.clear();
    if (blob.size() < 4 || blob.compare(0, 4, "HFX1") != 0)
    {
        return false;
    }
    size_t off = 4;
    auto rd32 = [&](uint32 &v) -> bool {
        if (off + 4 > blob.size())
        {
            return false;
        }
        v = (uint32)(uint8)blob[off] | ((uint32)(uint8)blob[off + 1] << 8)
            | ((uint32)(uint8)blob[off + 2] << 16) | ((uint32)(uint8)blob[off + 3] << 24);
        off += 4;
        return true;
    };
    uint32 ver = 0, count = 0;
    if (!rd32(ver) || !rd32(count))
    {
        return false;
    }
    XLOG("[GM] hotfix package parsed: version=%u files=%u", ver, count);

    files.reserve(count);
    for (uint32 i = 0; i < count; ++i)
    {
        uint32 plen = 0, clen = 0;
        if (!rd32(plen) || off + plen > blob.size())
        {
            return false;
        }
        std::string path(blob.data() + off, plen);
        off += plen;
        if (!rd32(clen) || off + clen > blob.size())
        {
            return false;
        }
        std::string content(blob.data() + off, clen);
        off += clen;
        files.emplace_back(std::move(path), std::move(content));
    }
    return true;
}

// 判断 relpath 是否属于本进程托管的 Actor 类型逻辑。
// 规则: actors/<dir>/... 按 dir → Actor 类型, 仅当该类型在本进程托管类型集合内才通过;
//       其余路径 (common/ 等公共模块、平台 init.lua) 一律通过 (公共模块所有进程都加载)。
bool IsHostedActorFile(const std::string &rel, const std::vector<EActorType> &hostedTypes)
{
    if (rel.rfind("actors/", 0) != 0 && rel.rfind("actors\\", 0) != 0)
    {
        return true; // 非 actors/<dir> 路径: 公共模块, 所有进程都加载
    }
    std::string rest = rel.substr(7); // 去掉 "actors/"
    size_t slash = rest.find_first_of("/\\");
    std::string dir = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    EActorType t = ActorTypeFromDirName(dir);
    if (t == EActorType_None)
    {
        return true; // 未知目录名, 保守不拦
    }
    for (EActorType h : hostedTypes)
    {
        if (h == t)
        {
            return true;
        }
    }
    return false;
}

// 把本次热更的状态写进本进程 Redis 注册 Hash (cluster:process:{type}:{id}),
// 供 WebPlatform 进程信息页 / 脚本热更页读取展示 (lua 版本 + 预检查/热更结果 + 报错)。
// 进程注册 Hash 带 TTL 且由心跳续期, 此处 HSET 的字段随 Hash 一起过期, 无孤儿数据。
void WriteHotfixStatus(CRedisCli *cli, int32 selfType, int64 selfPid, const std::string &version,
                       const std::string &status, const std::string &error, bool advanceLuaVersion)
{
    if (!cli)
    {
        return;
    }
    std::string key
        = "cluster:process:" + std::to_string(selfType) + ":" + std::to_string(selfPid);
    cli->HSet(key, "hotfix_last_ver", version);
    cli->HSet(key, "hotfix_status", status);
    cli->HSet(key, "hotfix_error", error);
    cli->HSet(key, "hotfix_ts", std::to_string((int64)time(nullptr)));
    if (advanceLuaVersion)
    {
        // 整体成功: 推进 lua 版本, 并把版本号追加进「已热更版本列表」。
        cli->HSet(key, "lua_version", version);
        std::string vers;
        cli->HGet(key, "hotfix_versions", vers);
        if (!vers.empty())
        {
            vers += ",";
        }
        vers += version;
        cli->HSet(key, "hotfix_versions", vers);
    }
}

// 按空白切分命令行 (简单空格/Tab 分隔, 不处理引号)
std::vector<std::string> SplitWords(const std::string &s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s)
    {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
    {
        out.push_back(cur);
    }
    return out;
}

// 动态开启进程: 启动与自身同目录的 RealServer.exe, 传 --type/--name/--platform
// 新进程继承工作目录 (含 config/ 与 script/), 独立控制台窗口。
bool SpawnProcess(EProcessType type, const std::string &name)
{
#ifdef _WIN32
    char exePath[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
    {
        XERR("[GM] spawn: GetModuleFileNameA failed (err=%lu)", GetLastError());
        return false;
    }

    std::string cmdline = "\"" + std::string(exePath) + "\"" + " --type="
                          + std::to_string((int32)type) + " --name=" + name + " --platform=dev";

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // 独立控制台 + 共享 cwd (config/script 相对路径), 不继承标准句柄
    BOOL ok = CreateProcessA(nullptr, (LPSTR)cmdline.c_str(), nullptr, nullptr, FALSE,
                             CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);
    if (!ok)
    {
        XERR("[GM] spawn: CreateProcessA failed (err=%lu)", GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    XLOG("[GM] spawn: launched %s (pid=%lu)", cmdline.c_str(), (unsigned long)pi.dwProcessId);
    return true;
#else
    std::string cmdline = "RealServer --type=" + std::to_string((int32)type) + " --name=" + name
                          + " --platform=dev &";
    XWRN("[GM] spawn (non-Windows fallback): %s", cmdline.c_str());
    return system(cmdline.c_str()) == 0;
#endif
}

} // namespace

// =============================================================================
// Init / Shutdown
// =============================================================================
bool CGmCommandMgr::Init(const ThridRedisConfig &cfg, EProcessType selfType, int64 selfPid)
{
    m_cfg = cfg;
    m_selfType = selfType;
    m_selfPid = selfPid;

    // 复用 CRedisPoolMgr 的共享 RedisCluster (按 title 取), 承担写命令/发布/注册,
    // 不再自建 hiredis 写连接 (hiredis 对 key 命令不感知 slot, 集群下会 MOVED)。
    m_cli = CRedisPoolMgr::getMe().GetRedisCli(cfg.title);
    if (!m_cli)
    {
        XWRN("[GM] shared RedisCluster not found for title '%s' — writes will be dropped",
             cfg.title.c_str());
    }

    m_running = true;
    m_thread = std::thread(&CGmCommandMgr::SubscriberLoop, this);

    XLOG("[GM] initialized: type=%d pid=%lld redis=%s:%d cluster=%s", (int32)selfType,
         (int64)selfPid, cfg.host.c_str(), cfg.port, m_cli ? "ok" : "missing");
    return true;
}

void CGmCommandMgr::Shutdown()
{
    m_running = false;
    // subscriber 线程阻塞在 redisGetReply; 强制断连以唤醒并退出。
    if (m_sub)
    {
        redisFree(m_sub);
        m_sub = nullptr;
    }
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    // m_cli 为 CRedisPoolMgr 共享句柄, 由其析构统一释放, 此处仅置空。
    m_cli = nullptr;
    XLOG("[GM] shutdown");
}

// =============================================================================
// LogReply / Publish (任意线程, 共享 RedisCluster 线程安全)
// =============================================================================
void CGmCommandMgr::LogReply(const std::string &text)
{
    if (!m_cli)
    {
        XWRN("[GM] LogReply dropped (no redis cluster): %s", text.c_str());
        return;
    }

    // 前缀: 时间戳 + 进程身份, 便于控制台按时间/来源排序
    char head[96] = {0};
    snprintf(head, sizeof(head), "%lld [%s:%lld] ", (int64)time(nullptr),
             ProcessTypeName(m_selfType), (int64)m_selfPid);

    std::string line = std::string(head) + text;
    m_cli->RPush(kGmReplyKey, line);
    m_cli->LTrim(kGmReplyKey, -kGmReplyMax, -1);
}

bool CGmCommandMgr::Publish(const std::string &channel, const std::string &text)
{
    if (!m_cli)
    {
        return false;
    }
    return m_cli->Publish(channel, text) >= 0;
}

// =============================================================================
// Actor 注册 / 注销 (K8s 风格管理)
// =============================================================================
bool CGmCommandMgr::RegisterActor(EActorType type, int64 actorId, const std::string &name,
                                  uint64 globalId)
{
    if (!m_cli)
    {
        return false;
    }

    std::string key
        = "cluster:actor:" + std::to_string((int32)type) + ":" + std::to_string(actorId);
    std::string idStr = std::to_string(actorId);

    bool ok = true;
    auto hset = [&](const char *field, const std::string &val)
    {
        if (!ok)
        {
            return;
        }
        if (m_cli->HSet(key, field, val) != REDIS_OK)
        {
            ok = false;
        }
    };

    hset("type", std::to_string((int32)type));
    hset("id", idStr);
    hset("gid", std::to_string((uint64)globalId)); // 全局唯一 ID (xActorId::MakeActorId)
    hset("name", name);
    hset("pid", std::to_string(m_selfPid));
    hset("ptype", std::to_string((int32)m_selfType));
    hset("state", "1"); // 1=active
    hset("created", std::to_string((int64)time(nullptr))); // Unix 秒, 供 Web 显示创建/存活时间

    m_cli->SAdd("cluster:actor:by_type:" + std::to_string((int32)type), idStr);

    // 持久实例索引 (无 TTL): 灰度迁移「先迁后杀」与进程宕机后冷恢复的枚举数据源。
    // cluster:actor:by_type 有 TTL, 进程死后会蒸发; 本索引独立、不过期, 只在
    // DeregisterActor 时移除, 保证幸存者可据其重建已失联 Actor。
    m_cli->SAdd("actor:index:" + std::to_string((int32)type), idStr);

    // 注册键 TTL: 进程宕机后由 Redis 过期清理 (僵尸 Actor 防护), 心跳续期见 RefreshActorRegistration
    m_cli->Expire(key, kActorRegTtlSeconds);
    m_cli->Expire("cluster:actor:by_type:" + std::to_string((int32)type), kActorRegTtlSeconds);

    XLOG("[GM] RegisterActor type=%d id=%lld gid=%llu name=%s ok=%d", (int32)type, (int64)actorId,
         (uint64)globalId, name.c_str(), ok);
    return ok;
}

bool CGmCommandMgr::DeregisterActor(EActorType type, int64 actorId)
{
    if (!m_cli)
    {
        return false;
    }

    std::string key
        = "cluster:actor:" + std::to_string((int32)type) + ":" + std::to_string(actorId);
    std::string idStr = std::to_string(actorId);

    m_cli->Del(key.c_str());
    m_cli->SRem("cluster:actor:by_type:" + std::to_string((int32)type), idStr);
    m_cli->SRem("actor:index:" + std::to_string((int32)type), idStr);

    XLOG("[GM] DeregisterActor type=%d id=%lld", (int32)type, (int64)actorId);
    return true;
}

bool CGmCommandMgr::RefreshActorRegistration(EActorType type, int64 actorId)
{
    if (!m_cli)
    {
        return false;
    }

    std::string key
        = "cluster:actor:" + std::to_string((int32)type) + ":" + std::to_string(actorId);
    m_cli->Expire(key, kActorRegTtlSeconds);
    m_cli->Expire("cluster:actor:by_type:" + std::to_string((int32)type), kActorRegTtlSeconds);
    return true;
}

// =============================================================================
// 订阅线程
// =============================================================================
void CGmCommandMgr::SubscriberLoop()
{
    m_sub = redisConnect(m_cfg.host.c_str(), m_cfg.port);
    if (!m_sub || m_sub->err)
    {
        if (m_sub)
        {
            XERR("[GM] subscriber connect failed: %s", m_sub->errstr);
            redisFree(m_sub);
            m_sub = nullptr;
        }
        else
        {
            XERR("[GM] subscriber connect failed: OOM");
        }
        return;
    }
    if (!m_cfg.authkey.empty())
    {
        redisReply *r = (redisReply *)redisCommand(m_sub, "AUTH %s", m_cfg.authkey.c_str());
        if (r)
        {
            freeReplyObject(r);
        }
    }

    std::string cAll = "cluster:gm:all";
    std::string cType = "cluster:gm:type:" + std::to_string((int32)m_selfType);
    std::string cPid = "cluster:gm:pid:" + std::to_string(m_selfPid);
    redisReply *r = (redisReply *)redisCommand(m_sub, "SUBSCRIBE %s %s %s", cAll.c_str(),
                                               cType.c_str(), cPid.c_str());
    if (r)
    {
        freeReplyObject(r);
    }
    XLOG("[GM] subscribed: %s %s %s", cAll.c_str(), cType.c_str(), cPid.c_str());

    while (m_running)
    {
        redisReply *reply = nullptr;
        if (redisGetReply(m_sub, (void **)&reply) != REDIS_OK)
        {
            XWRN("[GM] subscriber read failed, exiting loop");
            break;
        }
        if (!reply)
        {
            continue;
        }

        // pub/sub 消息帧: ["message", channel, payload]
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3
            && reply->element[0]->type == REDIS_REPLY_STRING
            && strncmp(reply->element[0]->str, "message", reply->element[0]->len) == 0)
        {
            std::string msg(reply->element[2]->str, reply->element[2]->len);
            HandleCommand(msg);
        }
        freeReplyObject(reply);
    }
}

// =============================================================================
// 命令分发
// =============================================================================
void CGmCommandMgr::HandleCommand(const std::string &message)
{
    std::vector<std::string> argv = SplitWords(message);
    if (argv.empty())
    {
        return;
    }

    if (argv[0] == "actor")
    {
        ForwardToActor(argv);
    }
    else
    {
        LogReply(ExecFramework(argv));
    }
}

std::string CGmCommandMgr::ExecFramework(const std::vector<std::string> &argv)
{
    const std::string &cmd = argv[0];

    if (cmd == "help")
    {
        return "commands: help | info | processes | actors | actor <type> <id> <cmd> | "
               "spawn <type> [name] | shutdown | loglevel <level> | reload [<actorType> [<id>]] | "
               "hotfix <version> <relpath...> | migrate_save";
    }

    if (cmd == "info")
    {
        const ProcessInfo &self = CProcessManager::getMe().GetSelfInfo();
        char buf[256] = {0};
        snprintf(buf, sizeof(buf),
                 "type=%s(%d) id=%lld name=%s host=%s rpcPort=%u state=%s actors=%zu",
                 ProcessTypeName(self.type), (int32)self.type, (int64)self.processId,
                 self.processName.c_str(), self.host.c_str(), self.rpcPort,
                 ProcessStateName(self.state), CActorMgr::instance()->GetInstanceCount());
        return buf;
    }

    if (cmd == "processes")
    {
        std::vector<ProcessInfo> peers = CServiceDiscovery::getMe().GetAllRunningPeers();
        if (peers.empty())
        {
            return "no running peers";
        }
        std::string out = std::to_string(peers.size()) + " peer(s): ";
        for (size_t i = 0; i < peers.size(); ++i)
        {
            if (i)
            {
                out += ", ";
            }
            out += std::string(ProcessTypeName(peers[i].type)) + ":"
                   + std::to_string(peers[i].processId) + "@" + peers[i].host + ":"
                   + std::to_string(peers[i].rpcPort);
        }
        return out;
    }

    if (cmd == "actors")
    {
        std::string out;
        for (EActorType t : CActorMgr::instance()->GetHostedTypes())
        {
            for (CActor *a : CActorMgr::instance()->GetByType(t))
            {
                if (!out.empty())
                {
                    out += "; ";
                }
                char buf[96] = {0};
                snprintf(buf, sizeof(buf), "type=%d id=%lld name=%s state=%d", (int32)t,
                         (int64)a->GetActorID(), a->GetActorName().c_str(), a->GetState());
                out += buf;
            }
        }
        return out.empty() ? "no local actors" : out;
    }

    if (cmd == "spawn")
    {
        if (argv.size() < 2)
        {
            return "usage: spawn <type> [name]  (type: 100=Gate 101=Login 102=Global 201=Game)";
        }
        EProcessType type = (EProcessType)atoi(argv[1].c_str());
        if (!IsValidProcessType(type))
        {
            return "invalid process type '" + argv[1] + "'";
        }
        std::string name
            = argv.size() >= 3 ? argv[2] : (std::string(ProcessTypeName(type)) + "-gm");
        return SpawnProcess(type, name) ? ("spawned " + name + " type=" + argv[1])
                                        : ("spawn failed for " + name);
    }

    if (cmd == "shutdown")
    {
        LogReply("shutdown requested — stopping now");
        CProcessManager::getMe().SetState(EProcessState_Stopping);
        CServiceDiscovery::getMe().Deregister();
        // main.cpp 主循环是 while(true) 死循环, 优雅关闭不可达;
        // 静态析构阶段会阻塞 (线程 join / 网络关闭), 因此用 _Exit 立即终止。
        std::_Exit(0);
        return "";
    }

    if (cmd == "loglevel")
    {
        return "loglevel: runtime log-level switch is compile-time only — arg='"
               + (argv.size() >= 2 ? argv[1] : "") + "'";
    }

    if (cmd == "reload")
    {
        // Lua 脚本热更: 唯一 lua_State 只能由调度线程触碰, 而 GM 订阅线程跑在独立
        // std::thread, 故经 CActorScheduler::DispatchSync 投递到调度器串行执行
        // (与 RPC 转发/创建链路同款, 避免跨线程直接调 Lua 并发毁栈)。
        // 用法: reload [<actorType> [<id>]] (参数当前仅回显; 全量重载 script/** 业务脚本)
        std::string target;
        if (argv.size() >= 2)
        {
            target = " type=" + argv[1];
            if (argv.size() >= 3)
            {
                target += " id=" + argv[2];
            }
        }

        bool ok = CActorScheduler::instance()->DispatchSync(
            []() { return LuaVM::instance()->HotReload(); });

        return ok ? ("reload OK — business scripts hot-reloaded" + target)
                  : ("reload FAILED — see server log" + target);
    }

    if (cmd == "migrate_save")
    {
        // 灰度迁移「先迁后杀」: 通知本地 Actor 把内存态落 Redis (data_save(2)),
        // 供新进程 createLocal + data_load 恢复 (见 common/migration.lua)。
        // 可选参数 <os_pid>: 精确瞄准被杀的进程 (gray_roll.ps1 广播到 type 频道时,
        // 只有 OS 进程号匹配者执行, 避免误伤同类型仍在运行的其它进程)。
        if (argv.size() >= 2)
        {
            int64 wantPid = 0;
            if (std::sscanf(argv[1].c_str(), "%lld", (long long *)&wantPid) == 1 && wantPid > 0)
            {
#ifdef _WIN32
                int64 selfPid = (int64)GetCurrentProcessId();
#else
                int64 selfPid = (int64)getpid();
#endif
                if (selfPid != wantPid)
                {
                    return "migrate_save SKIP — os_pid mismatch";
                }
            }
        }

        // 唯一 lua_State 只能调度线程碰, 故经 DispatchSync 串行执行 (与 reload 同款)。
        bool ok = CActorScheduler::instance()->DispatchSync(
            []() { return LuaVM::instance()->DoString("xEngine.ActorMigration.saveAll()"); });
        return ok ? "migrate_save OK — local actors flushed to redis"
                  : "migrate_save FAILED — see server log";
    }

    if (cmd == "drain")
    {
        // 进程级排空 (Battle 灰度滚动): 标记本进程 draining, 停止接受新 Actor 分配。
        // 已有 Actor (进行中的战斗) 自然结束, 由 gray_roll.ps1 轮询 cluster:process:<type>:<id>
        // 的 load==0 后再 migrate_save + 杀进程, 避免中途打断战斗。
        // 可选参数 <os_pid>: 与 migrate_save 同款 os_pid 过滤, 广播到 type 频道时只让
        // OS 进程号匹配者执行 (新进程启动即 draining=false, 无需 undrain)。
        if (argv.size() >= 2)
        {
            int64 wantPid = 0;
            if (std::sscanf(argv[1].c_str(), "%lld", (long long *)&wantPid) == 1 && wantPid > 0)
            {
#ifdef _WIN32
                int64 selfPid = (int64)GetCurrentProcessId();
#else
                int64 selfPid = (int64)getpid();
#endif
                if (selfPid != wantPid)
                {
                    return "drain SKIP — os_pid mismatch";
                }
            }
        }

        CServiceDiscovery::getMe().UpdateDrain(true);
        LogReply("drain requested — marking process draining (no new actors)");
        return "drain OK — draining";
    }

    if (cmd == "hotfix")
    {
        // 定向脚本热更 (数据驱动): WebPlatform 把「MD5 有变化的文件」打包成二进制
        // cluster:hotfix:pkg:<version>, 命令只带版本号。进程 GET 包 → 解包 → 用内存
        // 内容直接热更 (load, 不读盘) → 成功后覆盖本地文件。
        // 用法: hotfix <version>
        if (argv.size() < 2)
        {
            return "usage: hotfix <version>";
        }

        const std::string version = argv[1];

        if (!m_cli)
        {
            return "hotfix FAILED — no redis cluster";
        }

        std::string blob;
        const std::string pkgKey = "cluster:hotfix:pkg:" + version;
        if (m_cli->Get(pkgKey, blob) != REDIS_OK)
        {
            return "hotfix FAILED — package not found: " + pkgKey;
        }

        std::vector<std::pair<std::string, std::string>> files;
        if (!UnpackHotfixPackage(blob, files))
        {
            return "hotfix FAILED — package parse error (bad format)";
        }

        // 过滤: 只处理本进程托管的 Actor 类型逻辑 (actors/<dir>/ 按 dir→类型比对),
        // 公共模块 (common/ 等) 所有进程都加载、一律通过。非本进程托管的文件直接丢弃:
        // 既不预检查、不热更、也不写盘 (别的进程的文件语法错误不影响本进程)。
        std::vector<EActorType> hosted = GetHostedActorTypes(m_selfType);
        std::vector<std::pair<std::string, std::string>> relevant;
        relevant.reserve(files.size());
        int32 filteredOut = 0;
        for (auto &f : files)
        {
            if (!IsHostedActorFile(f.first, hosted))
            {
                XLOG("[GM] hotfix filter-out (not hosted here) file=%s", f.first.c_str());
                filteredOut++;
                continue;
            }
            relevant.push_back(std::move(f));
        }

        // hash 门控: 内容与上次成功应用一致则跳过 (不再热更、不再写盘)。
        std::vector<std::pair<std::string, std::string>> changed; // (relpath, content)
        std::unordered_map<std::string, std::string> newHashes;   // relpath -> FNV-1a hex
        int32 skipped = 0;
        for (auto &f : relevant)
        {
            const std::string hash = Fnv1a64Hex(f.second);
            auto it = m_hotfixAppliedHash.find(f.first);
            if (it != m_hotfixAppliedHash.end() && it->second == hash)
            {
                XLOG("[GM] hotfix skip (hash unchanged) file=%s hash=%s", f.first.c_str(), hash.c_str());
                skipped++;
                continue;
            }
            newHashes[f.first] = hash;
            changed.push_back(std::move(f));
        }

        if (changed.empty())
        {
            std::string tail = filteredOut ? ("; not-hosted: " + std::to_string(filteredOut)) : "";
            WriteHotfixStatus(m_cli, (int32)m_selfType, m_selfPid, version, "noop",
                              "no hosted content changed" + tail, false);
            return "hotfix NOOP — no hosted content changed (skipped " + std::to_string(skipped)
                   + " unchanged)" + tail;
        }

        // 热更前预检查: 调度线程上对内存源码做 load(..., "t") 语法校验 (不执行)。
        auto precheck = CActorScheduler::instance()->DispatchSync(
            [&changed]() { return LuaVM::instance()->HotReloadPrecheck(changed); });

        // 预检查无论成败都写入进程信息 (便于运维在界面上看到失败原因)。
        WriteHotfixStatus(m_cli, (int32)m_selfType, m_selfPid, version,
                          precheck.first ? "precheck_ok" : "precheck_fail", precheck.second, false);

        if (!precheck.first)
        {
            XERR("[GM] hotfix precheck FAILED version=%s:\n%s", version.c_str(), precheck.second.c_str());
            return "hotfix PRECHECK FAILED — version=" + version + "\n" + precheck.second;
        }

        // 调度线程上用内存内容热更, 返回「成功热更 / 本进程未加载可安全写盘」的 relpath。
        std::vector<std::string> okPaths = CActorScheduler::instance()->DispatchSync(
            [&changed]() { return LuaVM::instance()->HotReloadFiles(changed); });

        // 热更成功后才覆盖本地文件 + 记录已应用 hash (失败文件不落盘, 便于重试)。
        const std::string &root = LuaVM::instance()->GetScriptRoot();
        int32 applied = 0;
        std::string writeFailed;
        for (const std::string &p : okPaths)
        {
            auto nit = newHashes.find(p);
            if (nit == newHashes.end())
            {
                continue;
            }
            std::string content;
            for (const auto &f : changed)
            {
                if (f.first == p)
                {
                    content = f.second;
                    break;
                }
            }
            if (WriteScriptFile(root, p, content))
            {
                m_hotfixAppliedHash[p] = nit->second;
                applied++;
            }
            else
            {
                writeFailed += (writeFailed.empty() ? "" : ", ") + p;
            }
        }

        // 收集应用失败的 relpath (在 changed 里、却不在 okPaths 里)。
        std::unordered_set<std::string> okSet(okPaths.begin(), okPaths.end());
        std::string applyFailed;
        for (const auto &f : changed)
        {
            if (okSet.find(f.first) == okSet.end())
            {
                applyFailed += (applyFailed.empty() ? "" : ", ") + f.first;
            }
        }

        std::string error;
        if (!applyFailed.empty())
        {
            error += "apply-failed: " + applyFailed;
        }
        if (!writeFailed.empty())
        {
            if (!error.empty())
            {
                error += "; ";
            }
            error += "write-failed: " + writeFailed;
        }

        bool fullOk = applyFailed.empty() && writeFailed.empty();
        WriteHotfixStatus(m_cli, (int32)m_selfType, m_selfPid, version, fullOk ? "ok" : "partial",
                          error, fullOk);

        std::string skipTail = skipped ? ("; skipped-unchanged: " + std::to_string(skipped)) : "";
        std::string filterTail = filteredOut ? ("; not-hosted: " + std::to_string(filteredOut)) : "";
        if (fullOk)
        {
            return "hotfix OK — version=" + version + " applied " + std::to_string(applied)
                   + " file(s)" + skipTail + filterTail;
        }
        return "hotfix PARTIAL — version=" + version + " applied " + std::to_string(applied)
               + "/" + std::to_string(changed.size()) + skipTail + filterTail
               + (error.empty() ? "" : ("; " + error));
    }

    return "unknown command '" + cmd + "' — type 'help'";
}

void CGmCommandMgr::ForwardToActor(const std::vector<std::string> &argv)
{
    // actor <type> <id> <cmd...>
    if (argv.size() < 3)
    {
        LogReply("usage: actor <type> <id> <cmd...>");
        return;
    }
    int32 type = atoi(argv[1].c_str());
    int64 id = atoll(argv[2].c_str());

    std::string body;
    for (size_t i = 3; i < argv.size(); ++i)
    {
        if (i > 3)
        {
            body += " ";
        }
        body += argv[i];
    }
    if (body.empty())
    {
        body = "help";
    }

    CActor *pActor = CActorMgr::instance()->Find((EActorType)type, id);
    if (!pActor)
    {
        LogReply("actor not found: type=" + std::to_string(type) + " id=" + std::to_string(id));
        return;
    }
    // 投递到 Actor 消息队列, 由调度线程串行执行 on_gm_cmd (应答见 CLuaActor)
    pActor->PostMessage(xMsgId::SYS_GM_CMD, body);
}
