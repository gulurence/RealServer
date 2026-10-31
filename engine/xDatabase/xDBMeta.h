/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xDBMeta.h
      Purpose:   数据库列元数据定义

      定义 DBDATA_TYPE_* 枚举（CHAR/INT/UInt8..UInt64/BIN/BIN2）与 DBDataBin2 变长二进制结构
      定义 dbCol 结构（name/type/size）描述表字段布局
  *****************************************************************/

#pragma once

#include "../xBase/xDefine.h"
#include "xDatabaseDefine.h"

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

#define MAX_BIN2_SIZE 1024 * 10000

struct DBDataBin2
{
    uint32 size = 0;
    uint8 data[0];
};

typedef struct
{
    const char *name;
    int32 type;
    uint32 size;
} dbCol;
