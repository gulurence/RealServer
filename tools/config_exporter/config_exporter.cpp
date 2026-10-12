// =============================================================================
// config_exporter.cpp — Excel(TSV 导出) → bin + 生成 C++ 结构体头 (命令行工具)
//
// 用法: config_exporter <out.bin> <out.h> <表1.tsv> [表2.tsv ...]
//   - 表名 = 文件名去掉扩展名; 结构体名 = 表名 PascalCase + "Info" (sp01.tsv → Sp01Info)。
//   - TSV 前三行: 字段名 / 类型 / 注释; 第 4 行起数据; 首列 id。
//
// 说明: 直接解析 .xlsx(需 zip+inflate) 留作后续适配; 当前入口是 Excel「另存为
// 制表符分隔文本」的 TSV, 与策划 Excel 编辑源一一对应。
// =============================================================================
#include "../../shared/config/ConfigExporter.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rsconfig;

// 读整个文件
static bool ReadFile(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    if (n > 0) std::fread(&out[0], 1, (size_t)n, f);
    std::fclose(f);
    return true;
}

static bool WriteFile(const std::string& path, const void* data, size_t size) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(data, 1, size, f);
    std::fclose(f);
    return true;
}

// 表名 → 结构体名: sp01 → Sp01, wave → Wave, sp01_boss → Sp01Boss, 再 + "Info"
static std::string PascalCase(const std::string& s) {
    std::string out;
    bool up = true;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '_' || c == '-' || c == ' ') { up = true; continue; }
        if (up && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out.push_back(c);
        up = false;
    }
    return out.empty() ? std::string("Table") : out;
}

static std::string BaseName(const std::string& path) {
    size_t a = path.find_last_of("/\\");
    size_t b = path.find_last_of('.');
    if (a == std::string::npos) a = 0; else a += 1;
    if (b == std::string::npos || b <= a) b = path.size();
    return path.substr(a, b - a);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::printf("用法: config_exporter <out.bin> <out.h> <表1.tsv> [表2.tsv ...]\n");
        return 2;
    }
    std::string binPath = argv[1];
    std::string hPath = argv[2];

    std::vector<TableSource> srcs;
    for (int i = 3; i < argc; ++i) {
        std::string content;
        if (!ReadFile(argv[i], content)) { std::printf("读取失败: %s\n", argv[i]); return 1; }
        std::string base = BaseName(argv[i]);
        TableSource s;
        s.tableName = base;
        s.structName = PascalCase(base) + "Info";
        s.tsv = content;
        srcs.push_back(s);
    }

    std::vector<uint8_t> bin;
    std::string header, err;
    if (!ExportTables(srcs, bin, header, err)) {
        std::printf("导出失败: %s\n", err.c_str());
        return 1;
    }

    if (!WriteFile(binPath, bin.data(), bin.size())) { std::printf("写 bin 失败: %s\n", binPath.c_str()); return 1; }
    if (!WriteFile(hPath, header.data(), header.size())) { std::printf("写头失败: %s\n", hPath.c_str()); return 1; }

    std::printf("导出成功: %s (%u 字节), %s\n", binPath.c_str(), (unsigned)bin.size(), hPath.c_str());
    for (size_t i = 0; i < srcs.size(); ++i)
        std::printf("  表 %s -> 结构体 %s\n", srcs[i].tableName.c_str(), srcs[i].structName.c_str());
    return 0;
}
