#pragma once


#include "xBase/xDefine.h"
#include "xDatabase/xDBMeta.h"

#include "xDatabase/xDatabaseDefine.h"



// 订单数据库名称
#define ORDER_DB_TABLE_NAME "t_marketorder"


#pragma pack(1)
struct MarketDBOrderQueryData : public DatabaseTableCol
{
    uint64 orderid = 0;
    uint64 steamid = 0;
    uint32 cinfigid = 0;
    uint32 state = 0;
    char createurl[MAX_DBCHAR256_SIZE] = { 0 };
    char createparam[MAX_DBCHAR256_SIZE] = { 0 };
    char replyurl[MAX_DBCHAR256_SIZE] = { 0 };
    char replyparam[MAX_DBCHAR256_SIZE] = { 0 };

    MarketDBOrderQueryData() {
        CleanUp();
    }
    void CleanUp() {
        bzero(this, sizeof(MarketDBOrderQueryData));
    }
}__attribute__((packed));
#pragma pack()


// 数据库结构定义
#define AccountDBColDefine() \
static dbCol AccountDBCol[] = {\
    {"STATE", DBDATA_TYPE_UInt32, sizeof(uint32)},\
    {"ERRORCODE", DBDATA_TYPE_UInt32, sizeof(uint32)},\
    {"STEAMCODE", DBDATA_TYPE_UInt32, sizeof(uint32)},\
    {"UPDATETIME", DBDATA_TYPE_CHAR, MAX_NAMESIZE},\
    {"NULL", 0, 0},\
};

