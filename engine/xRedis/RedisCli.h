#pragma once

#include "RedisDefine.h"
#include "xBase/xRecordDataDefine.h"
#include "tService/tThridServiceConfig.h"

#include <sw/redis++/redis++.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef REDIS_OK
#define REDIS_OK 0
#endif
#ifndef REDIS_ERROR
#define REDIS_ERROR 0xFFFFFFFF
#endif

// CRedisCli — Redis Cluster 客户端封装 (redis-plus-plus RedisCluster)
//
// 底层为 sw::redis::RedisCluster: 自带连接池、slot 路由与 MOVED/ASK 重定向,
// 线程安全, 可被多模块共享同一句柄。全仓库只有单 key 命令 (无 MGET/EVAL/多 key),
// 因此逐命令按 slot 自动路由即可, 无需 hash tag。
//
// host/port 语义: 集群 seed 节点 (任意一个即可, 构造时自动 CLUSTER SLOTS 发现全集群)。
class CRedisCli
{
public:
    CRedisCli();
    ~CRedisCli();

    // 连接集群: cfg.host/cfg.port 为 seed 节点; cfg.authkey/user 注入连接; cfg.max_conn
    // 为连接池大小。
    int32 ConnectDB(const ThridRedisConfig &cfg);

public:
    // String
    int32 Set(const std::string &key, const RecordDataST *data);
    int32 Get(const std::string &key, RecordDataST *data);

    // String (string 值版) — 配置/分布式锁用
    int32 Set(const std::string &key, const std::string &value);
    int32 Get(const std::string &key, std::string &out);
    // SET key value NX EX ttl — 控制器 Leader 选举锁 (返回 REDIS_OK 仅当抢锁成功)
    int32 SetNXEx(const std::string &key, const std::string &value, int64 ttlSec);
    // EXPIRE key ttl — 锁续期 (返回 REDIS_OK 成功)
    int32 Expire(const std::string &key, int64 ttlSec);

    // Hash
    int32 HSet(const std::string &hash, const std::string &field, const RecordDataST *data);
    int32 HSet(const std::string &hash, const std::string &field,
               const std::string &value); // 字符串值 (GM 注册表)
    int32 HGet(const std::string &hash, const std::string &field, RecordDataST *data);
    int32 HGet(const std::string &hash, const std::string &field,
               std::string &out); // 字符串值 (NodeManager 读配置)
    int32 HGetAll(const std::string &hash,
                  std::map<std::string, std::string> &out); // 全量读 (Actor 状态恢复)

    int32 Del(const char *key);

public:
    // 集群命令 (供服务发现 / player 索引 / 链路追踪 / Lua 调用)
    int32 SAdd(const std::string &key, const std::string &member);
    int32 SRem(const std::string &key, const std::string &member);
    int32 SMembers(const std::string &key, std::vector<std::string> &members);
    int64 Incr(const std::string &key);
    int32 ZAdd(const std::string &key, int64 score, const std::string &member);
    int32 ZRemRangeByRank(const std::string &key, int64 start, int64 stop);

    // List (GM 应答日志)
    int32 RPush(const std::string &key, const std::string &value);
    int32 LTrim(const std::string &key, int64 start, int64 stop);

    // Pub/Sub (GM 控制面广播, 集群内全局广播)
    int64 Publish(const std::string &channel, const std::string &message);

    // 订阅 channel: 起后台线程阻塞 consume(), msg_cb 在订阅线程执行 (配置热更/控制面下行)。
    // Redis 订阅连接只能发 SUBSCRIBE/退订, 故每 channel 独立线程 + 独立连接, 与命令连接池隔离。
    bool Subscribe(const std::string &channel,
                   std::function<void(const std::string &channel, const std::string &msg)> msg_cb);
    // 停止全部订阅线程 (析构 / 进程关闭时调用)
    void UnsubscribeAll();

public:
    // 探活: 对集群所有 master 发 PING
    bool Validate();

private:
    std::shared_ptr<sw::redis::RedisCluster> m_cluster;
    char m_redisHost[64] = {0}; // seed 节点 (日志用)
    int32 m_redisPort = 0;
    int32 m_maxConn = 20;
    int32 m_connTimeout = 3;
    std::vector<std::thread> m_subThreads; // 订阅线程 (每 channel 一个)
    std::atomic<bool> m_subRun{true};      // 订阅循环开关
};

typedef std::list<CRedisCli *> RedisCliList;
