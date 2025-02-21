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
    m_context = redisConnect(m_redisHost, m_redisPort);
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

int CRedisCli::Set(const char* key, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[512] = { 0x00 };
    snprintf(buf, 512, "SET %s %s", key, format);

    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisvCommand(m_context, buf, ap);
    va_end(ap);
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

int CRedisCli::Get(const char* key, char* value, const int &len) {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisCommand(m_context, "GET %s", key);

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

    if (m_reply->type == REDIS_REPLY_STRING) {
        memcpy(value, m_reply->str, (m_reply->len>len)?(len):(m_reply->len));
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

int CRedisCli::Del(const char* key) {
    int ret = REDIS_OK;
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
    return ret;
}

int CRedisCli::HMSet(const char* key, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[512] = { 0x00 };
    snprintf(buf, 512, "HMSet %s %s", key, format);
    // printf("%s\n",buf);
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisvCommand(m_context, buf, ap);
    va_end(ap);

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

int CRedisCli::HMGet(const char* key, size_t* elements, char** element) {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisCommand(m_context, "HGETALL %s", key);
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

    if (m_reply->type == REDIS_REPLY_ARRAY) {
        int i = 0;
        for (i = 0; i < m_reply->elements; i++) {
            strncpy(element[i], m_reply->element[i]->str, m_reply->element[i]->len);
        }
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

int CRedisCli::HSetField(const char* key, const char* field, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[512] = { 0x00 };
    snprintf(buf, 512, "HSet %s %s %s", key, field, format);
    //printf("%s\n",buf);
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisvCommand(m_context, buf, ap);
    va_end(ap);

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

int CRedisCli::HGetField(const char* key, const char* field, char* value) {
    int ret = REDIS_OK;
#ifdef __USE_LOCK__
    pthread_mutex_lock(&m_mutex);
#endif
    m_reply = (redisReply*)redisCommand(m_context, "HGET %s %s", key, field);
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

    if (m_reply->type == REDIS_REPLY_STRING) {
        strncpy(value, m_reply->str, m_reply->len);
    }

    freeReplyObject(m_reply);
    m_reply = NULL;
#ifdef __USE_LOCK__
    pthread_mutex_unlock(&m_mutex);
#endif
    return ret;
}

int CRedisCli::HDel(const char* key) {
    return Del(key);
}

