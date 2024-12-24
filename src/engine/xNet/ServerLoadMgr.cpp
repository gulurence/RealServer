#include "ServerLoadMgr.h"
//#include "../xServer/ZoneServer.h"



ServerLoadMgr::ServerLoadMgr() : m_stTimer(5) {
}

ServerLoadMgr::~ServerLoadMgr() {
}

bool ServerLoadMgr::Init(ZoneServer *pServer) {
    if (m_pServer) {
        return true;
    }

    m_pServer = pServer;

    // 鍚姩绾跨▼
    thread_start();

    return true;
}

uint32 ServerLoadMgr::GetServerNodeCount(uint32 sid) {
    auto it = m_mapLoadNodeType.find(sid);
    if (it == m_mapLoadNodeType.end()) {
        return 0;
    }

    return it->second.size();
}

uint32 ServerLoadMgr::GetNiceLoadSGUID(SERVER_ROUTE_SID_DEF sid) {
    auto it = m_mapLoadNodeType.find(sid);
    if (it == m_mapLoadNodeType.end()) {
        return 0;
    }

    int sguid = 0;
    LoadNode *pTNode = nullptr;
    int cur_load_value = 0xFFFFFFF;
    for (auto &it_t : it->second) {

        SGUID_UINT32 stSGUID(it_t.first);

        // 涓績鑺傜偣涓嶇敤绠?
        //if (SServerStartConfig::getMe().isLoadMgr()
        //    && (uint32)cur_load_value > it_t.second.cur_load_value) {
        //    cur_load_value = it_t.second.cur_load_value;
        //    sguid = it_t.second.sguid.getSGUID();
        //    pTNode = &it_t.second;
        //}
    }

    if (pTNode && pTNode->cur_load_value >= pTNode->full_load_value) {
        return 0;
    }

    return sguid;
}

uint32 ServerLoadMgr::GetNiceLoadSGUIDByGID(uint32 sid, uint32 gid) {
    auto it = m_mapLoadNodeType.find(sid);
    if (it == m_mapLoadNodeType.end()) {
        return 0;
    }

    int sguid = 0;
    LoadNode *pTNode = nullptr;
    int cur_load_value = 0xFFFFFFF;
    for (auto &it_t : it->second) {

        SGUID_UINT32 stSGUID(it_t.first);
        
        // 涓績鑺傜偣涓嶇敤绠?
        if (gid == stSGUID.getGroupID()
            && stSGUID.getNodeID() > 0
            && (uint32)cur_load_value > it_t.second.cur_load_value) {
            cur_load_value = it_t.second.cur_load_value;
            sguid = it_t.second.sguid.getSGUID();
            pTNode = &it_t.second;
        }
    }

    if (pTNode && pTNode->cur_load_value >= pTNode->full_load_value) {
        return 0;
    }

    return sguid;
}

LoadNode &ServerLoadMgr::AddSLoad(uint32 sguid) {
    SGUID_UINT32 stSGUID(sguid);

    if (stSGUID.getServerID() > 0
        && m_mapLoadNodeType.find(stSGUID.getServerID()) == m_mapLoadNodeType.end()) {
        m_mapLoadNodeType[stSGUID.getServerID()] = LoadNodeMap();
    }

    if (m_mapLoadNodeType[stSGUID.getServerID()].find(sguid) == m_mapLoadNodeType[stSGUID.getServerID()].end()) {
        m_mapLoadNodeType[stSGUID.getServerID()][sguid] = LoadNode();
    }

    return m_mapLoadNodeType[stSGUID.getServerID()][sguid];
}

void ServerLoadMgr::ReqLoadInfo(uint32 sguid) {
    //ServerLoadInfoReqCmd sendMsg;
    //sendMsg.sguid = SServerStartConfig::getMe().GetServerSGUID();
    //if (m_pServer) {
    //    m_pServer->sendCmdToServer(sguid, (char*)&sendMsg, sizeof(ServerLoadInfoReqCmd));
    //}
}

void ServerLoadMgr::UpdateNode(const ServerLoadInfoRspCmd *pMsg) {

    for (int i = 0; i < pMsg->nodeCount; ++i) {

        SGUID_UINT32 stSGUID = pMsg->listData[i].sguid;

        LoadNode stNode;
        stNode.pre_update_tick = time(nullptr);
        // 鏈嶅姟鍣╯guid
        stNode.sguid = pMsg->listData[i].sguid;
        // 褰撳墠鎵胯浇鏁伴噺
        stNode.cur_load_value = pMsg->listData[i].cur_load_value;
        // 璀﹀憡 鎵胯浇鏁伴噺
        stNode.warning_load_value = pMsg->listData[i].warning_load_value;
        stNode.full_load_value = pMsg->listData[i].full_load_value;

        XLOG("ServerLoadMgr::UpdateNode [%d,%d,%d,%d],cur:%d,warning:%d,full:%d",
             stNode.sguid.getSGUID(),
             stNode.sguid.getServerID(),
             stNode.sguid.getGroupID(),
             stNode.sguid.getNodeID(),
             stNode.cur_load_value,
             stNode.warning_load_value,
             stNode.full_load_value);

        m_mapLoadNodeType[stSGUID.getServerID()][pMsg->listData[i].sguid.getSGUID()] = stNode;
    }
}

void ServerLoadMgr::thread_proc() {
    while (thread_getState() == xThread::THREAD_RUN) {
#ifdef _WIN32
        Sleep(5);
#else
        Sleep(10);
#endif

        if (m_stTimer.timeUp()) {
            // 閬嶅巻璇锋眰璐熻浇淇℃伅
            for (auto &it : m_mapLoadNodeType) {
                for (auto &it_t : it.second) {
                    ReqLoadInfo(it_t.first);
                }
            }
        }

    }
}







