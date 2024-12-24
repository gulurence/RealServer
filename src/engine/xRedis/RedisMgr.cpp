#include "RedisMgr.h"
#include "xBase/xXMLParser.h"
//#include "xSConnect/SStartConfig.h"


RedisNode::RedisNode()
    : _three_sec(3){
}

RedisNode::~RedisNode() {
}

void RedisNode::thread_proc() {
    while (thread_getState() == THREAD_RUN) {
        Sleep(10);
        // 闂佸墽鍋撶敮鏉懳涢埀顒勫蓟?
        if (_three_sec.timeUp()) {

            m_lockOrder.lock();
            CheckConnect();
            m_lockOrder.unlock();
        }
        // 闁兼儳鍢茶ぐ鍥川閹存帗濮㈤柛鎺擃殙閵?
        if (m_pRedisConnect) {
            RunLogic();
        }
    }
}

bool RedisNode::Init(const RedisConfig &stConfig) {
    
    mRedisConfig = stConfig;

    thread_start();

    return true;
}


void RedisNode::CheckConnect() {
    
    if (!m_pRedisConnect) {
        struct timeval timeout = { 1, 500000 };
        m_pRedisConnect = redisConnectWithTimeout(mRedisConfig.ip.c_str(), mRedisConfig.port, timeout);
        if (!m_pRedisConnect || m_pRedisConnect->err) {
            XERR("RedisNode::CheckConnect - !m_pRedisConnect || m_pRedisConnect->err");
            m_pRedisConnect = nullptr;
            return;
        }

        char tempAuth[128] = { 0 };
        sprintf(tempAuth,"AUTH %s", mRedisConfig.authkey.c_str());
        redisReply *reply = (redisReply *)redisCommand(m_pRedisConnect, tempAuth);
        if (reply) {
            if (reply->str && strcmp(reply->str, "OK") == 0) {
                // 濡ょ姴鐭侀惁澶愬箣閹邦剙顫犲ù?闁烩晛鐡ㄧ敮瀛樻交閺傛寧绀€
                XLOG("RedisNode::CheckConnect connect success:[%s,%d] authkey:%s ",
                     mRedisConfig.ip.c_str(),
                     mRedisConfig.port,
                     mRedisConfig.authkey.c_str());

                freeReplyObject(reply);
            } else {
                freeReplyObject(reply);
                redisFree(m_pRedisConnect);
                m_pRedisConnect = nullptr;
            }

        } else {
            XERR("RedisNode::CheckConnect - auth error [authkey:%s]", mRedisConfig.authkey.c_str());
            redisFree(m_pRedisConnect);
            m_pRedisConnect = nullptr;
        }
    }
}


void RedisNode::RunLogic() {

    m_lockOrder.lock();
    for (auto &it: m_mapList[REDIS_LIST_OP_TYPE_POP]) {
        static char tempCmd[128] = { 0 };
        sprintf(tempCmd,"LPOP %s", it.first.c_str());
        redisReply *reply = (redisReply *)redisCommand(m_pRedisConnect, tempCmd);
        if (reply) {
            if (reply->str) {
                XLOG("RedisNode::RunLogic %s", reply->str);
                it.second.push_back(reply->str);
            }
            freeReplyObject(reply);
        }
    }

    for (auto &it : m_mapList[REDIS_LIST_OP_TYPE_PUSH]) {
        ListData::iterator it_1 = it.second.begin();
        for (; it_1!= it.second.end();) {
            ListData::iterator it_1_1 = it_1++;

            redisReply *reply = (redisReply *)redisCommand(m_pRedisConnect, it_1_1->c_str());
            if (reply) {
                // 闁圭粯甯掗崣鍡涘箣閹邦剙顫?
                if (reply->type == REDIS_REPLY_INTEGER
                    && reply->integer >= 0) {
                    it.second.erase(it_1_1);
                } else {
                    // 闂佹潙鐡ㄥ鍫熷緞鏉堫偉袝
                    XLOG("RedisNode::RunLogic error cmd:%s error:%s", it_1_1->c_str(), reply->str);
                    freeReplyObject(reply);
                    m_lockOrder.unlock();
                    return;
                }

                freeReplyObject(reply);
            } else {
                XLOG("RedisNode::RunLogic error %s", it_1_1->c_str());
                m_lockOrder.unlock();
                return;
            }
        }
    }
    m_lockOrder.unlock();
}

void RedisNode::AddPopKey(const std::string &strKey) {
    if (strKey.empty()) {
        XERR("RedisNode::AddPopKey - strKey.empty()");
        return;
    }

    m_lockOrder.lock();
    m_mapList[REDIS_LIST_OP_TYPE_POP][strKey] = ListData();
    m_lockOrder.unlock();
}

int RedisNode::GetListData(const std::string &strKey, std::list<std::string> &orderList) {
    m_lockOrder.lock();
    auto it = m_mapList[REDIS_LIST_OP_TYPE_POP].find(strKey);
    if (it != m_mapList[REDIS_LIST_OP_TYPE_POP].end()) {
        orderList = it->second;
        it->second.clear();
    }
    m_lockOrder.unlock();

    return (int)orderList.size();
}

// 闁轰胶澧楀畵渚€寮ㄩ幆褍寮硆edis闂傚啰鍠庨崹?
bool RedisNode::PushDataToList(const std::string &strKey, const std::string &data) {
    m_lockOrder.lock();

    auto it = m_mapList[REDIS_LIST_OP_TYPE_PUSH].find("push");
    if (it == m_mapList[REDIS_LIST_OP_TYPE_PUSH].end()) {
        m_mapList[REDIS_LIST_OP_TYPE_PUSH]["push"] = ListData();
    }

    char tempStr[20000] = { 0 };
    sprintf(tempStr, "RPUSH %s %s", strKey.c_str(), data.c_str());
    m_mapList[REDIS_LIST_OP_TYPE_PUSH]["push"].push_back(tempStr);

    m_lockOrder.unlock();
    return true;
}


//========================================================================================

RedisMgr::RedisMgr() {
}

RedisMgr::~RedisMgr() {
}

bool RedisMgr::Init() {
    //const ServerNodeFullConfigST &stFullConfig = SServerStartConfig::getMe().getConfig();

    //for (auto it: stFullConfig.redis_config_list) {
    //    if (it.second.redisid <= 0) {
    //        XERR("RedisMgr::Init - error it.second.redisid <= 0 [%s]", it.second.title.c_str());
    //        continue;
    //    }
    //    if (it.second.title.empty()) {
    //        XERR("RedisMgr::Init - error it.second.redisid <= 0 [%d]", it.second.redisid);
    //        continue;
    //    }

    //    XERR("RedisMgr::Init - title:%s,ip:%s,port:%d", it.second.title.c_str(), it.second.ip.c_str(), it.second.port);

    //    RedisConfig stConfig;
    //    stConfig.redisid    = it.second.redisid; // ID 
    //    stConfig.title      = it.second.title;// 闁哄秴娲。浠嬪冀閸ヮ亶鍞?
    //    stConfig.ip         = it.second.ip;
    //    stConfig.port       = it.second.port;
    //    stConfig.user       = it.second.user;
    //    stConfig.authkey    = it.second.authkey;

    //    RedisNode *pNode = new RedisNode();
    //    pNode->Init(stConfig);
    //    m_mapRedis[stConfig.title] = pNode;
    //    // 闁告凹鍨版慨鈺呮焻閺勫繒甯嗙紒鎹愭硶閳?
    //    pNode->thread_start();
    //}
    
    return true;
}

RedisNode *RedisMgr::GetRedisNode(const std::string &strKey) {
    auto it = m_mapRedis.find(strKey);
    if (it!= m_mapRedis.end()) {
        return it->second;
    }

    return nullptr;
}







