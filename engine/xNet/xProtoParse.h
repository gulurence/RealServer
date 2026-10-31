/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xProtoParse.h
      Purpose:   客户端帧 protobuf 前导字段最小解析工具。

      namespace netproto 定义 ReadVarint 与 ParseRouteFields，解析 body 前两个 varint 字段 big_type(field 1)/small_type(field 2)
      只做前导字段读取并跳过 length-delimited/fixed32/fixed64 字段，不引入完整 protobuf 反序列化
      供 CNetGateway 与 CNetBattleGateway 共用，协议主体由 Lua 侧业务编解码处理
  *****************************************************************/

#pragma once

#include "xBase/xDefine.h"

// =============================================================================
// xProtoParse.h — 客户端帧 protobuf 前导字段解析 (CNetGateway / CNetBattleGateway 共用)
//
// 客户端帧 body 的前两个字段固定为 big_type(field 1, varint) / small_type(field 2,
// varint), 其余字段按需读取。这里只做「前导字段 + 跳过长度定界字段」的最小解析,
// 不引入完整 protobuf 反序列化 (协议主体由 Lua 侧 si_pb_* / 业务编解码处理)。
// =============================================================================

namespace netproto
{

// 读取一个 protobuf varint (返回 false 表示数据不足/截断)
inline bool ReadVarint(const uint8 *p, size_t len, size_t &pos, uint64 &value)
{
    value = 0;
    int32 shift = 0;
    while (pos < len && shift < 64)
    {
        uint8 b = p[pos++];
        value |= (uint64)(b & 0x7F) << shift;
        if (!(b & 0x80))
        {
            return true;
        }
        shift += 7;
    }
    return false;
}

// 从 protobuf body 解析前两个 varint 字段: big_type(field 1) / small_type(field 2)。
// 只读前导字段, 跳过长度定界字段 (string/token), 不解析完整消息。
inline void ParseRouteFields(const uint8 *body, size_t len, uint32 &bigType, uint32 &smallType)
{
    bigType = 0;
    smallType = 0;
    size_t pos = 0;
    uint64 key = 0;
    uint64 val = 0;
    while (pos < len && ReadVarint(body, len, pos, key))
    {
        uint32 fieldNum = (uint32)(key >> 3);
        uint32 wireType = (uint32)(key & 0x7);
        if (wireType == 0)
        { // varint
            if (!ReadVarint(body, len, pos, val))
            {
                break;
            }
            if (fieldNum == 1)
            {
                bigType = (uint32)val;
            }
            if (fieldNum == 2)
            {
                smallType = (uint32)val;
            }
        }
        else if (wireType == 2)
        { // length-delimited
            if (!ReadVarint(body, len, pos, val))
            {
                break;
            }
            pos += (size_t)val;
            if (pos > len)
            {
                break;
            }
        }
        else if (wireType == 1)
        { // fixed64
            pos += 8;
            if (pos > len)
            {
                break;
            }
        }
        else if (wireType == 5)
        { // fixed32
            pos += 4;
            if (pos > len)
            {
                break;
            }
        }
        else
        {
            break; // 未知 wire type, 停止
        }
    }
}

} // namespace netproto
