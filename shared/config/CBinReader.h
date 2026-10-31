/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CBinReader.h
      Purpose:   配置 bin 读取校验与随机访问

      CBinReader 加载 RSCF bin, 校验 magic/版本/content_hash/schema_hash 与各 section 越界
      提供 FindRow 按 id 二分取行、ForEachId 升序遍历、字符串表解析与 RowCursor 顺序解码游标
  *****************************************************************/

// =============================================================================
// CBinReader.h — 配置 bin 读取/校验/随机访问 (纯 C++, 无引擎/无网络/无第三方依赖)
//
// 帧同步内核/游戏框架只在 Init/spawn 时用 Get(id) 把配置值拷进实体字段, tick 内
// 不再碰配置, 避免每次读配置引入非确定性或开销。
// =============================================================================
#pragma once

#include "ConfigFormat.h"

#include <cstdio>
#include <functional>
#include <vector>

namespace rsconfig {

// 行游标: 生成代码按 schema 顺序解码字段用。
struct RowCursor {
    const uint8_t* p;
    const uint8_t* end;
    bool ok;
    RowCursor() : p(nullptr), end(nullptr), ok(false) {}
    RowCursor(const uint8_t* b, uint32_t len) : p(b), end(b + len), ok(b != nullptr) {}
    bool Need(size_t n) {
        if (!ok) return false;
        if ((size_t)(end - p) < n) { ok = false; return false; }
        return true;
    }
    uint32_t U32() { if (!Need(4)) return 0; uint32_t v = GetU32(p); p += 4; return v; }
    int32_t  I32() { return (int32_t)U32(); }
    int64_t  I64() { if (!Need(8)) return 0; uint64_t v = GetU64(p); p += 8; return (int64_t)v; }
    uint8_t  U8()  { if (!Need(1)) return 0; uint8_t v = *p; p += 1; return v; }
    uint16_t U16() { if (!Need(2)) return 0; uint16_t v = (uint16_t)(p[0] | (p[1] << 8)); p += 2; return v; }
    float    F32() { uint32_t v = U32(); float f; std::memcpy(&f, &v, 4); return f; }
};

class CBinReader {
public:
    CBinReader() : m_ok(false) {}

    bool Load(const uint8_t* data, size_t size);
    bool LoadFile(const std::string& path);

    bool     Ok() const { return m_ok; }
    const char* Error() const { return m_err.c_str(); }
    uint32_t Version() const { return m_version; }
    uint64_t ContentHash() const { return m_contentHash; }
    uint64_t SchemaHash() const { return m_schemaHash; }
    uint32_t TableCount() const { return m_tableCount; }

    const char* TableName(uint32_t i) const;
    int         TableIndex(const char* name) const;   // -1 不存在
    // 单表内容哈希 (schema+data+index 三段字节的 FNV-1a; 供 CConfigMgr 逐表热更 diff)
    uint64_t    TableContentHash(uint32_t i) const;
    uint32_t    RowCount(const char* name) const;
    uint32_t    FieldCount(const char* name) const;
    FieldType   FieldTypeAt(const char* name, uint32_t fidx) const;
    const char* FieldNameAt(const char* name, uint32_t fidx) const;

    // 按 id 二分取行原始字节; 不存在返回 nullptr。rowLen 输出该行编码长度。
    const uint8_t* FindRow(const char* name, uint32_t id, uint32_t* rowLen) const;
    // 升序遍历某表所有 id
    void ForEachId(const char* name, const std::function<void(uint32_t)>& fn) const;
    // 解析字符串表偏移 → C 字符串(内部 NUL 结尾), 越界返回 nullptr
    const char* GetString(uint32_t off) const;

private:
    struct TableView {
        uint32_t nameOff = 0;
        uint32_t rowCount = 0;
        uint32_t schemaOff = 0, schemaSize = 0;
        uint32_t dataOff = 0, dataSize = 0;
        uint32_t indexOff = 0, indexSize = 0;
    };

    int TableIndexInternal(const char* name) const;

    bool m_ok = false;
    std::string m_err;
    std::vector<uint8_t> m_buf;   // 持有数据副本
    uint32_t m_version = 0;
    uint64_t m_contentHash = 0, m_schemaHash = 0;
    uint32_t m_tableCount = 0;
    uint32_t m_stringOff = 0, m_stringSize = 0;
    std::vector<TableView> m_tables;
};

// ---------------------------------------------------------------------------

inline bool CBinReader::Load(const uint8_t* data, size_t size) {
    m_ok = false; m_err.clear(); m_buf.clear(); m_tables.clear();
    if (!data || size < kHeaderSize) { m_err = "数据为空或小于 header"; return false; }
    m_buf.assign(data, data + size);
    uint8_t* b = m_buf.data();

    if (b[0] != kMagic0 || b[1] != kMagic1 || b[2] != kMagic2 || b[3] != kMagic3) {
        m_err = "magic 不匹配 (非 RSCF)"; return false;
    }
    m_version = GetU32(b + kOffBinVersion);
    if (m_version != kBinVersion) { m_err = "bin_version 不支持"; return false; }

    uint32_t tc = GetU32(b + kOffTableCount);
    uint64_t storedHash = GetU64(b + kOffContentHash);
    m_schemaHash = GetU64(b + kOffSchemaHash);
    m_stringOff = GetU32(b + kOffStringOff);
    m_stringSize = GetU32(b + kOffStringSize);
    m_tableCount = tc;

    // content_hash 校验 (先把该字段置 0 再算)
    {
        uint8_t saved[8];
        std::memcpy(saved, b + kOffContentHash, 8);
        std::memset(b + kOffContentHash, 0, 8);
        uint64_t ch = Fnv1aRange(b, size);
        std::memcpy(b + kOffContentHash, saved, 8);
        if (ch != storedHash) { m_err = "content_hash 校验失败 (文件损坏)"; return false; }
        m_contentHash = storedHash;
    }

    // 目录 + 越界
    uint32_t dirStart = kHeaderSize;
    if (size < dirStart + (size_t)tc * kDirEntrySize) { m_err = "目录越界"; return false; }
    m_tables.resize(tc);
    for (uint32_t i = 0; i < tc; ++i) {
        const uint8_t* p = b + dirStart + i * kDirEntrySize;
        TableView& v = m_tables[i];
        v.nameOff = GetU32(p);      v.rowCount = GetU32(p + 4);
        v.schemaOff = GetU32(p + 8); v.schemaSize = GetU32(p + 12);
        v.dataOff = GetU32(p + 16);  v.dataSize = GetU32(p + 20);
        v.indexOff = GetU32(p + 24); v.indexSize = GetU32(p + 28);
        if (v.schemaOff + v.schemaSize > size ||
            v.dataOff + v.dataSize > size ||
            v.indexOff + v.indexSize > size) { m_err = "section 越界"; return false; }
    }

    // schema_hash 校验
    {
        uint64_t sh = Fnv1aInit();
        for (uint32_t i = 0; i < tc; ++i)
            for (uint32_t k = 0; k < m_tables[i].schemaSize; ++k) Fnv1aStep(sh, b[m_tables[i].schemaOff + k]);
        if (sh != m_schemaHash) { m_err = "schema_hash 校验失败"; return false; }
    }

    if (m_stringOff + 4 + m_stringSize > size) { m_err = "字符串表越界"; return false; }

    m_ok = true;
    return true;
}

inline bool CBinReader::LoadFile(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { m_err = "打开失败: " + path; m_ok = false; return false; }
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(n > 0 ? (size_t)n : 0);
    if (n > 0) std::fread(buf.data(), 1, (size_t)n, f);
    std::fclose(f);
    return Load(buf.data(), buf.size());
}

inline int CBinReader::TableIndexInternal(const char* name) const {
    for (uint32_t i = 0; i < m_tableCount; ++i) {
        const char* n = GetString(m_tables[i].nameOff);
        if (n && std::strcmp(n, name) == 0) return (int)i;
    }
    return -1;
}

inline int CBinReader::TableIndex(const char* name) const {
    return m_ok ? TableIndexInternal(name) : -1;
}

inline const char* CBinReader::TableName(uint32_t i) const {
    if (!m_ok || i >= m_tableCount) return nullptr;
    return GetString(m_tables[i].nameOff);
}

inline uint64_t CBinReader::TableContentHash(uint32_t i) const {
    if (!m_ok || i >= m_tableCount) return 0;
    const TableView& v = m_tables[i];
    const uint8_t* b = m_buf.data();
    uint64_t h = Fnv1aInit();
    for (uint32_t k = 0; k < v.schemaSize; ++k) Fnv1aStep(h, b[v.schemaOff + k]);
    for (uint32_t k = 0; k < v.dataSize; ++k)    Fnv1aStep(h, b[v.dataOff + k]);
    for (uint32_t k = 0; k < v.indexSize; ++k)   Fnv1aStep(h, b[v.indexOff + k]);
    return h;
}

inline uint32_t CBinReader::RowCount(const char* name) const {
    if (!m_ok) return 0;
    int ti = TableIndexInternal(name);
    return ti < 0 ? 0 : m_tables[ti].rowCount;
}

inline uint32_t CBinReader::FieldCount(const char* name) const {
    if (!m_ok) return 0;
    int ti = TableIndexInternal(name);
    return ti < 0 ? 0 : GetU32(m_buf.data() + m_tables[ti].schemaOff);
}

inline FieldType CBinReader::FieldTypeAt(const char* name, uint32_t fidx) const {
    if (!m_ok) return (FieldType)0;
    int ti = TableIndexInternal(name);
    if (ti < 0) return (FieldType)0;
    const TableView& v = m_tables[ti];
    uint32_t fc = GetU32(m_buf.data() + v.schemaOff);
    if (fidx >= fc) return (FieldType)0;
    return (FieldType)m_buf[v.schemaOff + 4 + fidx * kSchemaFieldSize + 4];
}

inline const char* CBinReader::FieldNameAt(const char* name, uint32_t fidx) const {
    if (!m_ok) return nullptr;
    int ti = TableIndexInternal(name);
    if (ti < 0) return nullptr;
    const TableView& v = m_tables[ti];
    uint32_t fc = GetU32(m_buf.data() + v.schemaOff);
    if (fidx >= fc) return nullptr;
    uint32_t off = GetU32(m_buf.data() + v.schemaOff + 4 + fidx * kSchemaFieldSize);
    return GetString(off);
}

inline const uint8_t* CBinReader::FindRow(const char* name, uint32_t id, uint32_t* rowLen) const {
    if (rowLen) *rowLen = 0;
    if (!m_ok) return nullptr;
    int ti = TableIndexInternal(name);
    if (ti < 0) return nullptr;
    const TableView& v = m_tables[ti];
    const uint8_t* idx = m_buf.data() + v.indexOff;
    uint32_t n = v.indexSize / kIndexEntrySize;
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint32_t midId = GetU32(idx + mid * kIndexEntrySize);
        if (midId == id) {
            uint32_t rel = GetU32(idx + mid * kIndexEntrySize + 4);
            uint32_t len = GetU32(idx + mid * kIndexEntrySize + 8);
            if (rowLen) *rowLen = len;
            return m_buf.data() + v.dataOff + rel;
        }
        if (midId < id) lo = mid + 1; else hi = mid;
    }
    return nullptr;
}

inline void CBinReader::ForEachId(const char* name, const std::function<void(uint32_t)>& fn) const {
    if (!m_ok || !fn) return;
    int ti = TableIndexInternal(name);
    if (ti < 0) return;
    const TableView& v = m_tables[ti];
    const uint8_t* idx = m_buf.data() + v.indexOff;
    uint32_t n = v.indexSize / kIndexEntrySize;
    for (uint32_t i = 0; i < n; ++i) fn(GetU32(idx + i * kIndexEntrySize));
}

inline const char* CBinReader::GetString(uint32_t off) const {
    if (!m_ok || off >= m_stringSize) return nullptr;
    const char* base = (const char*)(m_buf.data() + m_stringOff + 4);
    const char* end = base + m_stringSize;
    for (const char* q = base + off; q < end; ++q) if (*q == 0) return base + off;
    return nullptr;
}

} // namespace rsconfig
