// =============================================================================
// CConfigTable.h — 强类型配置表访问 (帧同步内核/游戏框架唯一读取入口)
//
// 用法:
//   CBinReader reader; reader.LoadFile("xxx.bin");
//   Sp01Info info;
//   if (CConfigTable<Sp01Info>(&reader, "sp01").Get(id, info)) { ... }
//
// T 是导出工具生成的结构体(字段与 schema 严格对齐), 需提供:
//   bool Load(const CBinReader& rd, const uint8_t* row, uint32_t len);
// =============================================================================
#pragma once

#include "CBinReader.h"

namespace rsconfig {

template <typename T>
class CConfigTable {
public:
    CConfigTable() : m_reader(nullptr), m_table(nullptr) {}
    CConfigTable(const CBinReader* reader, const char* table) : m_reader(reader), m_table(table) {}

    bool Ok() const { return m_reader && m_reader->Ok() && m_table; }
    uint32_t Size() const { return m_reader ? m_reader->RowCount(m_table) : 0; }

    // 按 id 取一行解码; 成功 true
    bool Get(uint32_t id, T& out) const {
        if (!Ok()) return false;
        uint32_t len = 0;
        const uint8_t* row = m_reader->FindRow(m_table, id, &len);
        if (!row) return false;
        return out.Load(*m_reader, row, len);
    }

    // 升序遍历所有 id
    void ForEachId(const std::function<void(uint32_t)>& fn) const {
        if (Ok()) m_reader->ForEachId(m_table, fn);
    }

private:
    const CBinReader* m_reader;
    const char* m_table;
};

} // namespace rsconfig
