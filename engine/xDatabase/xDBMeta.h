#pragma once

#include "../xBase/xDefine.h"

enum
{
    DBDATA_TYPE_NULL,
    DBDATA_TYPE_CHAR,
    DBDATA_TYPE_UInt8,
    DBDATA_TYPE_INT,
    DBDATA_TYPE_UInt16,
    DBDATA_TYPE_UInt32,
    DBDATA_TYPE_UInt64,
    DBDATA_TYPE_BIN,
    DBDATA_TYPE_BIN2,
};

#define MAX_BIN2_SIZE 1024*10000

struct DBDataBin2
{
    uint32 size = 0;
    uint8 data[0];
};

typedef struct
{
    const char *name;
    int type;
    unsigned int size;
}dbCol;




