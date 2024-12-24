#pragma once

#include "hiredis/hiredis.h"
#include "xBase/xThread.h"
#include "xBase/xTime.h"
#include "xBase/xSingleton.h"


struct RedisConfig
{
    uint32 redisid = 0; // ID 
    std::string title;
    std::string ip;
    uint32 port = 0;
    std::string user;
    std::string authkey;

    const RedisConfig& operator=(const RedisConfig& in) {
        if (this == &in) {
            return *this;
        }

        redisid = in.redisid;
        title = in.title;
        ip = in.ip;
        port = in.port;
        user = in.user;
        authkey = in.authkey;

        return *this;
    }
};

typedef std::list<std::string> ListData;
typedef std::map<std::string, ListData> ListDataMap;
typedef std::map<std::string, RedisConfig> RedisConfigMap;

enum REDIS_LIST_OP_TYPE
{
    REDIS_LIST_OP_TYPE_POP = 0,
    REDIS_LIST_OP_TYPE_PUSH = 1,
    REDIS_LIST_OP_TYPE_MAX,
};


class RedisNode : public xThread
{
public:
    RedisNode();
    ~RedisNode();

private:
    xTimer _three_sec;
    redisContext* m_pRedisConnect = nullptr;
    RedisConfig mRedisConfig;

private:
    std::mutex m_lockOrder;
    ListDataMap m_mapList[REDIS_LIST_OP_TYPE_MAX];

private:
    virtual void thread_proc();

public:
    RedisConfig& getPlateformOrderConfig() { return mRedisConfig; }

public:
    bool Init(const RedisConfig& stConfig);
    void CheckConnect();
    void RunLogic();

public:
    void AddPopKey(const std::string& strKey);

public:
    int GetListData(const std::string& strKey, std::list<std::string>& orderList);
    bool PushDataToList(const std::string& strKey, const std::string& data);
};
typedef std::map<std::string, RedisNode*> REDIS_NODE_MAP;


class RedisMgr : public xSingleton<RedisMgr>
{
public:
    RedisMgr();
    ~RedisMgr();

private:
    REDIS_NODE_MAP m_mapRedis;

public:
    bool Init();

public:
    RedisNode* GetRedisNode(const std::string& strKey);
};



