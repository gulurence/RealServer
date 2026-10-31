/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: sp01_exporter.cpp
      Purpose:   小怪属性 plist 转配置 bin 的命令行导出工具

      EnemyInfoToSp01Table 解析 enemyInfo.plist 生成 sp01 表，PtInfoToTurretTable 生成 turret 表
      存原始 plist 值，难度增量与换算留在解码层 CSp01Table::GetSp01Config 完成
      BuildBin 将两张表打包进同一 bin，键 name_N 中 N 取最后一个下划线后的数字作 sp01Id
  *****************************************************************/

// =============================================================================
// sp01_exporter.cpp — plist/enemyInfo.plist (小怪属性) → 配置 bin (命令行工具, S3)
//
// 用法: sp01_exporter <out.bin> <enemyInfo.plist>
//   - enemyInfo.plist 全局一份, 键 {name}_{N} → dict { hp/moveSpeed/hurtHPToHero/... }。
//   - name → classId 复用 WavePlist.h::ParseMonsterId (16 类全覆盖; ptinfo_* 判 special
//     跳过, 属 S4 炮台); N = 最后一个 '_' 之后的数字 (boy_max_53 → sp01Id=53)。
//   - 存原始 plist 值 (hp I32 / moveSpeed F32 / hurtHPToHero F32), 换算与难度增量在
//     解码层 CSp01Table::GetSp01Config 完成 (对齐 CMapTable 存像素 int 范式)。
//   - ptinfo_0..5 (S4 炮台属性) 另出 `turret` 表 (PtInfoToTurretTable), 与 sp01 同 bin。
//
// 依赖: shared/config/EnemyInfoPlist.h (纯头文件) + shared/config/CBinWriter.h。
// 编译: 同 wave_exporter, cl /std:c++14 /utf-8。
// =============================================================================
#include "../../shared/config/EnemyInfoPlist.h"
#include "../../shared/config/CBinWriter.h"

#include <cstdio>
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

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("用法: sp01_exporter <out.bin> <enemyInfo.plist>\n");
        return 2;
    }
    std::string binPath = argv[1];

    std::string content;
    if (!ReadFile(argv[2], content)) { std::printf("读取失败: %s\n", argv[2]); return 1; }

    PValue root;
    std::string err;
    if (!ParsePlist(content, root, err)) { std::printf("解析失败: %s\n", err.c_str()); return 1; }

    TableData sp01;
    int skipped = 0;
    if (!EnemyInfoToSp01Table(root, sp01, err, &skipped)) { std::printf("映射失败: %s\n", err.c_str()); return 1; }
    if (sp01.rows.empty()) { std::printf("无有效小怪行\n"); return 1; }

    TableData turret;
    int turretSkipped = 0;
    PtInfoToTurretTable(root, turret, err, &turretSkipped);

    std::vector<TableData> tables;
    tables.push_back(std::move(sp01));
    tables.push_back(std::move(turret));

    std::vector<uint8_t> bin;
    if (!BuildBin(tables, bin, err)) { std::printf("打包失败: %s\n", err.c_str()); return 1; }
    if (!WriteFile(binPath, bin.data(), bin.size())) { std::printf("写 bin 失败: %s\n", binPath.c_str()); return 1; }

    std::printf("导出成功: %s (%u 字节), 表 sp01(%u 行) + turret(%u 行), 跳过 %d 特殊键\n",
                binPath.c_str(), (unsigned)bin.size(),
                (unsigned)tables[0].rows.size(), (unsigned)tables[1].rows.size(), skipped);
    return 0;
}
