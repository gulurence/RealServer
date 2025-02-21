#pragma once

#define BUFFER_CMD(send, type) \
    char send##buf_3g[MAX_BUFSIZE];\
    bzero(send##buf_3g, sizeof(send##buf_3g));\
    type *send = (type *)send##buf_3g;\
    constructInPlace<type>(send)

#define BUFFER_CMD_SIZE(send, type, size) \
    char send##buf_3g[MAX_BUFSIZE];\
    bzero(send##buf_3g, sizeof(send##buf_3g));\
    type *send = (type *)send##buf_3g;\
    constructInPlace<type>(send)


// set defult cmd and param
#define DEF_SEND_PB_CMD(msgType) \
msgType send;\
send.set_has_cmd();\
send.set_has_param();


#define PROTO_PARSE_XLOG(msg, bufferStr) {\
if(!msg.ParseFromArray((bufferStr).GetProtoBuffer(),(bufferStr).len)){\
XERR("ParseFromArray error !!! [%s:%d]", __FILE__, __LINE__);}\
}


//
#define TO_BUFFER_SEND(protoCmd,pClient) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer);\
sendProtoTemp___->Init(protoCmd.cmdid().cmd(), protoCmd.param()); \
std::string protoStr___;\
protoCmd.SerializeToString(&protoStr___); \
if(protoStr___.size()>0){sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size());\
if (pClient)pClient->sendCmd(sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen());\
}}

//
#define TO_BUFFER_SEND_EX(protoCmd,pClient,funname) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer);\
sendProtoTemp___->Init((protoCmd).cmdid().cmd(), (protoCmd).param()); \
std::string protoStr___;\
(protoCmd).SerializeToString(&protoStr___); \
if(protoStr___.size()>0){sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size());\
if (pClient)pClient->funname(sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen());\
}}

#define TO_BUFFER_THIS_SEND_EX(protoCmd,pClient,funname) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer);\
sendProtoTemp___->Init((protoCmd).cmdid().cmd(), (protoCmd).param()); \
std::string protoStr___;\
(protoCmd).SerializeToString(&protoStr___); \
if(protoStr___.size()>0){sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size());\
pClient->funname(sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen());\
}}

//
#define TO_BUFFER_SEND_S_EX(sguid,protoCmd,pClient,funname) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer);\
sendProtoTemp___->Init((protoCmd).cmdid().cmd(), (protoCmd).param()); \
std::string protoStr___;\
(protoCmd).SerializeToString(&protoStr___); \
if(protoStr___.size()>0){sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size());\
if (pClient)pClient->funname(sguid, (const char *)sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen());\
}}


// other server send to user client
#define TO_BUFFER_SEND_TO_CLIENT(sguid, uid_, protoCmd) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer); \
sendProtoTemp___->Init((protoCmd).cmdid().cmd(), (protoCmd).param()); \
std::string protoStr___; \
(protoCmd).SerializeToString(&protoStr___); \
if (protoStr___.size() > 0){\
sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size()); \
BUFFER_CMD(pTmpMsg__, MsgToClientCmd); \
pTmpMsg__->uid = uid_;\
pTmpMsg__->msgLen = sendProtoTemp___->GetTotalLen();\
memcpy(pTmpMsg__->msgData, sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen()); \
thisServer->SendMsgToServer(sguid, (const char *)pTmpMsg__, sizeof(MsgToClientCmd)+ sendProtoTemp___->GetTotalLen()); \
}}

//
#define TO_BUFFER_BROADCAST_S_EX(stype,tochild,protoCmd,pClient,funname) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer);\
sendProtoTemp___->Init((protoCmd).cmdid().cmd(), (protoCmd).param()); \
std::string protoStr___;\
(protoCmd).SerializeToString(&protoStr___); \
if(protoStr___.size()>0){sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size());\
if (pClient)pClient->funname(stype, tochild, (const char *)sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen());\
}}

//
#define TO_BUFFER_SEND_TO_FD(fd, protoCmd,pClient,funname) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer);\
sendProtoTemp___->Init((protoCmd).cmdid().cmd(), (protoCmd).param()); \
std::string protoStr___;\
(protoCmd).SerializeToString(&protoStr___); \
if(protoStr___.size()>0){sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size());\
if (pClient)pClient->funname(fd, sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen());\
}}

//
#define TO_BUFFER_SEND_P1(protoCmd,pClient,funname,paramdata) \
{\
BUFFER_CMD(sendProtoTemp___, xCommandSendBuffer);\
sendProtoTemp___->Init((protoCmd).cmdid().cmd(), (protoCmd).param()); \
std::string protoStr___;\
(protoCmd).SerializeToString(&protoStr___); \
if(protoStr___.size()>0){sendProtoTemp___->SetProtoData((char*)protoStr___.c_str(), (UInt32)protoStr___.size());\
if (pClient)pClient->funname(sendProtoTemp___->GetBuffer(), sendProtoTemp___->GetTotalLen(),paramdata);\
}}

//
#define MAKE_PROTO_CMD(msgType,msgName) \
    xCommandSendBuffer *rev = (xCommandSendBuffer*)buf;\
    msgType msgName;\
    PROTO_PARSE_XLOG(msgName,*rev);




