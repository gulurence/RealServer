#include "ServerClient.h"


//------------------------------------------------------------------------------------------

Logic2NetSendMsgSwap::Logic2NetSendMsgSwap() {
    // 榛樿涓?00涓妭鐐?
    m_poolSwapMem.Init(SWAP_POOL_NORMAL_COUNT);
}

Logic2NetSendMsgSwap::~Logic2NetSendMsgSwap() {

}

RecordCacheData* Logic2NetSendMsgSwap::GetRecordCacheData() {
    return (RecordCacheData*)m_poolSwapMem.NewObj();
}

void Logic2NetSendMsgSwap::DelRecordCacheData(RecordCacheData* pCacheNode) {
    m_poolSwapMem.DeleteObj(pCacheNode);
}

void Logic2NetSendMsgSwap::AddSend(RecordCacheData* pData) {
    std::lock_guard<std::mutex> guard(m_lockMsgMap);
    mRecordCacheDataList.push_back(pData);
}

void Logic2NetSendMsgSwap::RunSendMsg(xNetProcessor* pNetProcess) {
    RecordCacheDataList stList;
    {
        std::lock_guard<std::mutex> guard(m_lockMsgMap);
        stList = mRecordCacheDataList;
        mRecordCacheDataList.clear();
    }

    for (auto& it : stList) {
        if (pNetProcess) {
            pNetProcess->sendCmd(it->buffer, it->size);
        }

        m_poolSwapMem.DeleteObj(it);
    }

    time_t t = time(nullptr);
    if (t - _pre_show_pool_size > 5) {
        XLOG("ServerClient - m_poolSwapMem size:%d", m_poolSwapMem.GetCount());
        _pre_show_pool_size = t;
    }
}

//---------------------------------------------------------------------------------


ServerClient::ServerClient() {
    pNetProcess = NULL;
    _heartbeat = SERVER_HEARTBEAT;
}

ServerClient::~ServerClient() {
}

bool ServerClient::sendCmd(const void* cmd, unsigned short len) {
    if (cmd && len && pNetProcess) {
        return pNetProcess->sendCmd(cmd, len);
    }
    return false;
}

bool ServerClient::getCmd(unsigned char*& cmd, unsigned short& len) {
    if (pNetProcess) {
        return pNetProcess->getCmd(cmd, len);
    }
    return false;
}

bool ServerClient::popCmd() {
    if (pNetProcess) {
        return pNetProcess->popCmd();
    }
    return false;
}

void ServerClient::put(unsigned char* cmd, unsigned short len) {
    if (pNetProcess) {
        //pNetProcess->put(cmd, len);
    }
}

inline bool ServerClient::get(unsigned char*& cmd, unsigned short& len) {
    if (pNetProcess) {
        //return pNetProcess->get(cmd, len);
    }
    return false;
}

void ServerClient::pop() {
    if (pNetProcess) {
        //pNetProcess->pop();
    }
}

xNetProcessor* ServerClient::getNp() {
    return pNetProcess;
}

const char* ServerClient::getName() {
    if (pNetProcess) { return pNetProcess->m_szName; }
    return "";
}

bool ServerClient::check() {
    _heartbeat--;
    if (_heartbeat <= 0) {
        return false;
    }
    return true;
}

void ServerClient::reset() {
    _heartbeat = SERVER_HEARTBEAT;
}




