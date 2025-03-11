#include "RedisCli.h"


CRedisCli::CRedisCli() {
    m_context = NULL;
    m_reply = NULL;
    strcpy(m_redisHost, "127.0.0.1");
    m_redisPort = 6379;
    memset(m_redisPswd, 0x00, sizeof(m_redisPswd));
#ifdef __USE_LOCK__
    pthread_mutex_init(&m_mutex, NULL);
#endif
}

CRedisCli::~CRedisCli() {
    if (m_context) {
        redisFree(m_context);
    }
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_destroy(&m_mutex);
#endif
}

int CRedisCli::ConnectDB(const char* hostName, const int port) {
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    strncpy(m_redisHost, hostName, sizeof(m_redisHost) - 1);
    m_redisPort = port;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif

    return ConnectDB();
}

int CRedisCli::ConnectDB() {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    struct timeval tv { conn_timeout, 0 };
    m_context = redisConnectWithTimeout(m_redisHost, m_redisPort, tv);
    if (m_context == NULL || m_context->err) {
        if (m_context) {
            fprintf(stderr, "Connection error: %s\n", m_context->errstr);
            redisFree(m_context);
            m_context = NULL;
        } else {
            fprintf(stderr, "Connection error: can't allocate redis context\n");
        }
        ret = REDIS_ERROR;
    }
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

int CRedisCli::Auth(const char* password) {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif

    strncpy(m_redisPswd, password, sizeof(m_redisPswd) - 1);
    m_reply = (redisReply*)redisCommand(m_context, "auth %s", m_redisPswd);

    if (m_reply == NULL || m_reply->type == REDIS_REPLY_ERROR) {
        if (m_reply) {
            fprintf(stderr, "redis error: %s\n", m_reply->str);
            freeReplyObject(m_reply);
            m_reply = NULL;
        } else {
            fprintf(stderr, "redis error with null m_reply");
        }
#ifdef __USE_LOCK__
        pthread_mutex_unlock(&m_mutex);
#endif
        return REDIS_ERROR;
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

int CRedisCli::SelectDB(int no) {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif

    m_reply = (redisReply*)redisCommand(m_context, "select %d", no);

    if (m_reply == NULL || m_reply->type == REDIS_REPLY_ERROR) {
        if (m_reply) {
            fprintf(stderr, "redis error: %s\n", m_reply->str);
            freeReplyObject(m_reply);
            m_reply = NULL;
        } else {
            fprintf(stderr, "redis error with null m_reply");
        }
#ifdef __USE_LOCK__
        pthread_mutex_unlock(&m_mutex);
#endif
        return REDIS_ERROR;
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

bool CRedisCli::Validate() {
    if (!m_context || m_context->err) return false;

    // PING命令检测心跳‌:ml-citation{ref="3" data="citationList"}
    redisReply* reply = (redisReply*)redisCommand(m_context, "PING");
    bool valid = reply && (reply->type == REDIS_REPLY_STATUS);
    freeReplyObject(reply);
    return valid;
}

int CRedisCli::FlushDB() {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisCommand(m_context, "flushdb");

    if (m_reply == NULL || m_reply->type == REDIS_REPLY_ERROR) {
        if (m_reply) {
            fprintf(stderr, "redis error: %s\n", m_reply->str);
            freeReplyObject(m_reply);
            m_reply = NULL;
        } else {
            fprintf(stderr, "redis error with null m_reply");
        }
#ifdef __USE_LOCK__
        pthread_mutex_unlock(&m_mutex);
#endif
        return REDIS_ERROR;
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

int CRedisCli::FlushAll() {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisCommand(m_context, "flushall");

    if (m_reply == NULL || m_reply->type == REDIS_REPLY_ERROR) {
        if (m_reply) {
            fprintf(stderr, "redis error: %s\n", m_reply->str);
            freeReplyObject(m_reply);
            m_reply = NULL;
        } else {
            fprintf(stderr, "redis error with null m_reply");
        }
#ifdef __USE_LOCK__
        pthread_mutex_unlock(&m_mutex);
#endif
        return REDIS_ERROR;
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

int CRedisCli::Set(const std::string& key, const RecordDataST* data) {
    redisReply* reply = (redisReply*)redisCommand(m_context, "SET %s %b", key.c_str(), data->Data(), data->Size());
    bool success = false;
    if (reply) {
        if (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK") {
            success = true;
        }
        freeReplyObject(reply);
    } else {
        std::cerr << "Error executing command: " << m_context->errstr << std::endl;
    }
    if (success) {
        return REDIS_OK;
    } else {
        return REDIS_ERROR;
    }
}

int CRedisCli::Get(const std::string& key, RecordDataST* data) {
    redisReply* reply = (redisReply*)redisCommand(m_context, "GET %s", key.c_str());
    if (reply) {
        if (reply->type == REDIS_REPLY_STRING) {
            data->Resize(reply->len);
            memcpy(data->MutableData(), reply->str, reply->len);
            data->SetSize(reply->len);
            return REDIS_OK;
        }
        freeReplyObject(reply);
    } else {
        std::cerr << "Error executing command: " << m_context->errstr << std::endl;
    }
    return REDIS_ERROR;
}

int CRedisCli::HSet(const std::string& hash, const std::string& field, const RecordDataST* data) {
    redisReply* reply = (redisReply*)redisCommand(m_context, "HSET %s %s %b", hash.c_str(), field.c_str(), data->Data(), data->Size());
    bool success = false;
    if (reply) {
        if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1) {
            success = true;
        }
        freeReplyObject(reply);
    } else {
        std::cerr << "Error executing command: " << m_context->errstr << std::endl;
    }
    if (success) {
        return REDIS_OK;
    } else {
        return REDIS_ERROR;
    }
}

int CRedisCli::HGet(const std::string& hash, const std::string& field, RecordDataST* data) {
    redisReply* reply = (redisReply*)redisCommand(m_context, "HGET %s %s", hash.c_str(), field.c_str());
    if (reply) {
        if (reply->type == REDIS_REPLY_STRING) {
            data->Resize(reply->len);
            memcpy(data->MutableData(), reply->str, reply->len);
            data->SetSize(reply->len);
            return REDIS_OK;
        }
        freeReplyObject(reply);
    } else {
        std::cerr << "Error executing command: " << m_context->errstr << std::endl;
    }
    return REDIS_ERROR;
}

int CRedisCli::Del(const char* key) {
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisCommand(m_context, "DEL %s", key);

    if (m_reply == NULL || m_reply->type == REDIS_REPLY_ERROR) {
        if (m_reply) {
            fprintf(stderr, "redis error: %s\n", m_reply->str);
            freeReplyObject(m_reply);
            m_reply = NULL;
        } else {
            fprintf(stderr, "redis error with null m_reply");
        }
#ifdef __USE_LOCK__
        pthread_mutex_unlock(&m_mutex);
#endif
        return REDIS_ERROR;
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return REDIS_OK;
}
