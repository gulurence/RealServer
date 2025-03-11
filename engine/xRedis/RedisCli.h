#pragma once

#include "RedisDefine.h"
#include "xBase/xRecordDataDefine.h"

#ifdef __USE_LOCK__
#include <pthread.h>
#endif

#define REDIS_OK 0
#define REDIS_ERROR 0xFFFFFFFF

class CRedisCli
{
public:
    CRedisCli();
    ~CRedisCli();
    /*
     * 连接到redis server
     */
    int ConnectDB(const char* hostName, const int port);
    int ConnectDB();
    int Auth(const char* password);

    /*
     * 系统管理
     */
    int SelectDB(int no);
    int FlushDB();
    int FlushAll();

public:
    int Set(const std::string& key, const RecordDataST* data);
    int Get(const std::string& key, RecordDataST*);

public:
    int HSet(const std::string& hash, const std::string& field, const RecordDataST* data);
    int HGet(const std::string& hash, const std::string& field, RecordDataST*);

public:
    int Del(const char* key);

public:
    bool Validate();

private:
#ifdef __USE_LOCK__
    pthread_mutex_t m_mutex;
#endif
    redisContext* m_context = nullptr;
    redisReply* m_reply = nullptr;
    char m_redisHost[32] = {0};
    int m_redisPort = 0;
    char m_redisPswd[32] = { 0 };
    int conn_timeout = 3;
    int m_redisIndex = 0;
};

typedef std::list<CRedisCli*> RedisCliList;

