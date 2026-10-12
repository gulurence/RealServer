// =============================================================================
// EnemyInfoPlist.h — plist/enemyInfo.plist (小怪属性) → sp01 表 的离线解析与映射 (纯 C++)
//
// 用途: S3 数据授权。把 `plist/enemyInfo.plist` (全局一份, 键 {name}_{N} → dict) 离线解析成
// `sp01` 单张 TableData 供 BuildBin 打包。
//
// 语义对齐旧客户端 (Sp01Boss::createASp01 / UISetLayerb::setSp01Info):
//   - name → classId 复用 ParseMonsterId (16 类全覆盖; ptinfo_* 判 special 跳过, 属 S4 炮台)。
//   - N = 最后一个 '_' 之后的数字 (boy_max_53 → sp01Id=53, 与客户端按完整串查 enemyInfo 一致)。
//   - 存原始 plist 值 (hp I32 / moveSpeed F32 / hurtHPToHero F32), 换算与难度增量在解码层
//     CSp01Table::GetSp01Config 完成 (对齐 CMapTable 存像素 int 范式)。
//
// 纯 C++、无引擎依赖, 仅被导出工具与单测引用 (不入运行时内核路径)。
// =============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "ConfigFormat.h"
#include "WavePlist.h"   // ParsePlist / ParseMonsterId / PValueToDouble

namespace rsconfig {

// enemyInfo plist 根 → sp01 表 (schema: id/classId/sp01Id/hp/moveSpeed/hurtHPToHero)。
// 返回 false 且 err 非空表示根结构异常; special 键 (ptinfo_* 等) 跳过并计入 skipped。
inline bool EnemyInfoToSp01Table(const PValue& root, TableData& out, std::string& err, int* skipped = nullptr) {
    out = TableData();
    out.name = "sp01";
    out.fields = {
        FieldDesc("id",           kTypeI32),
        FieldDesc("classId",      kTypeU8),
        FieldDesc("sp01Id",       kTypeI32),
        FieldDesc("hp",           kTypeI32),
        FieldDesc("moveSpeed",    kTypeF32),
        FieldDesc("hurtHPToHero", kTypeF32),
    };
    if (skipped) *skipped = 0;
    if (!root.IsDict()) { err = "enemyInfo 根不是 <dict>"; return false; }

    auto I32 = [](int64_t v) { Cell c; c.type = kTypeI32; c.i = v; return c; };
    auto U8  = [](int64_t v) { Cell c; c.type = kTypeU8;  c.i = v; return c; };
    auto F32 = [](float v)    { Cell c; c.type = kTypeF32; uint32_t u; std::memcpy(&u, &v, 4); c.i = (int32_t)u; return c; };

    uint32_t nextId = 1;
    for (const auto& kv : root.dict) {
        const std::string& name = kv.first;
        const PValue& entry = kv.second;
        if (!entry.IsDict()) { if (skipped) ++*skipped; continue; }

        MonsterRef m = ParseMonsterId(name);
        if (m.special) { if (skipped) ++*skipped; continue; }

        const PValue* hp   = entry.Find("hp");
        const PValue* ms   = entry.Find("moveSpeed");
        const PValue* hurt = entry.Find("hurtHPToHero");
        if (!hp || !ms || !hurt) { if (skipped) ++*skipped; continue; }

        Row r;
        r.id = nextId++;
        r.cells = {
            I32((int64_t)r.id),
            U8((int64_t)m.classId),
            I32((int64_t)m.sp01Id),
            I32((int64_t)PValueToDouble(*hp)),
            F32((float)PValueToDouble(*ms)),
            F32((float)PValueToDouble(*hurt)),
        };
        out.rows.push_back(std::move(r));
    }
    return true;
}

// enemyInfo plist 根 → turret 表 (schema: id/ptinfoId/hp/moveSpeed/hurtHPToHero/attackdis)。
// 只取 ptinfo_N 键 (S4 炮台属性, 客户端 setTurretInfo 读 raw 值); 存原始 plist 值, 换算
// (hurtToHero=FxFromFloat(hurtHPToHero) / bulletSpeed=FxFromFloat(moveSpeed×3) / hurtHeroDis=attackdis)
// 在解码层 CTurretTable::BuildTurretConfigMap 完成。非 ptinfo_ 键跳过计入 skipped。
inline bool PtInfoToTurretTable(const PValue& root, TableData& out, std::string& err, int* skipped = nullptr) {
    out = TableData();
    out.name = "turret";
    out.fields = {
        FieldDesc("id",           kTypeI32),
        FieldDesc("ptinfoId",     kTypeI32),
        FieldDesc("hp",           kTypeI32),
        FieldDesc("moveSpeed",    kTypeF32),
        FieldDesc("hurtHPToHero", kTypeF32),
        FieldDesc("attackdis",    kTypeI32),
    };
    if (skipped) *skipped = 0;
    if (!root.IsDict()) { err = "enemyInfo 根不是 <dict>"; return false; }

    auto I32 = [](int64_t v) { Cell c; c.type = kTypeI32; c.i = v; return c; };
    auto F32 = [](float v)    { Cell c; c.type = kTypeF32; uint32_t u; std::memcpy(&u, &v, 4); c.i = (int32_t)u; return c; };

    uint32_t nextId = 1;
    for (const auto& kv : root.dict) {
        const std::string& name = kv.first;
        const PValue& entry = kv.second;
        if (name.compare(0, 7, "ptinfo_") != 0) { if (skipped) ++*skipped; continue; }
        if (!entry.IsDict()) { if (skipped) ++*skipped; continue; }

        const PValue* hp   = entry.Find("hp");
        const PValue* ms   = entry.Find("moveSpeed");
        const PValue* hurt = entry.Find("hurtHPToHero");
        const PValue* dis  = entry.Find("attackdis");
        if (!hp || !ms || !hurt) { if (skipped) ++*skipped; continue; }

        int32_t ptinfoId = (int32_t)std::strtol(name.c_str() + 7, nullptr, 10);   // "ptinfo_N" → N

        Row r;
        r.id = nextId++;
        r.cells = {
            I32((int64_t)r.id),
            I32((int64_t)ptinfoId),
            I32((int64_t)PValueToDouble(*hp)),
            F32((float)PValueToDouble(*ms)),
            F32((float)PValueToDouble(*hurt)),
            I32((int64_t)(dis ? PValueToDouble(*dis) : 0.0)),
        };
        out.rows.push_back(std::move(r));
    }
    return true;
}

} // namespace rsconfig
