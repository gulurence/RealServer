#pragma once

#include "xEventDefine.h"

#include "xNet/xNetDefine.h"
#include "xBase/xSingleton.h"

#include <google/protobuf/message.h>

#include "xScheduler/xScheduler.h"

#include "xEvent/xEvent.h"

#include "xService/xService.h"


#include "xService/xServiceMgr.h"



// 大小协议合并前缀
#define CMD_ID_PRIFX_ 10000
// 监听所有小协议
#define ALL_CMD_ID (CMD_ID_PRIFX_-1)



// 负责协议到上层逻辑的转发
class xEventDispatcher : public xSingleton<xEventDispatcher>
{
public:
    void RegistProtoCallBack(uint32 msgType, uint32 msgID, OnServiceProtoMsgCallBack callBack) {
        uint64 u64MsgID = msgType;
        u64MsgID = (u64MsgID << 32) | msgID;

        if (m_mapSchedulerTaskCallBack.find(u64MsgID) != m_mapSchedulerTaskCallBack.end()) {
            XERR("xEventDispatcher RegistProtoCallBack msg id double regist [%ld] ", u64MsgID);
        }

        m_mapSchedulerTaskCallBack[u64MsgID] = callBack;
    }

public:
    bool OnMsg(uint64 u64SrcServiceID, uint64 u64TargetServiceID , uint32 msgType, uint32 msgID, PBEventPtr ptrEvent) {
        uint64 u64MsgID = msgType;
        u64MsgID = (u64MsgID << 32) | msgID;

        auto *pService = xServiceMgr::getMe().Get(u64TargetServiceID);
        if (!pService) {
            XERR("xEventDispatcher OnMsg service not find [%ld] ", u64TargetServiceID);
            return false;
        }

        auto it  = m_mapSchedulerTaskCallBack.find(u64MsgID);
        if (it == m_mapSchedulerTaskCallBack.end()) {
            XERR("xEventDispatcher OnMsg msg id not find regist [%ld] ", u64MsgID);
            return false;
        }

        OnServiceProtoMsgCallBack pCall = it->second;

        auto pServiceScheduler = pService->GetServiceScheduler();

        //PbMsgPtr ptrRequest;// = std::make_shared<>();
        //PbMsgPtr ptrResponse;// = std::make_shared<>();
        //PBEventPtr ptrEvent = std::make_shared<xPBEvent>(ptrRequest, ptrResponse);

        //pServiceScheduler->postRequest([&]() {xService* pServiceT = pService; PBEventPtr ptrEventT = ptrEvent; return pCall(pServiceT, ptrEventT); /*将response返回给请求段*/});
        pServiceScheduler->postRequest(pService, ptrEvent, pCall);
        return true;
    }

private:
    std::unordered_map<uint64, OnServiceProtoMsgCallBack> m_mapSchedulerTaskCallBack;
};

// 注册协议处理回调 开始
#define REGIST_EVENT_CALLBACK_BEGIN(rname) \
struct RegistEventCallBack_##rname{\
    RegistEventCallBack_##rname() {\

// 注册协议处理回调 注册proto协议 接口
#define REGIST_PROTO_EVENT_CALLBACK(threadType, pbmsg, callback){\
    pbmsg *_msg = new pbmsg();\
    MsgDispatcher::getMe().RegistProtoCallBack(_msg->cmdid().cmd(), _msg->param(), threadType, _msg, callback);}

// 注册协议处理回调 注册二进制协议 接口
#define REGIST_BIN_EVENT_CALLBACK(threadType, cmdid, msgid, callback)\
    MsgDispatcher::getMe().RegistBinCallBack(cmdid, msgid, threadType, callback);

// 注册协议处理回调 注册二进制协议 大协议ID所有协议
#define REGIST_ALLID_BIN_EVENT_CALLBACK(threadType, cmdid, callback)\
    MsgDispatcher::getMe().RegistBinCallBack(cmdid, ALL_CMD_ID, threadType, _msg, callback);

// 注册协议处理回调 结束
#define REGIST_EVENT_CALLBACK_END(rname) \
    }\
};\
RegistCmdCallBack_##rname _temp_reg_##rname;\


