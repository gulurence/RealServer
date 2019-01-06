#pragma once
#include "xCommand.h"


#pragma pack(1)

//old change
#define SYSTEM_CMD 0
struct SystemCmd : public xCommand
{
    SystemCmd(unsigned char p=0):xCommand(SYSTEM_CMD, p){}//xCommand(SYSTEM_CMD, p){}
};

#define HEART_BEAT_SYSCMD 1
struct HeartBeatSystemCmd : public SystemCmd
{
    HeartBeatSystemCmd():SystemCmd(HEART_BEAT_SYSCMD){}
};

#define VERIFY_CONN_SYSCMD 2
struct VerifyConnSystemCmd : public SystemCmd
{
    VerifyConnSystemCmd():SystemCmd(VERIFY_CONN_SYSCMD)
    {
        bzero(type, sizeof(type));
        bzero(name, sizeof(name));
        ret = 1;
        zoneID=0;
    }
    char type[MAX_NAMESIZE];
    char name[MAX_NAMESIZE];
    unsigned char ret;
    UInt32 zoneID;
};

//测试连接
#define TEST_CONN_SYSCMD 3
struct TestConnSystemCmd : public SystemCmd
{
    TestConnSystemCmd():SystemCmd(TEST_CONN_SYSCMD)
    {
        bzero(str, sizeof(str));
    }
    char str[MAX_NAMESIZE];
};
//session->gate 网关心跳
#define CHECK_CONN_SYSCMD 4
struct CheckConnSystemCmd : public SystemCmd
{
    CheckConnSystemCmd():SystemCmd(CHECK_CONN_SYSCMD)
    {
        bzero(gateName, sizeof(gateName));
    }
    char gateName[MAX_NAMESIZE];
};
//检查场景服务器连接
#define CHECK_CONN_SCENE_SYSCMD 5
struct CheckConnSceneSystemCmd : public SystemCmd
{
    CheckConnSceneSystemCmd():SystemCmd(CHECK_CONN_SCENE_SYSCMD)
    {
        bzero(serverName, sizeof(serverName));
    }
    char serverName[MAX_NAMESIZE];
};
////释放数据库ID
#define RE_CONN_DB_SYSCMD 6
struct ReConnDBSystemCmd : public SystemCmd
{
    ReConnDBSystemCmd():SystemCmd(RE_CONN_DB_SYSCMD)
    {
    }
};
//添加帮助系统？
#define ADD_HELPER_SYSCMD 7
struct AddHelperSystemCmd : public SystemCmd
{
    AddHelperSystemCmd():SystemCmd(ADD_HELPER_SYSCMD)
    {
        charid = 0;
        bzero(name, sizeof(name));
        bzero(nickname, sizeof(nickname));
    }
    UInt64 charid;
    char name[MAX_NAMESIZE];
    char nickname[MAX_NAMESIZE];
};
//移除帮助系统?
#define DEL_HELPER_SYSCMD 8
struct DelHelperSystemCmd : public SystemCmd
{
    DelHelperSystemCmd():SystemCmd(DEL_HELPER_SYSCMD)
    {
        charid = 0;
        bzero(name, sizeof(name));
    }
    UInt64 charid;
    char name[MAX_NAMESIZE];
};
#define SERVER_CLOSE_SYSCMD 9//关闭服务器 CenterServer->AllServer
struct ServerCloseSysCmd:public SystemCmd{
    ServerCloseSysCmd():SystemCmd(SERVER_CLOSE_SYSCMD){

    }
};

// 最大的消息长度
const uint32 CONST_SERVER_TRANSFER_MAX_LEN = 65535;
//服务器间的消息透传
#define SERVER_TRANSFER_SYSCMD 10
struct ServerTransferSysCmd :public SystemCmd
{
    ServerTransferSysCmd() :SystemCmd(SERVER_TRANSFER_SYSCMD) {

    }

    uint32 targetServerTypsId = 0;
    uint32 cmdLen = 0;
    char arrData[CONST_SERVER_TRANSFER_MAX_LEN] = {0};

    void clean() {
        cmdLen = 0;
        targetServerTypsId = 0;
    }

    uint32 getLen() {
        if (CONST_SERVER_TRANSFER_MAX_LEN < cmdLen) {
            cmdLen = CONST_SERVER_TRANSFER_MAX_LEN;
        }

        return reinterpret_cast<char *>(arrData + cmdLen) - reinterpret_cast<char *>(this);
    }
};

#pragma pack()
