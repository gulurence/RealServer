#include "GameServerRunShareMgr.h"
#include "xServerMgrDataDef.h"


bool GameServerRunShareMgr::Init() {
    //     m_bIsInit = m_shareMenMgr.Create(SHARE_MEM_KEY_ID, DEFAULT_NODE_COUNT*DEFAULT_NODE_SIZE);
    //     if (!m_bIsInit) {
    //         XERR("GameServerRunShareMgr::Init - !m_bIsInit  SHARE_MEM_KEY_ID:%d,DEFAULT_NODE_COUNT:%d,DEFAULT_NODE_SIZE:%d", SHARE_MEM_KEY_ID, DEFAULT_NODE_COUNT, DEFAULT_NODE_SIZE);
    //     }
    m_bIsInit = m_shareMenMgr.Attach(SHARE_MEM_KEY_ID, DEFAULT_NODE_COUNT * DEFAULT_NODE_SIZE);
    if (!m_bIsInit) {
        m_bIsInit = m_shareMenMgr.Create(SHARE_MEM_KEY_ID, DEFAULT_NODE_COUNT * DEFAULT_NODE_SIZE);
    }
    if (!m_bIsInit) {
        XERR("GameServerRunShareMgr::Init - !m_bIsInit  SHARE_MEM_KEY_ID:%d,DEFAULT_NODE_COUNT:%d,DEFAULT_NODE_SIZE:%d", SHARE_MEM_KEY_ID, DEFAULT_NODE_COUNT, DEFAULT_NODE_SIZE);
    }
    return m_bIsInit;
}

int GameServerRunShareMgr::GetEmptyNode() {
    for (int i = 0; i < DEFAULT_NODE_COUNT; ++i) {

        bool bFind = false;
        for (auto it : m_mapShareIndex) {
            if (it.second == i) {
                bFind = true;
                break;
            }
        }

        if (!bFind) {
            return i;
        }
    }

    return -1;
}

bool GameServerRunShareMgr::Attach(SM_KEY key, uint32 Size) {
    return m_shareMenMgr.Attach(key, Size);
}

void GameServerRunShareMgr::Clean(SM_KEY key) {
    m_shareMenMgr.Clean(key);
}

void GameServerRunShareMgr::Destroy() {
    m_shareMenMgr.Destory();
}

int GameServerRunShareMgr::GetIDNodeIndex(uint32 sguid) {
    auto it = m_mapShareIndex.find(sguid);
    if (it != m_mapShareIndex.end()) {
        return it->second;
    }

    return -1;
}

int GameServerRunShareMgr::GetIDNodeSGUID(int index) {
    for (auto it : m_mapShareIndex) {
        if (it.second == index) {
            return it.first;
        }
    }

    return 0;
}

uint64 GameServerRunShareMgr::GetSGUIDProcessID(uint32 sguid) {
    const char* pSharePtr = GetNodePtr(sguid);
    if (!pSharePtr) {
        return 0;
    }

    SMemStruct* pSMemStruct = (SMemStruct*)pSharePtr;
    return pSMemStruct->u64PID;
}

void GameServerRunShareMgr::RemoveNode(uint32 sguid) {
    m_mapShareIndex.erase(sguid);
}

int GameServerRunShareMgr::UpdateNodeData(uint32 sguid, const char* sname, const char* nodeData, const int& nodeLen) {
    if (!m_bIsInit) {
        XLOG("GameServerRunShareMgr::UpdateNodeData - !m_bIsInit sguid:%d", sguid);
        return -1;
    }

    if (nodeLen > DEFAULT_NODE_SIZE) {
        XLOG("GameServerRunShareMgr::UpdateNodeData - nodeLen > %d sguid:%d", DEFAULT_NODE_SIZE, sguid);
        return -1;
    }

    int index = -1;
    do {
        // 宸茬粡瀛樺湪浜?
        auto it = m_mapShareIndex.find(sguid);
        if (it != m_mapShareIndex.end()) {
            index = it->second;
            break;
        }

        // 鑾峰彇涓€涓柊鐨勮妭鐐?
        index = GetEmptyNode();
        if (index < 0) {
            XLOG("GameServerRunShareMgr::UpdateNodeData - index < 0  sguid:%d", sguid);
            return -1;
        }

    } while (false);

    if (index >= 0) {
        char* pSharePtr = m_shareMenMgr.GetTypePtr(DEFAULT_NODE_SIZE, index);

        SMemStruct* pSMemStruct = (SMemStruct*)pSharePtr;
        pSMemStruct->u32SGUID = sguid;
        // 涓嶈兘淇敼杩涚▼ID  
        //pSMemStruct->u64PID = 0;
        memcpy(pSMemStruct->szProcessName, sname, UE_MIN(MAX_NAMESIZE - 1, strlen(sname)));
        pSMemStruct->pNodeFullConfigDataLen = nodeLen;
        memcpy(&pSMemStruct->pNodeFullConfigData[0], nodeData, nodeLen);

        m_mapShareIndex[sguid] = index;
    }

    return index;
}

const char* GameServerRunShareMgr::GetNodePtr(int sguid) {
    for (int i = 0; i < DEFAULT_NODE_COUNT; ++i) {
        char* pSharePtr = m_shareMenMgr.GetTypePtr(DEFAULT_NODE_SIZE, i);
        if (pSharePtr) {
            SMemStruct* pSMemStruct = (SMemStruct*)pSharePtr;
            if (sguid == pSMemStruct->u32SGUID) {
                return pSharePtr;
            }
        }
    }

    return nullptr;
}



