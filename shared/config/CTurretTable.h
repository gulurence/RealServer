/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CTurretTable.h
      Purpose:   炮台属性与出生点配置强类型解码

      CTurretTable 跨 turret/turret_point 两表解码 TurretConfig 与 TurretSpawn
      bulletSpeed 按 moveSpeed×3 折算 20Hz 帧, hurtHeroDis 取 attackdis, 炮台无难度加成
  *****************************************************************/

// =============================================================================
// CTurretTable.h — 炮台属性 + 出生点配置 bin 的强类型解码 (跨 turret / turret_point 两表)
//
// 用法 (客户端/服务器框架在 Init 后注入核心):
//   CBinReader sp01Rd; sp01Rd.LoadFile("sp01.bin");   // turret 表随 sp01.bin 打包 (enemyInfo.plist)
//   CBinReader mapRd;  mapRd.LoadFile("map.bin");     // turret_point 表随碰撞 bin 打包 (TMX)
//   std::map<int32_t, battlecore::TurretConfig> cfg;
//   CTurretTable(&sp01Rd).BuildTurretConfigMap(cfg);
//   core.SetTurretConfig(cfg);
//   std::vector<battlecore::TurretSpawn> spawns;
//   CTurretTable(&mapRd).GetTurretSpawns(levelId, spawns);
//   core.SetTurretSpawns(spawns);
//
// 换算口径 (对齐旧客户端 setTurretInfo / createAPaoTaB):
//   maxHp       = hp                               // 整数 HP (内核 ×kHpScale=100 进 centi)
//   hurtToHero  = FxFromFloat(hurtHPToHero)        // Q16.16
//   bulletSpeed = FxFromFloat(moveSpeed * 3.0f)    // px/帧 @20Hz (客户端 60fps → 20Hz ×3)
//   hurtHeroDis = attackdis                        // 像素距离 (FxDistPx 直接比较)
// 炮台无难度加成 (客户端 setTurretInfo 只读 raw 值, 难度数组只作用于 Sp01::rebirth)。
// =============================================================================
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "CBinReader.h"
#include "CConfigTable.h"
#include "../battlecore/BattleEntity.h"

namespace rsconfig {

// turret 表行 (schema: id/ptinfoId/hp/moveSpeed/hurtHPToHero/attackdis)
struct TurretInfo {
    uint32_t id           = 0;
    int32_t  ptinfoId     = 0;
    int32_t  hp           = 0;
    float    moveSpeed    = 0.0f;
    float    hurtHPToHero = 0.0f;
    int32_t  attackdis    = 0;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id           = c.U32();
        ptinfoId     = c.I32();
        hp           = c.I32();
        moveSpeed    = c.F32();
        hurtHPToHero = c.F32();
        attackdis    = c.I32();
        return c.ok;
    }
};

// turret_point 表行 (schema: id/levelId/nameIndex/seq/x/y/ptinfoId)
struct TurretPointInfo {
    uint32_t id        = 0;
    uint32_t levelId   = 0;
    int32_t  nameIndex = 0;
    uint32_t seq       = 0;
    int32_t  x = 0, y = 0;
    int32_t  ptinfoId  = 0;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id        = c.U32();
        levelId   = c.U32();
        nameIndex = c.I32();
        seq       = c.U32();
        x = c.I32(); y = c.I32();
        ptinfoId  = c.I32();
        return c.ok;
    }
};

// 跨两表解码炮台配置 + 出生点
class CTurretTable {
public:
    CTurretTable() : m_reader(nullptr) {}
    explicit CTurretTable(const CBinReader* r) : m_reader(r) {}

    bool Ok() const { return m_reader && m_reader->Ok(); }

    // turret 表全量打包成 SetTurretConfig 需要的 map (键 = ptinfoId)
    void BuildTurretConfigMap(std::map<int32_t, battlecore::TurretConfig>& out) const;

    // 取某关 (levelId) 的炮台出生点 (ForEachId 升序 = seq 序); 无炮台返回 false
    bool GetTurretSpawns(uint32_t levelId, std::vector<battlecore::TurretSpawn>& out) const;

private:
    const CBinReader* m_reader;
};

inline void CTurretTable::BuildTurretConfigMap(std::map<int32_t, battlecore::TurretConfig>& out) const {
    out.clear();
    if (!Ok()) return;

    CConfigTable<TurretInfo> tt(m_reader, "turret");
    if (!tt.Ok()) return;

    tt.ForEachId([&](uint32_t id) {
        TurretInfo v;
        if (!tt.Get(id, v)) return;
        battlecore::TurretConfig c;
        c.maxHp       = v.hp;
        c.hurtToHero  = battlecore::FxFromFloat(v.hurtHPToHero);
        c.bulletSpeed = battlecore::FxFromFloat(v.moveSpeed * 3.0f);
        c.hurtHeroDis = v.attackdis;
        out[v.ptinfoId] = c;
    });
}

inline bool CTurretTable::GetTurretSpawns(uint32_t levelId, std::vector<battlecore::TurretSpawn>& out) const {
    out.clear();
    if (!Ok()) return false;

    CConfigTable<TurretPointInfo> pt(m_reader, "turret_point");
    if (!pt.Ok()) return false;

    bool any = false;
    pt.ForEachId([&](uint32_t id) {
        TurretPointInfo v;
        if (!pt.Get(id, v) || v.levelId != levelId) return;
        battlecore::TurretSpawn s;
        s.nameIndex = v.nameIndex;
        s.x = battlecore::FxFromInt(v.x);
        s.y = battlecore::FxFromInt(v.y);
        s.ptinfoId = v.ptinfoId;
        out.push_back(s);
        any = true;
    });
    return any;
}

} // namespace rsconfig
