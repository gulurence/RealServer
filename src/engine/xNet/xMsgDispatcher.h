#pragma once

#include "../xNet/xNetProcessor.h"
#include "../xBase/xSingleton.h"
#include "../xCommand/xServerRouteDefine.h"

#include "google/protobuf/message.h"


typedef bool(*OnProtoMsgCallBack)(uint32 sguid, xNetProcessor* pNetProcess, const ::google::protobuf::Message*);
typedef bool(*OnBinMsgCallBack)(uint32 sguid, xNetProcessor* pNetProcess, const unsigned char* buff, unsigned int len);


// 澶у皬鍗忚鍚堝苟鍓嶇紑 
#define CMD_ID_PRIFX_ 10000
// 鐩戝惉鎵€鏈夊皬鍗忚 
#define ALL_CMD_ID (CMD_ID_PRIFX_-1)



// 璐熻矗鍗忚鍒颁笂灞傞€昏緫鐨勮浆鍙?
class MsgDispatcher : public xSingleton<MsgDispatcher>
{
public:
    void RegistProtoCallBack(int msgType, int msgID, CMD_RUN_THREAD_TYPE runThreadType, ::google::protobuf::Message*, OnProtoMsgCallBack callBack);
    void RegistBinCallBack(int msgType, int msgID, CMD_RUN_THREAD_TYPE runThreadType, OnBinMsgCallBack callBack);

public:
    bool OnMsg(uint32 sguid, CMD_RUN_THREAD_TYPE runThreadType, xNetProcessor* pNetProcess, const unsigned char* buff, unsigned int len);

private:
    std::map<int, std::pair<::google::protobuf::Message*, OnProtoMsgCallBack>> m_mapProtoMsgCallBack[CMD_RUN_THREAD_MAX];
    std::map<int, OnBinMsgCallBack> m_mapBinMsgCallBack[CMD_RUN_THREAD_MAX];
};


// 娉ㄥ唽鍗忚澶勭悊鍥炶皟 寮€濮?
#define REGIST_MSG_CALLBACK_BEGIN(rname) \
struct RegistCmdCallBack_##rname{\
    RegistCmdCallBack_##rname() {\

// 娉ㄥ唽鍗忚澶勭悊鍥炶皟 娉ㄥ唽proto鍗忚 鎺ュ彛 
#define REGIST_PROTO_MSG_CALLBACK(threadType, pbmsg, callback){\
    pbmsg *_msg = new pbmsg();\
    MsgDispatcher::getMe().RegistProtoCallBack(_msg->cmdid().cmd(), _msg->param(), threadType, _msg, callback);}

// 娉ㄥ唽鍗忚澶勭悊鍥炶皟 娉ㄥ唽浜岃繘鍒跺崗璁?鎺ュ彛 
#define REGIST_BIN_MSG_CALLBACK(threadType, cmdid, msgid, callback)\
    MsgDispatcher::getMe().RegistBinCallBack(cmdid, msgid, threadType, callback);

// 娉ㄥ唽鍗忚澶勭悊鍥炶皟 娉ㄥ唽浜岃繘鍒跺崗璁?澶у崗璁甀D鎵€鏈夊崗璁?
#define REGIST_ALLID_BIN_MSG_CALLBACK(threadType, cmdid, callback)\
    MsgDispatcher::getMe().RegistBinCallBack(cmdid, ALL_CMD_ID, threadType, _msg, callback);

// 娉ㄥ唽鍗忚澶勭悊鍥炶皟 缁撴潫 
#define REGIST_MSG_CALLBACK_END(rname) \
    }\
};\
RegistCmdCallBack_##rname _temp_reg_##rname;\


// 娉ㄥ唽渚嬪瓙 
//bool On_PB_CLIENT_CONECCT_CHECK_REQ_CMD(xNetProcessor *pNetProcess, const ::google::protobuf::Message &stMsg) {
//}
//BEGIN_REGIST_MSG_CALLBACK(PB_CLIENT)
//REGIST_MSG_CALLBACK(PBClientFriendDataUpdateCmd, On_PB_CLIENT_CONECCT_CHECK_REQ_CMD)
//END_REGIST_MSG_CALLBACK(PB_CLIENT)



