// =============================================================================
// CBinWriter.h — 配置 bin 序列化 (确定性: 同输入 → 逐字节同输出)
//
// 布局:
//   [Header 40] [Directory tableCount*32] [Schema sections] [Data sections]
//   [Index sections] [String table]
//
// content_hash 覆盖整个文件(计算时该字段按 0 处理), 供热更比对/坏包校验;
// schema_hash 覆盖全部 schema section, 供「schema 与数据错配」校验。
// =============================================================================
#pragma once

#include "ConfigFormat.h"

#include <algorithm>
#include <map>
#include <utility>

namespace rsconfig {

// 编码单行: out==nullptr 时只测尺寸(返回字节数), 否则写入并返回字节数。
// 行编码: [id u32][各字段按 schema 顺序]。数组字段 = [len u32][int32 * len]。
inline size_t EncodeRow(const TableData& t, const Row& row,
                        const std::map<std::string, uint32_t>& strOff,
                        uint8_t* out) {
    size_t n = 0;
    uint8_t tmp[8];
    auto put = [&](const uint8_t* b, size_t len) {
        if (out) std::memcpy(out + n, b, len);
        n += len;
    };

    PutU32(tmp, row.id);
    put(tmp, 4);

    // 字段 0 固定为 id, 已由上面的 row.id 写出, 这里从字段 1 开始。
    for (size_t f = 1; f < t.fields.size(); ++f) {
        const FieldDesc& fd = t.fields[f];
        const Cell& c = (f < row.cells.size()) ? row.cells[f] : Cell();
        switch (fd.type) {
            case kTypeI32: case kTypeFx: case kTypeF32:
                PutU32(tmp, (uint32_t)(int32_t)c.i); put(tmp, 4); break;
            case kTypeI64:
                PutU64(tmp, (uint64_t)c.i); put(tmp, 8); break;
            case kTypeStr: {
                uint32_t off = 0;
                std::map<std::string, uint32_t>::const_iterator it = strOff.find(c.s);
                if (it != strOff.end()) off = it->second;
                PutU32(tmp, off); put(tmp, 4); break;
            }
            case kTypeBool: case kTypeU8:
                tmp[0] = (uint8_t)c.i; put(tmp, 1); break;
            case kTypeU16:
                PutU32(tmp, (uint32_t)(uint16_t)c.i); put(tmp, 2); break;
            case kTypeArrI32: case kTypeArrFx:
                PutU32(tmp, (uint32_t)c.a.size()); put(tmp, 4);
                for (size_t k = 0; k < c.a.size(); ++k) { PutU32(tmp, (uint32_t)c.a[k]); put(tmp, 4); }
                break;
            default: break;
        }
    }
    return n;
}

// 把多张表序列化成一个 bin。返回 false 时 err 描述原因。
inline bool BuildBin(const std::vector<TableData>& tables, std::vector<uint8_t>& out, std::string& err) {
    err.clear();
    const uint32_t tc = (uint32_t)tables.size();

    // ---- Pass 1: 字符串表 intern (顺序: 表名 → 字段名 → 数据 str 值) ----
    std::vector<std::string> strList;         // 去重后按首次出现顺序
    std::map<std::string, uint32_t> strOff;   // 字符串 → 偏移(进入字符串字节区)
    uint32_t strBytes = 0;
    struct Intern {
        std::vector<std::string>* list; std::map<std::string, uint32_t>* off; uint32_t* bytes;
        uint32_t operator()(const std::string& s) {
            std::map<std::string, uint32_t>::iterator it = off->find(s);
            if (it != off->end()) return it->second;
            uint32_t o = *bytes;
            (*off)[s] = o;
            list->push_back(s);
            *bytes += (uint32_t)(s.size() + 1);  // +NUL
            return o;
        }
    } intern{&strList, &strOff, &strBytes};

    for (size_t i = 0; i < tables.size(); ++i) intern(tables[i].name);
    for (size_t i = 0; i < tables.size(); ++i)
        for (size_t f = 0; f < tables[i].fields.size(); ++f) intern(tables[i].fields[f].name);
    for (size_t i = 0; i < tables.size(); ++i)
        for (size_t r = 0; r < tables[i].rows.size(); ++r)
            for (size_t f = 0; f < tables[i].rows[r].cells.size() && f < tables[i].fields.size(); ++f)
                if (tables[i].fields[f].type == kTypeStr) intern(tables[i].rows[r].cells[f].s);

    // ---- Pass 2: 逐表编码 schema/data/index + 校验 id 唯一 ----
    struct Enc {
        std::vector<uint8_t> schema;  // 含 field_count u32 前缀
        std::vector<uint8_t> data;
        std::vector<uint8_t> index;
    };
    struct IdxEntry { uint32_t id, rel, len; };

    std::vector<Enc> encs(tc);
    std::vector<uint32_t> schemaOff(tc), schemaSize(tc), dataOff(tc), dataSize(tc),
                          indexOff(tc), indexSize(tc);

    for (uint32_t ti = 0; ti < tc; ++ti) {
        const TableData& t = tables[ti];
        Enc& e = encs[ti];
        uint32_t fc = (uint32_t)t.fields.size();

        e.schema.resize(4 + fc * kSchemaFieldSize);
        PutU32(e.schema.data(), fc);
        for (uint32_t f = 0; f < fc; ++f) {
            uint8_t* p = e.schema.data() + 4 + f * kSchemaFieldSize;
            PutU32(p, strOff[t.fields[f].name]);
            p[4] = (uint8_t)t.fields[f].type; p[5] = 0; p[6] = 0; p[7] = 0;
        }

        std::vector<IdxEntry> idx;
        std::map<uint32_t, bool> seen;
        for (size_t r = 0; r < t.rows.size(); ++r) {
            const Row& row = t.rows[r];
            if (seen.count(row.id)) { err = "表 " + t.name + " id 重复 " + std::to_string(row.id); return false; }
            seen[row.id] = true;
            size_t len = EncodeRow(t, row, strOff, nullptr);
            size_t base = e.data.size();
            IdxEntry ie; ie.id = row.id; ie.rel = (uint32_t)base; ie.len = (uint32_t)len;
            idx.push_back(ie);
            e.data.resize(base + len);
            EncodeRow(t, row, strOff, e.data.data() + base);
        }
        std::sort(idx.begin(), idx.end(),
                  [](const IdxEntry& a, const IdxEntry& b) { return a.id < b.id; });
        e.index.resize(idx.size() * kIndexEntrySize);
        for (size_t k = 0; k < idx.size(); ++k) {
            uint8_t* p = e.index.data() + k * kIndexEntrySize;
            PutU32(p, idx[k].id); PutU32(p + 4, idx[k].rel); PutU32(p + 8, idx[k].len);
        }
    }

    // ---- Pass 3: 计算 section 偏移并组装 ----
    uint32_t cursor = kHeaderSize;
    uint32_t dirStart = cursor; cursor += tc * kDirEntrySize;
    for (uint32_t ti = 0; ti < tc; ++ti) { schemaOff[ti] = cursor; schemaSize[ti] = (uint32_t)encs[ti].schema.size(); cursor += schemaSize[ti]; }
    for (uint32_t ti = 0; ti < tc; ++ti) { dataOff[ti] = cursor; dataSize[ti] = (uint32_t)encs[ti].data.size(); cursor += dataSize[ti]; }
    for (uint32_t ti = 0; ti < tc; ++ti) { indexOff[ti] = cursor; indexSize[ti] = (uint32_t)encs[ti].index.size(); cursor += indexSize[ti]; }
    uint32_t stringOff = cursor;
    uint32_t stringSize = 4 + strBytes;

    uint32_t total = stringOff + stringSize;
    out.assign(total, 0);
    uint8_t* b = out.data();

    b[0] = kMagic0; b[1] = kMagic1; b[2] = kMagic2; b[3] = kMagic3;
    PutU32(b + kOffBinVersion, kBinVersion);
    PutU32(b + kOffFlags, 0);
    PutU64(b + kOffContentHash, 0);   // 最后回写
    PutU32(b + kOffTableCount, tc);
    PutU64(b + kOffSchemaHash, 0);    // 回写
    PutU32(b + kOffStringOff, stringOff);
    PutU32(b + kOffStringSize, strBytes);

    for (uint32_t ti = 0; ti < tc; ++ti) {
        uint8_t* p = b + dirStart + ti * kDirEntrySize;
        PutU32(p, strOff[tables[ti].name]);
        PutU32(p + 4, (uint32_t)tables[ti].rows.size());
        PutU32(p + 8, schemaOff[ti]); PutU32(p + 12, schemaSize[ti]);
        PutU32(p + 16, dataOff[ti]);  PutU32(p + 20, dataSize[ti]);
        PutU32(p + 24, indexOff[ti]); PutU32(p + 28, indexSize[ti]);
    }
    for (uint32_t ti = 0; ti < tc; ++ti) {
        std::memcpy(b + schemaOff[ti], encs[ti].schema.data(), encs[ti].schema.size());
        std::memcpy(b + dataOff[ti], encs[ti].data.data(), encs[ti].data.size());
        std::memcpy(b + indexOff[ti], encs[ti].index.data(), encs[ti].index.size());
    }
    PutU32(b + stringOff, strBytes);
    uint32_t off = stringOff + 4;
    for (size_t i = 0; i < strList.size(); ++i) {
        std::memcpy(b + off, strList[i].data(), strList[i].size());
        b[off + strList[i].size()] = 0;
        off += (uint32_t)(strList[i].size() + 1);
    }

    // schema_hash (schema sections 拼接的 FNV-1a)
    uint64_t sh = Fnv1aInit();
    for (uint32_t ti = 0; ti < tc; ++ti)
        for (size_t i = 0; i < encs[ti].schema.size(); ++i) Fnv1aStep(sh, encs[ti].schema[i]);
    PutU64(b + kOffSchemaHash, sh);

    // content_hash (整个文件, content_hash 字段此时为 0)
    uint64_t ch = Fnv1aRange(b, out.size());
    PutU64(b + kOffContentHash, ch);

    return true;
}

} // namespace rsconfig
