#pragma once

#include "RedisDefine.h"


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

    /*
     * string类
     */
    int Set(const char* key, const char* format, ...);
    int Get(const char* key, char* value, const int& len);
    int Del(const char* key);

    /*
     * 哈希类
     */
    int HMSet(const char* key, const char* format, ...);
    int HMGet(const char* key, size_t* elements, char** element);

    int HSetField(const char* key, const char* field, const char* format, ...);
    int HGetField(const char* key, const char* field, char* value);

    int HDel(const char* key);

private:
#ifdef __USE_LOCK__
    pthread_mutex_t m_mutex;
#endif
    redisContext* m_context;
    redisReply* m_reply;
    char m_redisHost[32];
    int m_redisPort;
    char m_redisPswd[32];
};

typedef std::list<CRedisCli*> RedisCliList;

