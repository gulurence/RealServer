#include "RedisCli.h"

#include <chrono>
#include <cstring>
#include <iterator>

CRedisCli::CRedisCli()
{
    strcpy(m_redisHost, "127.0.0.1");
    m_redisPort = 6379;
}

CRedisCli::~CRedisCli()
{
    UnsubscribeAll();
    // m_cluster 由 shared_ptr 释放; RedisCluster 内部连接池自行清理。
}

int32 CRedisCli::ConnectDB(const ThridRedisConfig &cfg)
{
    strncpy(m_redisHost, cfg.host.c_str(), sizeof(m_redisHost) - 1);
    m_redisPort = cfg.port;
    m_maxConn = cfg.max_conn > 0 ? cfg.max_conn : 20;
    m_connTimeout = cfg.conn_timeout > 0 ? cfg.conn_timeout : 3;

    XINF("CRedisCli::ConnectDB [host:%s,port:%d,max_conn:%d] (cluster seed)", m_redisHost,
         m_redisPort, m_maxConn);

    try
    {
        sw::redis::ConnectionOptions connOpts;
        connOpts.host = cfg.host;
        connOpts.port = cfg.port;
        // 仅在有 ACL 用户时覆盖默认 "default" (空用户 + 空密码 = 无鉴权, 不发 AUTH)
        if (!cfg.user.empty())
        {
            connOpts.user = cfg.user;
        }
        connOpts.password = cfg.authkey;
        connOpts.connect_timeout = std::chrono::seconds(m_connTimeout);
        connOpts.socket_timeout = std::chrono::seconds(m_connTimeout);

        sw::redis::ConnectionPoolOptions poolOpts;
        poolOpts.size = (size_t)m_maxConn;

        // 构造时即连 seed + CLUSTER SLOTS 发现全集群; 不可达则抛 Error。
        m_cluster = std::make_shared<sw::redis::RedisCluster>(connOpts, poolOpts);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::ConnectDB error [host:%s,port:%d,error:%s] !!!", m_redisHost, m_redisPort,
             e.what());
        m_cluster.reset();
        return REDIS_ERROR;
    }
    catch (const std::exception &e)
    {
        XERR("CRedisCli::ConnectDB unexpected error [host:%s,port:%d,error:%s] !!!", m_redisHost,
             m_redisPort, e.what());
        m_cluster.reset();
        return REDIS_ERROR;
    }
}

bool CRedisCli::Validate()
{
    if (!m_cluster)
    {
        return false;
    }
    try
    {
        m_cluster->for_each([](sw::redis::Redis &r) { r.ping(); });
        return true;
    }
    catch (const sw::redis::Error &e)
    {
        XWRN("CRedisCli::Validate error [host:%s,port:%d,error:%s]", m_redisHost, m_redisPort,
             e.what());
        return false;
    }
}

int32 CRedisCli::Set(const std::string &key, const RecordDataST *data)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    XDBG("CRedisCli::Set [host:%s,port:%d,key:%s,size:%d]", m_redisHost, m_redisPort, key.c_str(),
         data->Size());
    try
    {
        m_cluster->set(key, std::string(data->Data(), data->Size()));
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Set error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost, m_redisPort,
             key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::Get(const std::string &key, RecordDataST *data)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        auto val = m_cluster->get(key);
        if (!val)
        {
            return REDIS_ERROR; // 键不存在 (nil)
        }
        const std::string &s = *val;
        data->Resize((int32)s.size());
        memcpy(data->MutableData(), s.data(), s.size());
        data->SetSize((int32)s.size());
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Get error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost, m_redisPort,
             key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::Set(const std::string &key, const std::string &value)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->set(key, value);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Set(str) error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::Get(const std::string &key, std::string &out)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        auto val = m_cluster->get(key);
        if (!val)
        {
            return REDIS_ERROR; // 键不存在 (nil)
        }
        out = *val;
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Get(str) error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::SetNXEx(const std::string &key, const std::string &value, int64 ttlSec)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        // SET key value NX EX ttl: 仅当 key 不存在时写入 (抢锁), 成功返回 true。
        bool acquired = m_cluster->set(key, value, std::chrono::seconds((long)ttlSec),
                                       sw::redis::UpdateType::NOT_EXIST);
        return acquired ? REDIS_OK : REDIS_ERROR;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::SetNXEx error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::Expire(const std::string &key, int64 ttlSec)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        return m_cluster->expire(key, std::chrono::seconds((long)ttlSec)) ? REDIS_OK : REDIS_ERROR;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Expire error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::HSet(const std::string &hash, const std::string &field, const RecordDataST *data)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    XDBG("CRedisCli::HSet [host:%s,port:%d,key:%s,field:%s,size:%d]", m_redisHost, m_redisPort,
         hash.c_str(), field.c_str(), data->Size());
    try
    {
        m_cluster->hset(hash, field, std::string(data->Data(), data->Size()));
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::HSet error [host:%s,port:%d,key:%s,field:%s,error:%s] !!!", m_redisHost,
             m_redisPort, hash.c_str(), field.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::HSet(const std::string &hash, const std::string &field, const std::string &value)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->hset(hash, field, value);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::HSet(str) error [host:%s,port:%d,key:%s,field:%s,error:%s] !!!",
             m_redisHost, m_redisPort, hash.c_str(), field.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::HGet(const std::string &hash, const std::string &field, RecordDataST *data)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        auto val = m_cluster->hget(hash, field);
        if (!val)
        {
            return REDIS_ERROR; // 字段不存在 (nil)
        }
        const std::string &s = *val;
        data->Resize((int32)s.size());
        memcpy(data->MutableData(), s.data(), s.size());
        data->SetSize((int32)s.size());
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::HGet error [host:%s,port:%d,key:%s,field:%s,error:%s] !!!", m_redisHost,
             m_redisPort, hash.c_str(), field.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::HGet(const std::string &hash, const std::string &field, std::string &out)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        auto val = m_cluster->hget(hash, field);
        if (!val)
        {
            return REDIS_ERROR; // 字段不存在 (nil)
        }
        out = *val;
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::HGet(str) error [host:%s,port:%d,key:%s,field:%s,error:%s] !!!",
             m_redisHost, m_redisPort, hash.c_str(), field.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::HGetAll(const std::string &hash, std::map<std::string, std::string> &out)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->hgetall(hash, std::inserter(out, out.end()));
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::HGetAll error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, hash.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::Del(const char *key)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    XDBG("CRedisCli::Del [host:%s,port:%d,key:%s]", m_redisHost, m_redisPort, key);
    try
    {
        m_cluster->del(key);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Del error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost, m_redisPort,
             key, e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::SAdd(const std::string &key, const std::string &member)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->sadd(key, member);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::SAdd error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::SRem(const std::string &key, const std::string &member)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->srem(key, member);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::SRem error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::SMembers(const std::string &key, std::vector<std::string> &members)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->smembers(key, std::back_inserter(members));
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::SMembers error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int64 CRedisCli::Incr(const std::string &key)
{
    if (!m_cluster)
    {
        return -1;
    }
    try
    {
        return (int64)m_cluster->incr(key);
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Incr error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return -1;
    }
}

int32 CRedisCli::ZAdd(const std::string &key, int64 score, const std::string &member)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->zadd(key, member, (double)score);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::ZAdd error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::ZRemRangeByRank(const std::string &key, int64 start, int64 stop)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->zremrangebyrank(key, (int64)start, (int64)stop);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::ZRemRangeByRank error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::RPush(const std::string &key, const std::string &value)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->rpush(key, value);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::RPush error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int32 CRedisCli::LTrim(const std::string &key, int64 start, int64 stop)
{
    if (!m_cluster)
    {
        return REDIS_ERROR;
    }
    try
    {
        m_cluster->ltrim(key, (int64)start, (int64)stop);
        return REDIS_OK;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::LTrim error [host:%s,port:%d,key:%s,error:%s] !!!", m_redisHost,
             m_redisPort, key.c_str(), e.what());
        return REDIS_ERROR;
    }
}

int64 CRedisCli::Publish(const std::string &channel, const std::string &message)
{
    if (!m_cluster)
    {
        return -1;
    }
    try
    {
        return (int64)m_cluster->publish(channel, message);
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Publish error [host:%s,port:%d,channel:%s,error:%s] !!!", m_redisHost,
             m_redisPort, channel.c_str(), e.what());
        return -1;
    }
}

bool CRedisCli::Subscribe(const std::string &channel,
                          std::function<void(const std::string &, const std::string &)> msg_cb)
{
    if (!m_cluster || !msg_cb)
    {
        return false;
    }
    try
    {
        // Subscriber 与命令连接池隔离, 由同一线程完成 subscribe→consume 全生命周期,
        // 故整个订阅塞进后台线程。空闲超时(3s)继续等下一消息, 其他错误才退出循环。
        m_subThreads.emplace_back(
            [this, channel, cb = std::move(msg_cb)]()
            {
                try
                {
                    sw::redis::Subscriber sub = m_cluster->subscriber();
                    sub.on_message([cb](std::string ch, std::string msg)
                                   { cb(std::move(ch), std::move(msg)); });
                    sub.subscribe(channel);
                    while (m_subRun.load(std::memory_order_relaxed))
                    {
                        try
                        {
                            sub.consume();
                        }
                        catch (const sw::redis::TimeoutError &)
                        {
                            continue; // 无消息的空闲超时, 继续等
                        }
                    }
                }
                catch (const sw::redis::Error &e)
                {
                    XERR("CRedisCli::Subscribe error [channel:%s,error:%s]", channel.c_str(),
                         e.what());
                }
            });
        XINF("CRedisCli::Subscribe [channel:%s] started", channel.c_str());
        return true;
    }
    catch (const sw::redis::Error &e)
    {
        XERR("CRedisCli::Subscribe error [channel:%s,error:%s]", channel.c_str(), e.what());
        return false;
    }
}

void CRedisCli::UnsubscribeAll()
{
    if (m_subThreads.empty())
    {
        return;
    }
    m_subRun.store(false, std::memory_order_relaxed);
    for (auto &t : m_subThreads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    m_subThreads.clear();
    m_subRun.store(true, std::memory_order_relaxed);
}
