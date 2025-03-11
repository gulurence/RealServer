#include "RedisMgr.h"
#include "xBase/xXMLParser.h"
//#include "xSConnect/SStartConfig.h"


CRedisPool::CRedisPool(const RedisConfigST& cfg) : config_(cfg) {
    for (int i = 0; i < cfg.max_conn / 2; ++i) { // 初始创建半数连接‌:ml-citation{ref="3" data="citationList"}
        CreateConnection();
    }
}

CRedisCli* CRedisPool::CreateConnection() {
    CRedisCli* pCli = new CRedisCli();
    if (pCli->ConnectDB(config_.host.c_str(), config_.port) != 0) {
        delete pCli;
        return nullptr;
    }
    return pCli;
}

CRedisCli* CRedisPool::GetConnection() {
    std::unique_lock<std::mutex> lk(mtx_);

    while (pool_.empty()) {
        if (pool_.size() < config_.max_conn) {
            auto conn = CreateConnection();
            if (conn) pool_.push(conn);
        }
        cv_.wait_for(lk, std::chrono::seconds(config_.retry_interval)); // ‌:ml-citation{ref="8" data="citationList"}
    }

    auto conn = pool_.front();
    pool_.pop();

    if (!Validate(conn)) {       // 失效连接重建‌:ml-citation{ref="2,3" data="citationList"}
        delete conn;
        //redisFree(conn);
        conn = CreateConnection();
    }
    return conn;
}

bool CRedisPool::Validate(CRedisCli* conn) {
    if (!conn || !conn->Validate()) return false;
    return true;
}

void CRedisPool::ReleaseConnection(CRedisCli* conn) {
    std::lock_guard<std::mutex> lk(mtx_);

    if (Validate(conn)) {
        pool_.push(conn);  // 有效连接回池‌:ml-citation{ref="1" data="citationList"}
    } else {
        delete conn;
        //redisFree(conn);    // 失效连接销毁‌:ml-citation{ref="2" data="citationList"}
    }
    cv_.notify_one();
}

