/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: wave_exporter.cpp
      Purpose:   关卡波次时间线 plist 转配置 bin 的命令行导出工具

      PlistToWavePlan 解析 out{bb}{ss}.plist 为 WaveDef 波次计划，WavePlanToTables 生成 wave 与 wave_spawn 表
      levelId 从文件名末尾数字推导，多关卡合成一张大表后 BuildBin 打包
      行结构体 WaveInfo、WaveSpawnInfo 与 shared/config/CWaveTable.h 严格对齐
  *****************************************************************/

// =============================================================================
// wave_exporter.cpp — out/*.plist (关卡波次时间线) → 配置 bin (命令行工具, S1)
//
// 用法: wave_exporter <out.bin> <plist1> [plist2 ...]
//   - levelId 从文件名 out{bb}{ss}.plist 的末尾数字推导 (11..68)。
//   - 所有关卡合成一张大表 (wave + wave_spawn), 对齐 R-A5「一张大表按 levelId 索引」。
//   - 行结构体 (WaveInfo/WaveSpawnInfo) 手工维护在 shared/config/CWaveTable.h, 与
//     此处 schema 严格对齐 (字段名/顺序一致)。
//
// 依赖: shared/config/WavePlist.h (纯头文件) + shared/config/CBinWriter.h。
// 编译: 同 config_exporter, cl /std:c++14 /utf-8。
// =============================================================================
#include "../../shared/config/WavePlist.h"
#include "../../shared/config/CBinWriter.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rsconfig;

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

// out{bb}{ss}.plist → levelId = 末尾数字 (11..68)
static int LevelIdFromPath(const std::string& path) {
    std::string base = path;
    size_t a = base.find_last_of("/\\");
    if (a != std::string::npos) base = base.substr(a + 1);
    size_t b = base.find('.');
    if (b != std::string::npos) base = base.substr(0, b);
    size_t p = base.size();
    while (p > 0 && std::isdigit((unsigned char)base[p - 1])) --p;
    int id = 0;
    if (p < base.size()) id = std::atoi(base.c_str() + p);
    return id;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("用法: wave_exporter <out.bin> <plist1> [plist2 ...]\n");
        return 2;
    }
    std::string binPath = argv[1];

    TableData waveTable, waveSpawnTable;
    waveTable.name = "wave";
    waveSpawnTable.name = "wave_spawn";
    // 用首张 plist 初始化 schema, 后续追加行 (schema 一致)
    bool schemaInit = false;
    uint32_t nextWaveId = 1, nextSpawnId = 1;

    for (int a = 2; a < argc; ++a) {
        std::string content;
        if (!ReadFile(argv[a], content)) { std::printf("读取失败: %s\n", argv[a]); return 1; }

        int levelId = LevelIdFromPath(argv[a]);
        if (levelId < 11 || levelId > 68) {
            std::printf("警告: 无法从 %s 推导 levelId(=%d), 跳过\n", argv[a], levelId);
            continue;
        }

        PValue root;
        std::string err;
        if (!ParsePlist(content, root, err)) { std::printf("解析失败 %s: %s\n", argv[a], err.c_str()); return 1; }

        std::vector<battlecore::WaveDef> plan;
        if (!PlistToWavePlan(root, (uint32_t)levelId, plan, err)) {
            std::printf("映射失败 %s: %s\n", argv[a], err.c_str()); return 1;
        }

        TableData wt, wst;
        WavePlanToTables((uint32_t)levelId, plan, wt, wst, nextWaveId, nextSpawnId);
        if (!schemaInit) { waveTable.fields = wt.fields; waveSpawnTable.fields = wst.fields; schemaInit = true; }
        for (auto& r : wt.rows)  waveTable.rows.push_back(std::move(r));
        for (auto& r : wst.rows) waveSpawnTable.rows.push_back(std::move(r));
        nextWaveId  += (uint32_t)wt.rows.size();
        nextSpawnId += (uint32_t)wst.rows.size();

        std::printf("  关 %d: %u 波, %u 出生组\n", levelId,
                    (unsigned)wt.rows.size(), (unsigned)wst.rows.size());
    }

    if (!schemaInit) { std::printf("无有效关卡\n"); return 1; }

    std::vector<TableData> tables;
    tables.push_back(std::move(waveTable));
    tables.push_back(std::move(waveSpawnTable));

    std::vector<uint8_t> bin;
    std::string err;
    if (!BuildBin(tables, bin, err)) { std::printf("打包失败: %s\n", err.c_str()); return 1; }
    if (!WriteFile(binPath, bin.data(), bin.size())) { std::printf("写 bin 失败: %s\n", binPath.c_str()); return 1; }

    std::printf("导出成功: %s (%u 字节), 表 wave(%u 行) + wave_spawn(%u 行)\n",
                binPath.c_str(), (unsigned)bin.size(),
                (unsigned)tables[0].rows.size(), (unsigned)tables[1].rows.size());
    return 0;
}
