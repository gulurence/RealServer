/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xRecordDefine.h
      Purpose:   记录模块类型定义入口

      引入 xBase/xRecordDataDefine.h 提供 RecordDataST 等记录结构
      文件内保留注释掉的订单表字段/列定义示例, 无实际生效代码
  *****************************************************************/

#pragma once

#include "xBase/xDefine.h"
#include "xBase/xRecordDataDefine.h"
//
//// 订单数据库名称
// #define ORDER_DB_TABLE_NAME "t_marketorder"
//
//
// #pragma pack(1)
// struct MarketDBOrderQueryData
//{
//     UInt64 orderid = 0;
//     UInt64 steamid = 0;
//     UInt32 cinfigid = 0;
//     UInt32 state = 0;
//     char createurl[MAX_DBCHAR256_SIZE] = { 0 };
//     char createparam[MAX_DBCHAR256_SIZE] = { 0 };
//     char replyurl[MAX_DBCHAR256_SIZE] = { 0 };
//     char replyparam[MAX_DBCHAR256_SIZE] = { 0 };
//
//     MarketDBOrderQueryData() {
//         CleanUp();
//     }
//     void CleanUp() {
//         bzero(this, sizeof(MarketDBOrderQueryData));
//     }
// }__attribute__((packed));
//// 状态更新
// struct MarketDBOrderStateUpdateData
//{
//     UInt32 state = 0;
//     UInt32 errorcode = 0;
//     UInt32 steamcode = 0;
//     char updatetime[MAX_NAMESIZE] = { 0 };
// }__attribute__((packed));
// #pragma pack()
//
//
// static dbCol MarketOrderUpdateStateCol[] = {
//     {"STATE", DBDATA_TYPE_UInt32, sizeof(UInt32)},
//     {"ERRORCODE", DBDATA_TYPE_UInt32, sizeof(UInt32)},
//     {"STEAMCODE", DBDATA_TYPE_UInt32, sizeof(UInt32)},
//     {"UPDATETIME", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
//     {"NULL", 0, 0},
// };
//
//
