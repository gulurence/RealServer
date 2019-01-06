#include "xBalanceMgr.h"
#include "xNetProcessor.h"


CBalanceMgr::CBalanceMgr() {
}


CBalanceMgr::~CBalanceMgr() {
}

bool CBalanceMgr::addBalanceNode(BalanceNodeST *pNode) {
    if (nullptr == pNode|| pNode->strServerName.empty() ||pNode->strServerType.empty()) {
        return false;
    }

    auto it = m_mapBalance.find(pNode->strServerType);
    if (it != m_mapBalance.end()) {
        if (it->second.find(pNode->strServerType) != it->second.end()) {
            return false;
        }
    }
    
    m_mapBalance[pNode->strServerType][pNode->strServerName] = pNode;
    
    return true;
}

bool CBalanceMgr::removeBalanceNode(void *pNetProcess) {
    if (nullptr == pNetProcess) {
        return false;
    }

    xNetProcessor *pNetProcessor = static_cast<xNetProcessor*>(pNetProcess);
    if (nullptr == pNetProcessor) {
        return false;
    }

    for (auto it: m_mapBalance) {
        auto it_f = it.second.find(pNetProcessor->name);
        if (it_f != it.second.end()) {

            BalanceNodeMap *pNode = (BalanceNodeMap *)it_f->second;

            it.second.erase(pNetProcessor->name);

            if (nullptr != pNode) {
                delete pNode;
                pNode = nullptr;
            }

            return true;
        }
    }

    return false;
}

