#include "xServerDispatcher.h"
#include "../xNet/xNetDefine.h"
#include "../xCommand/xServerRouteDefine.h"
#include "../xSconnect/SStartConfig.h"
#include "../xSconnect/xMsgDispatcher.h"



//ZoneServer* xServerDispatcher::m_pZoneServer = nullptr;
//
//ServerClient* xServerDispatcher::GetNetConnect(const uint32& targetSUID) {
//    const ServerClientUIDMap& stServerClientUIDMap = m_pZoneServer->GetServerClientMap();
//
//    {
//        // 鐩存帴鏌ユ壘鐩爣鏈嶅姟鍣?
//        auto it = stServerClientUIDMap.find(targetSUID);
//        if (it != stServerClientUIDMap.end()) {
//            return it->second;
//        }
//    }
//
//    SGUID_UINT32* pTargetNode = (SGUID_UINT32*)&targetSUID;
//    // 濡傛灉鏄潪涓棿鏈嶅姟鍣?璇曠潃鎵句粬鐨勪腑蹇冩湇鍔″櫒
//    if (!pTargetNode->isSCenter()) {
//        // 鏌ユ壘闆嗙兢鏍稿績
//        for (auto& it : stServerClientUIDMap) {
//            SGUID_UINT32* pNode = (SGUID_UINT32*)&it.first;
//            if (pTargetNode->getServerID() == pNode->getServerID()
//                && pNode->isSCenter()) {
//                return it.second;
//            }
//        }
//    }
//
//    {
//        // 鍙戠粰鑷繁鐨?涓績鑺傜偣
//        SGUID_UINT32 tempUID(SServerStartConfig::getMe().GetServerSGUID());
//        if (!tempUID.isSCenter()) {
//            tempUID.setSid(CENTER_SERVER_BASE_SID | tempUID.getServerID());
//            auto it = stServerClientUIDMap.find(tempUID.getSGUID());
//            if (it != stServerClientUIDMap.end()) {
//                return it->second;
//            }
//        }
//    }
//
//    return nullptr;
//}

//int xServerDispatcher::GetNetConnectListSameAsSType(const SERVER_ROUTE_SID_DEF& targetType, bool bNeedCenter, std::list<ServerClient*>& outList) {
//    outList.clear();
//
//    const ServerClientUIDMap& stServerClientUIDMap = m_pZoneServer->GetServerClientMap();
//    // 鏌ユ壘闆嗙兢鏍稿績
//    for (auto& it : stServerClientUIDMap) {
//        SGUID_UINT32* pNode = (SGUID_UINT32*)&it.first;
//        if (targetType == pNode->getServerID()
//            || targetType == (pNode->getServerID() % CENTER_SERVER_BASE_SID)) {
//            // 涓嶉渶瑕佷腑蹇冭妭鐐?
//            if (!bNeedCenter
//                && (pNode->getNodeID() == 0 || pNode->getServerID() > CENTER_SERVER_BASE_SID)) {
//                continue;
//            }
//
//            outList.push_back(it.second);
//        }
//    }
//
//    return outList.size();
//}

// 鍙戦€佸埌鐩爣鏈嶅姟鍣?
bool xServerDispatcher::SendMsgToServer(uint32 targetSUID, const char* buffer, int len) {
    //ServerClient* pServerClient = GetNetConnect(targetSUID);
    //if (pServerClient) {
    //    if (pServerClient->pNetProcess) {
    //        BUFFER_CMD(msg, xServerDispatcherMsg);
    //        // 鍗忚ID
    //        msg->cmdID.cmd = MSG_DISPATCHER_CMD;
    //        msg->cmdID.param = 0;
    //        // 璁剧疆褰撳墠涓€绾?璺敱淇℃伅
    //        msg->stRouteInfo.AddPreNode(SServerStartConfig::getMe().GetServerSGUID());
    //        msg->stRouteInfo.stSrcSGUID = SServerStartConfig::getMe().GetServerSGUID();
    //        msg->stRouteInfo.stTargetSGUID = targetSUID;
    //        // 杩欓噷鏄疨rotoBuff String 鏁版嵁
    //        msg->msg_len = len;
    //        memcpy(msg->msg_data, buffer, len);
    //        pServerClient->sendCmd(msg, sizeof(xServerDispatcherMsg) + len);
    //        return true;
    //    } else {
    //        XERR("xServerDispatcher::SendMsg GetNetConnect pNetProcess is null [%d,%d,%d] !!!",
    //            SGUID_UINT32::ToSGUID(targetSUID).getServerID(),
    //            SGUID_UINT32::ToSGUID(targetSUID).getGroupID(),
    //            SGUID_UINT32::ToSGUID(targetSUID).getNodeID());
    //    }
    //} else {
    //    BUFFER_CMD(msg, xServerDispatcherMsg);
    //    // 鍗忚ID
    //    msg->cmdID.cmd = MSG_DISPATCHER_CMD;
    //    msg->cmdID.param = 0;
    //    // 璁剧疆褰撳墠涓€绾?璺敱淇℃伅
    //    if (!msg->stRouteInfo.AddPreNode(SServerStartConfig::getMe().GetServerSGUID())) {
    //        XERR("xServerDispatcher::SendMsgToServer - AddPreNode error [targetSUID:%d] ", targetSUID);
    //        return true;
    //    }
    //    msg->stRouteInfo.stSrcSGUID = SServerStartConfig::getMe().GetServerSGUID();
    //    msg->stRouteInfo.stTargetSGUID = targetSUID;
    //    // 杩欓噷鏄疨rotoBuff String 鏁版嵁
    //    msg->msg_len = len;
    //    memcpy(msg->msg_data, buffer, len);

    //    std::list<ServerClient*> outList;
    //    if (GetNetConnectListSameAsSType((SERVER_ROUTE_SID_DEF)(SGUID_UINT32::ToSGUID(targetSUID).getServerID()), true, outList) > 0) {
    //        for (auto it : outList) {
    //            it->sendCmd((char*)msg, sizeof(xServerDispatcherMsg) + len);
    //        }
    //    } else {
    //        BroadcastMsgToServer((char*)msg, sizeof(xServerDispatcherMsg) + len, 0);
    //    }
    //}

    return false;
}

bool xServerDispatcher::BroadcastMsgToServer(SERVER_ROUTE_SID_DEF targetSType, bool bToChild, const char* buffer, int len, xNetProcessor* ignoreProcess) {

    BUFFER_CMD(msg, xServerDispatcherMsg);
    // 鍗忚ID
    msg->cmdID.cmd = MSG_DISPATCHER_CMD;
    msg->cmdID.param = 0;
    msg->stRouteInfo.cIsBroadCast = 1;
    // 鍙戦€佺粰鐩磋繛鑺傜偣杩樻槸缁欎簩绾у瓙鑺傜偣
    msg->stRouteInfo.cIsToChiled = bToChild ? 1 : 0;
    // 璁剧疆褰撳墠涓€绾?璺敱淇℃伅
    if (!msg->stRouteInfo.AddPreNode(SServerStartConfig::getMe().GetServerSGUID())) {
        XERR("xServerDispatcher::SendMsgToServer - AddPreNode error [targetSType:%d] ", targetSType);
        return true;
    }
    msg->stRouteInfo.stSrcSGUID = SServerStartConfig::getMe().GetServerSGUID();
    msg->stRouteInfo.stTargetSGUID.setSid(targetSType);
    // 杩欓噷鏄疨rotoBuff String 鏁版嵁
    msg->msg_len = len;
    memcpy(msg->msg_data, buffer, len);


    //std::list<ServerClient*> outList;
    //if (GetNetConnectListSameAsSType(targetSType, true, outList) > 0) {
    //    for (auto it : outList) {
    //        it->sendCmd((char*)msg, sizeof(xServerDispatcherMsg) + len);
    //    }
    //} else {
    //    BroadcastMsgToServer((char*)msg, sizeof(xServerDispatcherMsg) + len, nullptr);
    //}

    return false;
}

bool xServerDispatcher::BroadcastMsgToServer(const char* buffer, int len, const RouteInfo* preInfo) {
    //const ServerClientUIDMap& stServerClientUIDMap = m_pZoneServer->GetServerClientMap();

    //for (auto& it : stServerClientUIDMap) {
    //    SGUID_UINT32* pNode = (SGUID_UINT32*)&it.first;

    //    // SLauncher/SManager 涓嶈兘鍋氬箍鎾彂閫?浼氬鍙戜竴娆″崗璁?
    //    if (pNode->getServerID() == MECHINE_SMANAGER_SID
    //        || pNode->getServerID() == MECHINE_SLAUNCHER_SID) {
    //        continue;
    //    }

    //    bool isPreNode = false;
    //    for (int i = 0; i < ROUTE_NODE_LEVEL_NORMAL; ++i) {
    //        if (preInfo && preInfo->stPreSGUID[i].getSGUID() == pNode->getSGUID()) {
    //            isPreNode = true;
    //            break;
    //        }
    //    }

    //    if (!isPreNode && it.second) {
    //        it.second->sendCmd(buffer, len);
    //    }
    //}

    return true;
}

bool xServerDispatcher::OnMsg(CMD_RUN_THREAD_TYPE runThreadType, xNetProcessor* pNetProcess, const char* buffer, int len) {
    //xServerDispatcherMsg* pMsg = (xServerDispatcherMsg*)buffer;

    //xCommand* cmdID = (xCommand*)pMsg->GetMsgBuffer();

    //XLOG("xServerDispatcher::OnMsg - cmd:%d,param:%d,len:%d", cmdID->cmd, cmdID->param, len);

    //do {
    //    // 骞挎挱鐨勪笖鏄湰鏈嶅姟鍣ㄧ殑绫诲瀷
    //    if (pMsg->stRouteInfo.cIsBroadCast) {
    //        if (pMsg->stRouteInfo.stTargetSGUID.getServerID() == SServerStartConfig::getMe().GetServerSID()
    //            && ((pMsg->stRouteInfo.cIsToChiled && SServerStartConfig::getMe().GetServerNodeID() > 0)
    //                || (!pMsg->stRouteInfo.cIsToChiled && SServerStartConfig::getMe().GetServerNodeID() == 0))) {
    //            return MsgDispatcher::getMe().OnMsg(pMsg->stRouteInfo.stSrcSGUID.getSGUID(), runThreadType, pNetProcess, (const unsigned char*)pMsg->msg_data, pMsg->msg_len);
    //        } else {
    //            // 鏈夐摼鎺ョ殑涓嬬骇鐩存帴鍙戦€?
    //            std::list<ServerClient*> outList;
    //            if (GetNetConnectListSameAsSType((SERVER_ROUTE_SID_DEF)pMsg->stRouteInfo.stTargetSGUID.getServerID(), true, outList) > 0) {
    //                for (auto it : outList) {
    //                    it->sendCmd(buffer, len);
    //                }
    //                return true;
    //            }

    //            // 鐩存帴杩涘叆骞挎挱
    //            break;
    //        }
    //    }

    //    // 闈炶礋杞界被鍨?鍜?骞挎挱绫诲瀷 灏辨槸绮剧‘鐨凷GUID绫诲瀷
    //    if (SServerStartConfig::getMe().GetServerSGUID() == pMsg->stRouteInfo.stTargetSGUID.getSGUID()) {
    //        return MsgDispatcher::getMe().OnMsg(pMsg->stRouteInfo.stSrcSGUID.getSGUID(), runThreadType, pNetProcess, (const unsigned char*)pMsg->msg_data, pMsg->msg_len);
    //    }

    //    // 濡傛灉鑳界洿鎺ユ嬁鍒颁笅绾у氨涓嶅仛骞挎挱浜?
    //    ServerClient* pServerClient = GetNetConnect(pMsg->stRouteInfo.stTargetSGUID.getSGUID());
    //    if (pServerClient) {
    //        if (pServerClient->pNetProcess && pServerClient->pNetProcess != pNetProcess) {
    //            pServerClient->sendCmd(buffer, len);
    //        }

    //        return true;
    //    }

    //} while (false);

    //// 璺敱璁℃暟鍣?鍑忓皯 1
    //pMsg->stRouteInfo.cNodeLevel--;

    //// 璺敱鐢熷懡缁撴潫
    //if (pMsg->stRouteInfo.cNodeLevel <= 0) {
    //    XERR("xServerDispatcher::OnMsg pMsg->stRouteInfo.cNodeLevel <= 0 [%d,%d,%d] !!!",
    //        pMsg->stRouteInfo.stTargetSGUID.getServerID(),
    //        pMsg->stRouteInfo.stTargetSGUID.getGroupID(),
    //        pMsg->stRouteInfo.stTargetSGUID.getNodeID());
    //    return true;
    //}


    //if (!pMsg->stRouteInfo.AddPreNode(SServerStartConfig::getMe().GetServerSGUID())) {
    //    XERR("xServerDispatcher::OnMsg - AddPreNode is error [stTargetSGUID:%d] ", pMsg->stRouteInfo.stTargetSGUID.getSGUID());
    //    return true;
    //}

    //// continue
    //std::list<ServerClient*> outList;
    //if (GetNetConnectListSameAsSType((SERVER_ROUTE_SID_DEF)pMsg->stRouteInfo.stTargetSGUID.getServerID(), true, outList) > 0) {
    //    for (auto it : outList) {
    //        it->sendCmd(buffer, len);
    //    }
    //} else {
    //    BroadcastMsgToServer(buffer, len, &pMsg->stRouteInfo);
    //}

    return true;
}



