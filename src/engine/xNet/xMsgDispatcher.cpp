#include "xMsgDispatcher.h"

#include "../xCommand/xCommand.h"


void MsgDispatcher::RegistProtoCallBack(int msgType, int msgID, CMD_RUN_THREAD_TYPE runThreadType, ::google::protobuf::Message* pMsg, OnProtoMsgCallBack callBack) {
    if (msgType <= 0 || msgID <= 0) {
        XERR("MsgDispatcher::RegistProtoCallBack msgType <= 0 || msgID <= 0 [msgType:%d,msgID:%d]", msgType, msgID);
        return;
    }

    if (runThreadType < 0 || runThreadType >= CMD_RUN_THREAD_MAX) {
        XERR("MsgDispatcher::RegistProtoCallBack runThreadType < 0 || runThreadType >= CMD_RUN_THREAD_MAX [msgType:%d,msgID:%d]", msgType, msgID);
        return;
    }

    auto& idMap = m_mapProtoMsgCallBack[runThreadType][msgType * CMD_ID_PRIFX_ + msgID];
    idMap.first = pMsg;
    idMap.second = callBack;
}

void MsgDispatcher::RegistBinCallBack(int msgType, int msgID, CMD_RUN_THREAD_TYPE runThreadType, OnBinMsgCallBack callBack) {
    if (msgType <= 0 || msgID <= 0) {
        XERR("MsgDispatcher::RegistBinCallBack msgType <= 0 || msgID <= 0 [msgType:%d,msgID:%d]", msgType, msgID);
        return;
    }

    if (runThreadType < 0 || runThreadType >= CMD_RUN_THREAD_MAX) {
        XERR("MsgDispatcher::RegistBinCallBack runThreadType < 0 || runThreadType >= CMD_RUN_THREAD_MAX [msgType:%d,msgID:%d]", msgType, msgID);
        return;
    }

    m_mapBinMsgCallBack[runThreadType][msgType * CMD_ID_PRIFX_ + msgID] = callBack;
}

bool MsgDispatcher::OnMsg(uint32 sguid, CMD_RUN_THREAD_TYPE runThreadType, xNetProcessor* pNetProcess, const unsigned char* buffer, unsigned int len) {
    xCommandSendBuffer* pCmd = (xCommandSendBuffer*)buffer;

    if (runThreadType < 0 || runThreadType >= CMD_RUN_THREAD_MAX) {
        XERR("MsgDispatcher::OnMsg runThreadType < 0 || runThreadType >= CMD_RUN_THREAD_MAX [sguid:%d,runThreadType:%d]", sguid, runThreadType);
        return false;
    }

    //XLOG("MsgDispatcher::OnMsg - [cmd:%d,param:%d,thread:%d,t:%d]", pCmd->cmdID.cmd, pCmd->cmdID.param, runThreadType,(uint32)time(nullptr));

    // 澶у崗璁粦瀹?
    {
        auto idMap1 = m_mapBinMsgCallBack[runThreadType].find(pCmd->cmdID.cmd * CMD_ID_PRIFX_ + ALL_CMD_ID);
        if (idMap1 != m_mapBinMsgCallBack[runThreadType].end()) {
            (idMap1->second)(sguid, pNetProcess, buffer, len);
            return true;
        }
    }

    // 灏忓崗璁粦瀹?
    {
        auto idMap1 = m_mapBinMsgCallBack[runThreadType].find(pCmd->cmdID.cmd * CMD_ID_PRIFX_ + pCmd->cmdID.param);
        if (idMap1 != m_mapBinMsgCallBack[runThreadType].end()) {
            (idMap1->second)(sguid, pNetProcess, buffer, len);
            return true;
        }

        auto idMap = m_mapProtoMsgCallBack[runThreadType].find(pCmd->cmdID.cmd * CMD_ID_PRIFX_ + pCmd->cmdID.param);
        if (idMap == m_mapProtoMsgCallBack[runThreadType].end()) {
            // 杩欓噷鎵句笉鍒版甯?鍥犱负鏈変袱涓嚎绋嬪湪澶勭悊 
            //XERR("MsgDispatcher::OnMsg not find msg listener regist info 銆?pMsg||!pMsg->ParseFromArray(buffer, len)銆?[msgType:%d,msgID:%d]", pCmd->cmdID.cmd, pCmd->cmdID.param);
            return false;
        }

        ::google::protobuf::Message* pMsg = idMap->second.first;
        if (!pMsg || !pMsg->ParseFromArray(pCmd->data, pCmd->len)) {
            XERR("MsgDispatcher::OnMsg 銆?pMsg||!pMsg->ParseFromArray(buffer, len)銆?[msgType:%d,msgID:%d]", pCmd->cmdID.cmd, pCmd->cmdID.param);
            return false;
        }

        (idMap->second.second)(sguid, pNetProcess, pMsg);
        return true;
    }

    return false;
}



