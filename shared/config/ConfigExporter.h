/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: ConfigExporter.h
      Purpose:   Excel TSV 导出为 bin 与结构体头

      ParseTsv 解析 TSV(字段名/类型/注释/数据)为 TableData, fx 字段复用 FxFromFloat 保持导出与运行时一致
      ExportTables 打包 bin, GenerateHeader 生成强类型结构体头(生成物勿手改)
  *****************************************************************/

// =============================================================================
// ConfigExporter.h — Excel(TSV 导出) → bin + 生成 C++ 结构体头
//
// 策划编辑 Excel → 「另存为 制表符分隔文本 (.txt)」→ 导出工具读 TSV 打包成 bin。
// TSV 前三行: 字段名 / 类型 / 注释; 第 4 行起数据; 首列固定 id。
//
// fx 字段在 TSV 里写浮点(如 5.0), 用与内核同一份 battlecore::FxFromFloat 转成
// Q16.16 原始位 (R-C1 一致性: 导出时与运行时是同一份转换代码)。
// =============================================================================
#pragma once

#include "../battlecore/FixedPoint.h"
#include "CBinWriter.h"

#include <cstdlib>

namespace rsconfig {

struct TableSource {
    std::string tableName;   // "sp01" (sheet 名 / 表名)
    std::string structName;  // "Sp01Info" (生成结构体名)
    std::string tsv;         // TSV 内容
};

// 解析一张 TSV 为 TableData
inline bool ParseTsv(const std::string& tsv, const TableSource& src, TableData& out, std::string& err);

// 生成 C++ 结构体头 (生成物勿手改)
inline std::string GenerateHeader(const std::vector<TableSource>& tables, const std::vector<TableData>& parsed);

// 导出多张表 → 一个 bin + 一个合并头文件
inline bool ExportTables(const std::vector<TableSource>& tables,
                         std::vector<uint8_t>& bin, std::string& header, std::string& err);

// ---------------------------------------------------------------------------

namespace detail {

inline void SplitTabs(const std::string& line, std::vector<std::string>& out) {
    out.clear();
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(line[i]);
    }
    out.push_back(cur);
}

inline std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

inline bool ParseFieldType(const std::string& s, FieldType& t) {
    if (s == "i32" || s == "int" || s == "int32") t = kTypeI32;
    else if (s == "i64" || s == "int64") t = kTypeI64;
    else if (s == "fx") t = kTypeFx;
    else if (s == "f32" || s == "float") t = kTypeF32;
    else if (s == "str" || s == "string") t = kTypeStr;
    else if (s == "bool") t = kTypeBool;
    else if (s == "u8" || s == "uint8") t = kTypeU8;
    else if (s == "u16" || s == "uint16") t = kTypeU16;
    else if (s == "arr_i32" || s == "array" || s == "int[]") t = kTypeArrI32;
    else if (s == "arr_fx" || s == "fx[]") t = kTypeArrFx;
    else return false;
    return true;
}

inline const char* CppType(FieldType t) {
    switch (t) {
        case kTypeI32: case kTypeFx: return "int32_t";
        case kTypeI64: return "int64_t";
        case kTypeF32: return "float";
        case kTypeStr: return "std::string";
        case kTypeBool: return "bool";
        case kTypeU8: return "uint8_t";
        case kTypeU16: return "uint16_t";
        case kTypeArrI32: case kTypeArrFx: return "std::vector<int32_t>";
        default: return "int32_t";
    }
}

} // namespace detail

// ---------------------------------------------------------------------------

inline bool ParseTsv(const std::string& tsv, const TableSource& src, TableData& out, std::string& err) {
    using namespace detail;
    out = TableData();
    out.name = src.tableName;

    std::vector<std::string> lines;
    {
        std::string cur;
        for (size_t i = 0; i < tsv.size(); ++i) {
            if (tsv[i] == '\n') { lines.push_back(cur); cur.clear(); }
            else if (tsv[i] != '\r') cur.push_back(tsv[i]);
        }
        if (!cur.empty()) lines.push_back(cur);
    }
    std::vector<std::string> rows;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string t = Trim(lines[i]);
        if (t.empty() || t[0] == '#') continue;
        rows.push_back(lines[i]);
    }
    if (rows.size() < 3) { err = "TSV 至少需要 3 行 (字段名/类型/注释)"; return false; }

    std::vector<std::string> names, types, comments;
    SplitTabs(rows[0], names); SplitTabs(rows[1], types); SplitTabs(rows[2], comments);
    if (names.empty() || Trim(names[0]) != "id") { err = "首列必须是 id"; return false; }
    size_t nf = names.size();
    if (types.size() < nf) { err = "类型行列数不足"; return false; }

    FieldType idType;
    if (!ParseFieldType(Trim(types[0]), idType) || idType != kTypeI32) { err = "id 列类型必须是 i32"; return false; }

    out.fields.resize(nf);
    for (size_t i = 0; i < nf; ++i) {
        FieldType t;
        if (!ParseFieldType(Trim(types[i]), t)) { err = "未知类型: " + types[i]; return false; }
        out.fields[i] = FieldDesc(Trim(names[i]), t);
    }

    for (size_t r = 3; r < rows.size(); ++r) {
        std::vector<std::string> cells;
        SplitTabs(rows[r], cells);
        if (cells.empty() || Trim(cells[0]).empty()) continue;  // 跳过空 id
        Row row;
        row.id = (uint32_t)std::strtoul(cells[0].c_str(), nullptr, 10);
        row.cells.resize(nf);
        for (size_t i = 0; i < nf; ++i) {
            std::string v = (i < cells.size()) ? Trim(cells[i]) : std::string();
            Cell& c = row.cells[i];
            c.type = out.fields[i].type;
            switch (c.type) {
                case kTypeI32:  c.i = (int32_t)std::strtoll(v.c_str(), nullptr, 10); break;
                case kTypeI64:  c.i = (int64_t)std::strtoll(v.c_str(), nullptr, 10); break;
                case kTypeFx:   c.i = (int32_t)battlecore::FxFromFloat((float)std::strtod(v.c_str(), nullptr)); break;
                case kTypeF32: { float f = (float)std::strtod(v.c_str(), nullptr); uint32_t bits; std::memcpy(&bits, &f, 4); c.i = (int64_t)(int32_t)bits; break; }
                case kTypeStr:  c.s = v; break;
                case kTypeBool: c.i = (v == "1" || v == "true" || v == "TRUE") ? 1 : 0; break;
                case kTypeU8:   c.i = (uint8_t)std::strtoul(v.c_str(), nullptr, 10); break;
                case kTypeU16:  c.i = (uint16_t)std::strtoul(v.c_str(), nullptr, 10); break;
                case kTypeArrI32: case kTypeArrFx: {
                    std::string cur;
                    for (size_t k = 0; k <= v.size(); ++k) {
                        char ch = (k < v.size()) ? v[k] : ',';
                        if (ch == ',') {
                            cur = Trim(cur);
                            if (!cur.empty()) {
                                if (c.type == kTypeArrFx) c.a.push_back((int32_t)battlecore::FxFromFloat((float)std::strtod(cur.c_str(), nullptr)));
                                else c.a.push_back((int32_t)std::strtol(cur.c_str(), nullptr, 10));
                            }
                            cur.clear();
                        } else cur.push_back(ch);
                    }
                    break;
                }
                default: break;
            }
        }
        out.rows.push_back(std::move(row));
    }
    return true;
}

inline std::string GenerateHeader(const std::vector<TableSource>& tables, const std::vector<TableData>& parsed) {
    using namespace detail;
    std::string h;
    h += "// 自动生成, 勿手改 (config_exporter 从 Excel/TSV 生成)\n";
    h += "#pragma once\n";
    h += "#include <cstdint>\n#include <string>\n#include <vector>\n";
    h += "#include \"CBinReader.h\"\n\n";
    h += "namespace rsconfig {\n\n";

    for (size_t ti = 0; ti < parsed.size(); ++ti) {
        const TableData& t = parsed[ti];
        const std::string& sn = tables[ti].structName;
        h += "// 表 " + t.name + "\n";
        h += "struct " + sn + " {\n";
        h += "    uint32_t id = 0;\n";
        for (size_t f = 1; f < t.fields.size(); ++f) {
            const FieldDesc& fd = t.fields[f];
            std::string init = "0";
            if (fd.type == kTypeStr || IsArrayType(fd.type)) init = "{}";
            else if (fd.type == kTypeF32) init = "0.0f";
            std::string note;
            if (fd.type == kTypeFx) note = "   // fx_t (Q16.16)";
            h += "    " + std::string(CppType(fd.type)) + " " + fd.name + " = " + init + ";" + note + "\n";
        }
        h += "\n    bool Load(const CBinReader& rd, const uint8_t* row, uint32_t len) {\n";
        h += "        RowCursor c(row, len);\n";
        h += "        id = c.U32();\n";
        for (size_t f = 1; f < t.fields.size(); ++f) {
            const FieldDesc& fd = t.fields[f];
            switch (fd.type) {
                case kTypeI32: case kTypeFx: h += "        " + fd.name + " = c.I32();\n"; break;
                case kTypeI64: h += "        " + fd.name + " = c.I64();\n"; break;
                case kTypeF32: h += "        " + fd.name + " = c.F32();\n"; break;
                case kTypeU8:  h += "        " + fd.name + " = c.U8();\n"; break;
                case kTypeU16: h += "        " + fd.name + " = c.U16();\n"; break;
                case kTypeBool: h += "        " + fd.name + " = (c.U8() != 0);\n"; break;
                case kTypeStr:
                    h += "        { uint32_t _o = c.U32(); const char* _s = rd.GetString(_o); " + fd.name + " = _s ? _s : \"\"; }\n"; break;
                case kTypeArrI32: case kTypeArrFx:
                    h += "        { uint32_t _n = c.U32(); " + fd.name + ".clear(); for (uint32_t _i = 0; _i < _n; ++_i) " + fd.name + ".push_back(c.I32()); }\n"; break;
                default: break;
            }
        }
        h += "        return c.ok;\n";
        h += "    }\n};\n\n";
    }
    h += "} // namespace rsconfig\n";
    return h;
}

inline bool ExportTables(const std::vector<TableSource>& tables,
                         std::vector<uint8_t>& bin, std::string& header, std::string& err) {
    err.clear();
    bin.clear();
    header.clear();
    std::vector<TableData> parsed;
    parsed.resize(tables.size());
    for (size_t i = 0; i < tables.size(); ++i) {
        if (!ParseTsv(tables[i].tsv, tables[i], parsed[i], err)) return false;
    }
    if (!BuildBin(parsed, bin, err)) return false;
    header = GenerateHeader(tables, parsed);
    return true;
}

} // namespace rsconfig
