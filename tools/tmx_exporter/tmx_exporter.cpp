/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tmx_exporter.cpp
      Purpose:   碰撞对象层 tmx 转配置 bin 的命令行导出工具

      ParseTmxCollision 解析 map/level{bb}{ss}.tmx，MapCollisionToTables 生成 map、map_rect、map_entry 表
      ParseTmxTurretPoints 解析 paota_point 层生成 turret_point 表，缺层时跳过不报错
      levelId 从文件名末尾数字推导，多关卡合成一张大表后 BuildBin 打包
  *****************************************************************/

// =============================================================================
// tmx_exporter.cpp — map/level{bb}{ss}.tmx (碰撞对象层) → 配置 bin (命令行工具, S2)
//
// 用法: tmx_exporter <out.bin> <tmx1> [tmx2 ...]
//   - levelId 从文件名 level{bb}{ss}.tmx 的末尾数字推导 (11..68)。
//   - 所有关卡合成一张大表 (map + map_rect + map_entry + turret_point), 对齐 R-A5「一张大表
//     按 levelId 索引」; paota_point 仅 17 个单机关卡有, 其余关卡无此层则跳过。
//   - 行结构体 (MapInfo/MapRectInfo/MapEntryInfo/TurretPointInfo) 手工维护在
//     shared/config/CMapTable.h 与 CTurretTable.h, 与此处 schema 严格对齐 (字段名/顺序一致)。
//
// 依赖: shared/config/TmxCollision.h (纯头文件) + shared/config/CBinWriter.h。
// 编译: 同 wave_exporter, cl /std:c++14 /utf-8。
// =============================================================================
#include "../../shared/config/TmxCollision.h"
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

// level{bb}{ss}.tmx → levelId = 末尾数字 (11..68)
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
        std::printf("用法: tmx_exporter <out.bin> <tmx1> [tmx2 ...]\n");
        return 2;
    }
    std::string binPath = argv[1];

    TableData mapTable, rectTable, entryTable, turretPointTable;
    mapTable.name = "map";
    rectTable.name = "map_rect";
    entryTable.name = "map_entry";
    turretPointTable.name = "turret_point";
    bool schemaInit = false;
    uint32_t nextMapId = 1, nextRectId = 1, nextEntryId = 1, nextTurretPointId = 1;

    for (int a = 2; a < argc; ++a) {
        std::string content;
        if (!ReadFile(argv[a], content)) { std::printf("读取失败: %s\n", argv[a]); return 1; }

        int levelId = LevelIdFromPath(argv[a]);
        if (levelId < 11 || levelId > 68) {
            std::printf("警告: 无法从 %s 推导 levelId(=%d), 跳过\n", argv[a], levelId);
            continue;
        }

        battlecore::MapCollision col;
        std::string err;
        if (!ParseTmxCollision(content, col, err)) { std::printf("解析失败 %s: %s\n", argv[a], err.c_str()); return 1; }

        TableData mt, rt, et;
        MapCollisionToTables((uint32_t)levelId, col, mt, rt, et, nextMapId, nextRectId, nextEntryId);
        if (!schemaInit) {
            mapTable.fields   = mt.fields;
            rectTable.fields  = rt.fields;
            entryTable.fields = et.fields;
            schemaInit = true;
        }
        for (auto& r : mt.rows) mapTable.rows.push_back(std::move(r));
        for (auto& r : rt.rows) rectTable.rows.push_back(std::move(r));
        for (auto& r : et.rows) entryTable.rows.push_back(std::move(r));
        nextMapId   += (uint32_t)mt.rows.size();
        nextRectId  += (uint32_t)rt.rows.size();
        nextEntryId += (uint32_t)et.rows.size();

        // 炮台出生点 (S4): paota_point 仅 17 个单机关卡有, 其余关卡无此层 → 跳过不报错。
        std::vector<TurretPointRaw> pts;
        std::string terr;
        bool hasTurret = ParseTmxTurretPoints(content, pts, terr);
        if (hasTurret) {
            TableData tt;
            TurretPointsToTables((uint32_t)levelId, pts, tt, nextTurretPointId);
            if (turretPointTable.fields.empty()) turretPointTable.fields = tt.fields;
            for (auto& r : tt.rows) turretPointTable.rows.push_back(std::move(r));
            nextTurretPointId += (uint32_t)tt.rows.size();
        }

        std::printf("  关 %d: %dx%d, people=%u bullet=%u entry=%u turret=%u\n", levelId,
                    col.mapWidth, col.mapHeight,
                    (unsigned)col.people.size(), (unsigned)col.bullet.size(), (unsigned)col.entries.size(),
                    (unsigned)pts.size());
    }

    if (!schemaInit) { std::printf("无有效关卡\n"); return 1; }

    std::vector<TableData> tables;
    tables.push_back(std::move(mapTable));
    tables.push_back(std::move(rectTable));
    tables.push_back(std::move(entryTable));
    tables.push_back(std::move(turretPointTable));

    std::vector<uint8_t> bin;
    std::string err;
    if (!BuildBin(tables, bin, err)) { std::printf("打包失败: %s\n", err.c_str()); return 1; }
    if (!WriteFile(binPath, bin.data(), bin.size())) { std::printf("写 bin 失败: %s\n", binPath.c_str()); return 1; }

    std::printf("导出成功: %s (%u 字节), 表 map(%u 行) + map_rect(%u 行) + map_entry(%u 行) + turret_point(%u 行)\n",
                binPath.c_str(), (unsigned)bin.size(),
                (unsigned)tables[0].rows.size(), (unsigned)tables[1].rows.size(),
                (unsigned)tables[2].rows.size(), (unsigned)tables[3].rows.size());
    return 0;
}
