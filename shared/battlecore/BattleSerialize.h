/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: BattleSerialize.h
      Purpose:   二进制序列化与状态哈希原语

      BinWriter/BinReader 小端字节流读写与 Hasher FNV-1a 64 位累加器
      供 CBattleCore 与 BattleGame 复用, serialize/deserialize/hash 三处字段顺序严格对齐
  *****************************************************************/

// =============================================================================
// BattleSerialize.h — 二进制序列化 / 状态哈希原语 (客户端 / 服务器共享, 零引擎依赖)
//
// 把 BattleCore 快照序列化 (SerializeState/DeserializeState) 与状态一致性哈希
// (HashState) 共用的底层原语抽到这里, 供帧同步引擎 (CBattleCore) 与战斗数据模块
// (BattleGame) 各自复用, 消除两模块的重复实现:
//   - BinWriter / BinReader: 小端字节流写出 / 读出 (与 FNV-1a 哈希字节序对齐)。
//   - Hasher: FNV-1a 64 位累加器 (与 xProcessDefine::Fnv1a64 同源)。
//
// 约定: serialize/deserialize/hash 三处的字段顺序严格对齐 (同一字段列表), 加属性
// 需同步改 Write*/Read*/HashInto 三处 (见 BattleGame.cpp)。
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "FixedPoint.h"

namespace battlecore {

// ---------------------------------------------------------------------------
// 二进制写出 (小端, 跨平台一致)
// ---------------------------------------------------------------------------
struct BinWriter {
    std::string out;
    void u8(uint32_t v)  { out.push_back((char)(v & 0xFF)); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back((char)((v >> (8 * i)) & 0xFF)); }
    void u64(uint64_t v) { for (int i = 0; i < 8; ++i) out.push_back((char)((v >> (8 * i)) & 0xFF)); }
    void i32(int32_t v)  { u32((uint32_t)v); }
    void fx(fx_t v)      { i32(v); }
    void bl(bool b)      { u8(b ? 1 : 0); }
    void bytes(const std::vector<uint8_t>& b) {
        u32((uint32_t)b.size());
        out.insert(out.end(), (const char*)b.data(), (const char*)b.data() + b.size());
    }
};

// ---------------------------------------------------------------------------
// 二进制读出 (小端)
// ---------------------------------------------------------------------------
struct BinReader {
    const unsigned char* p;
    size_t n, pos;
    BinReader(const std::string& s) : p((const unsigned char*)s.data()), n(s.size()), pos(0) {}

    bool u8(uint32_t& v)  { if (pos + 1 > n) return false; v = p[pos++]; return true; }
    bool u32(uint32_t& v) {
        if (pos + 4 > n) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) v |= (uint32_t)p[pos + i] << (8 * i);
        pos += 4;
        return true;
    }
    bool u64(uint64_t& v) {
        if (pos + 8 > n) return false;
        v = 0;
        for (int i = 0; i < 8; ++i) v |= (uint64_t)p[pos + i] << (8 * i);
        pos += 8;
        return true;
    }
    bool i32(int32_t& v)  { uint32_t t; if (!u32(t)) return false; v = (int32_t)t; return true; }
    bool fx(fx_t& v)      { return i32(v); }
    bool bl(bool& v)      { uint32_t t; if (!u8(t)) return false; v = (t != 0); return true; }
    bool bytes(std::vector<uint8_t>& b) {
        uint32_t sz = 0;
        if (!u32(sz) || pos + sz > n) return false;
        b.assign(p + pos, p + pos + sz);
        pos += sz;
        return true;
    }
};

// ---------------------------------------------------------------------------
// FNV-1a 64 位哈希累加器 (与 HashState 字段列表严格同序)
// ---------------------------------------------------------------------------
struct Hasher {
    uint64_t h = 14695981039346656037ULL;

    void mix(uint8_t c)    { h ^= c; h *= 1099511628211ULL; }
    void mix32(uint32_t v) { for (int i = 0; i < 4; ++i) mix((uint8_t)((v >> (8 * i)) & 0xFF)); }
    void mix64(uint64_t v) { for (int i = 0; i < 8; ++i) mix((uint8_t)((v >> (8 * i)) & 0xFF)); }
    void mixFx(fx_t v)     { mix32((uint32_t)v); }

    uint64_t result() const { return h; }
};

} // namespace battlecore
