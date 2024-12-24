#pragma once

#include "xCommand.h"

#pragma pack(1)

#define SYSTEM_CMD 0
struct SystemCmd : public xCommand
{
    SystemCmd(unsigned char p = 0) :xCommand(SYSTEM_CMD, p) {}//xCommand(SYSTEM_CMD, p){}
};

#define HEART_BEAT_SYSCMD 1
struct HeartBeatSystemCmd : public SystemCmd
{
    HeartBeatSystemCmd() :SystemCmd(HEART_BEAT_SYSCMD) {}
};

#define VERIFY_CONN_SYSCMD 2
struct VerifyConnSystemCmd : public SystemCmd
{
    VerifyConnSystemCmd() :SystemCmd(VERIFY_CONN_SYSCMD) {
    }

    struct server_info
    {
        char sguid[MAX_NAMESIZE] = { 0 };
        uint32 uid = 0;
        // 閾炬帴璇︾粏淇℃伅
        char ip[MAX_NAMESIZE] = { 0 };
        uint32 port = 0;
        char extip[MAX_NAMESIZE] = { 0 };
        uint32 extport = 0;
        uint32 isload = 0;// 鏄惁鏄湇鍔″櫒璐熻浇鑺傜偣
        uint32 mid = 0;
    };

    server_info src_info;
    server_info target_info;
    char vkey[MAX_NAMESIZE] = { 0 };
    unsigned char ret = 1;
};

#pragma pack()



