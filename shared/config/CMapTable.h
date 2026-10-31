/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CMapTable.h
      Purpose:   地图碰撞配置强类型解码

      CMapTable 跨 map/map_rect/map_entry 三表按 levelId 重组 battlecore::MapCollision
      矩形(people/bullet)与出生点像素 int 经 FxFromInt 转定点, 对齐旧客户端底左原点语义
  *****************************************************************/

// =============================================================================
// CMapTable.h — 碰撞配置 bin 的强类型解码 (跨 map / map_rect / map_entry 三张表)
//
// 用法 (客户端/服务器框架在 Init 后注入核心):
//   CBinReader rd; rd.LoadFile("xxx.bin");
//   battlecore::MapCollision col;
//   if (CMapTable(&rd).GetMapCollision(levelId, col)) core.MutableCollision() = col;
//
// 与 CConfigTable<T> 的区别: 碰撞是「矩形列表 + 出生点列表」, 分三张平表存
// (map 一行一关尺寸, map_rect 一行一矩形, map_entry 一行一出生点), 本类跨三表按 levelId 重组。
// x/y/w/h 存像素 int (已换算底左原点), 解码时 FxFromInt 转定点与旧路径逐位一致。
// =============================================================================
#pragma once

#include <cstdint>
#include <vector>

#include "CBinReader.h"
#include "CConfigTable.h"
#include "../battlecore/BattleEntity.h"

namespace rsconfig {

// map 表行 (schema: id/levelId/mapWidth/mapHeight)
struct MapInfo {
    uint32_t id        = 0;
    uint32_t levelId   = 0;
    int32_t  mapWidth  = 0;
    int32_t  mapHeight = 0;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id        = c.U32();
        levelId   = c.U32();
        mapWidth  = c.I32();
        mapHeight = c.I32();
        return c.ok;
    }
};

// map_rect 表行 (schema: id/levelId/kind/seq/x/y/w/h; kind 0=people 1=bullet)
struct MapRectInfo {
    uint32_t id      = 0;
    uint32_t levelId = 0;
    uint8_t  kind    = 0;
    uint32_t seq     = 0;
    int32_t  x = 0, y = 0, w = 0, h = 0;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id      = c.U32();
        levelId = c.U32();
        kind    = c.U8();
        seq     = c.U32();
        x = c.I32(); y = c.I32(); w = c.I32(); h = c.I32();
        return c.ok;
    }
};

// map_entry 表行 (schema: id/levelId/seq/x/y)
struct MapEntryInfo {
    uint32_t id      = 0;
    uint32_t levelId = 0;
    uint32_t seq     = 0;
    int32_t  x = 0, y = 0;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id      = c.U32();
        levelId = c.U32();
        seq     = c.U32();
        x = c.I32(); y = c.I32();
        return c.ok;
    }
};

// 跨三表解码 MapCollision
class CMapTable {
public:
    CMapTable() : m_reader(nullptr) {}
    explicit CMapTable(const CBinReader* r) : m_reader(r) {}

    bool Ok() const { return m_reader && m_reader->Ok(); }

    // 取某关 (levelId = big*10+small) 的碰撞; 关卡不存在返回 false
    bool GetMapCollision(uint32_t levelId, battlecore::MapCollision& out) const;

private:
    const CBinReader* m_reader;
};

inline bool CMapTable::GetMapCollision(uint32_t levelId, battlecore::MapCollision& out) const {
    out = battlecore::MapCollision();
    if (!Ok()) return false;

    CConfigTable<MapInfo>      mt(m_reader, "map");
    CConfigTable<MapRectInfo>  rt(m_reader, "map_rect");
    CConfigTable<MapEntryInfo> et(m_reader, "map_entry");
    if (!mt.Ok() || !rt.Ok() || !et.Ok()) return false;

    // 1) 关卡尺寸
    bool found = false;
    mt.ForEachId([&](uint32_t id) {
        if (found) return;
        MapInfo m;
        if (mt.Get(id, m) && m.levelId == levelId) {
            out.mapWidth  = m.mapWidth;
            out.mapHeight = m.mapHeight;
            found = true;
        }
    });
    if (!found) return false;

    // 2) 矩形 (ForEachId 升序 = 导出序 = levelId→seq; seq 已 people 先 bullet 后)
    rt.ForEachId([&](uint32_t id) {
        MapRectInfo r;
        if (!rt.Get(id, r) || r.levelId != levelId) return;
        battlecore::FxRect q;
        q.x = battlecore::FxFromInt(r.x);
        q.y = battlecore::FxFromInt(r.y);
        q.w = battlecore::FxFromInt(r.w);
        q.h = battlecore::FxFromInt(r.h);
        if (r.kind == 0) out.people.push_back(q);
        else             out.bullet.push_back(q);
    });

    // 3) 出生点 (ForEachId 升序 = seq 序)
    et.ForEachId([&](uint32_t id) {
        MapEntryInfo e;
        if (!et.Get(id, e) || e.levelId != levelId) return;
        battlecore::FxPoint p;
        p.x = battlecore::FxFromInt(e.x);
        p.y = battlecore::FxFromInt(e.y);
        out.entries.push_back(p);
    });

    return true;
}

} // namespace rsconfig
