/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: TmxCollision.h
      Purpose:   TMX 碰撞对象层离线解析

      ParseTmxCollision 解析 collide_people/collide_bullet/entry 对象层为 battlecore::MapCollision
      MapCollisionToTables/ParseTmxTurretPoints/TurretPointsToTables 转 map 表与炮台出生点, y 翻转底左原点
  *****************************************************************/

// =============================================================================
// TmxCollision.h — TMX 对象层 (collide_people/collide_bullet/entry) → 内核碰撞
// MapCollision 的离线解析与映射 (纯 C++, S2)
//
// 用途: S2 数据授权。把 `map/level{bb}{ss}.tmx` 的碰撞对象层离线解析成核心的
// battlecore::MapCollision (people/bullet 矩形 + entry 出生点), 再经 MapCollisionToTables
// 转成 map / map_rect / map_entry 三张 TableData 供 BuildBin 打包。
//
// 语义对齐旧客户端 (CCTMXXMLParser.cpp:513/523/526 + IOBuiledLay::addCollides/addEntrys):
//   - 对象层无 offsetx/offsety → 偏移 (0,0)。
//   - 矩形: x = rawX (可负), y = mapHeight - rawY - rawHeight, w/h 不变 (底左原点)。
//   - entry: x = rawX, y = mapHeight - rawY (对象顶边; 客户端读 y+height 后再加 height)。
//   - people/bullet 矩形 = 文件序 (getObjects() 解析序); entry = 名字数字序 (objectNamed("entryN"))。
//   - mapWidth/mapHeight = <map> width*tilewidth / height*tileheight。
//
// 纯 C++、无引擎依赖, 仅被导出工具与单测引用 (不入运行时内核路径)。
// =============================================================================
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../battlecore/BattleEntity.h"
#include "ConfigFormat.h"

namespace rsconfig {

namespace tmx_detail {

// 从标签字符串里取 name="value" 的属性值 (无则返回空串)。
inline std::string AttrOf(const std::string& tag, const char* name) {
    std::string key = std::string(name) + "=\"";
    size_t p = tag.find(key);
    if (p == std::string::npos) return std::string();
    p += key.size();
    size_t q = tag.find('"', p);
    if (q == std::string::npos) return std::string();
    return tag.substr(p, q - p);
}

} // namespace tmx_detail

// 整棵 TMX XML → MapCollision (仅取 collide_people / collide_bullet / entry 三对象层)。
inline bool ParseTmxCollision(const std::string& xml, battlecore::MapCollision& col, std::string& err) {
    using namespace tmx_detail;
    col = battlecore::MapCollision();

    // 1) <map> 尺寸
    size_t mp = xml.find("<map ");
    if (mp == std::string::npos) { err = "无 <map> 标签"; return false; }
    size_t me = xml.find('>', mp);
    if (me == std::string::npos) { err = "<map> 未闭合"; return false; }
    std::string mtag = xml.substr(mp, me - mp + 1);
    int mw = std::atoi(AttrOf(mtag, "width").c_str());
    int mh = std::atoi(AttrOf(mtag, "height").c_str());
    int tw = std::atoi(AttrOf(mtag, "tilewidth").c_str());
    int th = std::atoi(AttrOf(mtag, "tileheight").c_str());
    if (mw <= 0 || mh <= 0 || tw <= 0 || th <= 0) { err = "<map> 尺寸非法"; return false; }
    col.mapWidth  = mw * tw;
    col.mapHeight = mh * th;

    struct EntryTmp { int num = 0; int x = 0; int y = 0; };
    std::vector<EntryTmp> entries;

    // 2) 遍历 objectgroup
    size_t pos = 0;
    while ((pos = xml.find("<objectgroup ", pos)) != std::string::npos) {
        size_t openEnd = xml.find('>', pos);
        if (openEnd == std::string::npos) { err = "<objectgroup> 未闭合"; return false; }
        std::string gtag = xml.substr(pos, openEnd - pos + 1);
        std::string gname = AttrOf(gtag, "name");

        int kind = -1;  // 0=people, 1=bullet, 2=entry
        if      (gname == "collide_people") kind = 0;
        else if (gname == "collide_bullet") kind = 1;
        else if (gname == "entry")          kind = 2;

        size_t groupEnd = xml.find("</objectgroup>", openEnd);
        if (groupEnd == std::string::npos) groupEnd = xml.size();

        if (kind >= 0) {
            size_t q = openEnd + 1;
            while ((q = xml.find("<object ", q)) != std::string::npos && q < groupEnd) {
                size_t oend = xml.find('>', q);
                if (oend == std::string::npos || oend >= groupEnd) break;
                std::string otag = xml.substr(q, oend - q + 1);
                int x = std::atoi(AttrOf(otag, "x").c_str());
                int y = std::atoi(AttrOf(otag, "y").c_str());
                int w = std::atoi(AttrOf(otag, "width").c_str());
                int h = std::atoi(AttrOf(otag, "height").c_str());

                if (kind == 0 || kind == 1) {
                    battlecore::FxRect r;
                    r.x = battlecore::FxFromInt(x);
                    r.y = battlecore::FxFromInt(col.mapHeight - y - h);
                    r.w = battlecore::FxFromInt(w);
                    r.h = battlecore::FxFromInt(h);
                    if (kind == 0) col.people.push_back(r); else col.bullet.push_back(r);
                } else {
                    std::string nm = AttrOf(otag, "name");  // entryN
                    size_t d = nm.size();
                    while (d > 0 && std::isdigit((unsigned char)nm[d - 1])) --d;
                    EntryTmp e;
                    e.num = (d < nm.size()) ? std::atoi(nm.c_str() + d) : 0;
                    e.x   = x;
                    e.y   = col.mapHeight - y;   // 顶边: mapHeight - rawY
                    entries.push_back(e);
                }
                q = oend + 1;
            }
        }
        pos = (groupEnd != xml.size()) ? groupEnd + 1 : xml.size();
    }

    // 3) entry 按名字数字序 (entry1,entry2,...)
    std::sort(entries.begin(), entries.end(),
              [](const EntryTmp& a, const EntryTmp& b) { return a.num < b.num; });
    for (const auto& e : entries) {
        battlecore::FxPoint p;
        p.x = battlecore::FxFromInt(e.x);
        p.y = battlecore::FxFromInt(e.y);
        col.entries.push_back(p);
    }

    if (col.people.empty() && col.bullet.empty()) { err = "无碰撞对象层数据"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
// MapCollision → map / map_rect / map_entry 三张 TableData (行 id 自增)
// rect 顺序 = people 先、bullet 后, seq 连续; entry 顺序 = col.entries 序 (已按名字排)。
// ---------------------------------------------------------------------------
inline void MapCollisionToTables(uint32_t levelId, const battlecore::MapCollision& col,
                                 TableData& mapTable, TableData& rectTable, TableData& entryTable,
                                 uint32_t mapIdBase = 1, uint32_t rectIdBase = 1, uint32_t entryIdBase = 1) {
    auto I32 = [](int64_t v) { Cell c; c.type = kTypeI32; c.i = v; return c; };
    auto U8  = [](int64_t v) { Cell c; c.type = kTypeU8;  c.i = v; return c; };

    mapTable = TableData();
    mapTable.name = "map";
    mapTable.fields = { FieldDesc("id", kTypeI32), FieldDesc("levelId", kTypeI32),
                        FieldDesc("mapWidth", kTypeI32), FieldDesc("mapHeight", kTypeI32) };

    rectTable = TableData();
    rectTable.name = "map_rect";
    rectTable.fields = { FieldDesc("id", kTypeI32), FieldDesc("levelId", kTypeI32), FieldDesc("kind", kTypeU8),
                         FieldDesc("seq", kTypeI32), FieldDesc("x", kTypeI32), FieldDesc("y", kTypeI32),
                         FieldDesc("w", kTypeI32), FieldDesc("h", kTypeI32) };

    entryTable = TableData();
    entryTable.name = "map_entry";
    entryTable.fields = { FieldDesc("id", kTypeI32), FieldDesc("levelId", kTypeI32),
                          FieldDesc("seq", kTypeI32), FieldDesc("x", kTypeI32), FieldDesc("y", kTypeI32) };

    {   // map 行 (每关一行)
        Row r; r.id = mapIdBase;
        r.cells = { I32((int64_t)mapIdBase), I32((int64_t)levelId),
                    I32((int64_t)col.mapWidth), I32((int64_t)col.mapHeight) };
        mapTable.rows.push_back(std::move(r));
    }

    {   // rect 行
        uint32_t rid = rectIdBase;
        int32_t seq = 0;
        auto put = [&](uint8_t kind, const battlecore::FxRect& rr) {
            Row r; r.id = rid++;
            r.cells = { I32((int64_t)r.id), I32((int64_t)levelId), U8((int64_t)kind), I32((int64_t)seq++),
                        I32((int64_t)battlecore::FxToIntRound(rr.x)), I32((int64_t)battlecore::FxToIntRound(rr.y)),
                        I32((int64_t)battlecore::FxToIntRound(rr.w)), I32((int64_t)battlecore::FxToIntRound(rr.h)) };
            rectTable.rows.push_back(std::move(r));
        };
        for (const auto& r : col.people) put(0, r);
        for (const auto& r : col.bullet) put(1, r);
    }

    {   // entry 行
        uint32_t eid = entryIdBase;
        int32_t seq = 0;
        for (const auto& p : col.entries) {
            Row r; r.id = eid++;
            r.cells = { I32((int64_t)r.id), I32((int64_t)levelId), I32((int64_t)seq++),
                        I32((int64_t)battlecore::FxToIntRound(p.x)), I32((int64_t)battlecore::FxToIntRound(p.y)) };
            entryTable.rows.push_back(std::move(r));
        }
    }
}

// ---------------------------------------------------------------------------
// 炮台出生点 (S4): TMX `paota_point` 对象层 → 原始像素点列表。
// 对象形如 (非自闭合, 内嵌子属性):
//   <object name="pt_1" x y width height>
//     <property name="plist" value="ptinfo_0"/>
//   </object>
// 语义对齐旧客户端 (Sp01Boss::initPTSp01s + CCTMXXMLParser.cpp:526):
//   - nameIndex = name("pt_N") 的 N; ptinfoId = plist("ptinfo_N") 的 N。
//   - x = rawX, y = mapHeight - rawY - rawHeight (与碰撞矩形同公式, 底左原点)。
//   - 顺序 = getObjects() 解析序 (文件序); level62 有重复 pt_1 (数据瑕疵), 内核按「后者胜」。
// ---------------------------------------------------------------------------
struct TurretPointRaw {
    int32_t nameIndex = 0;
    int32_t x = 0;         // 已翻转底左原点像素
    int32_t y = 0;
    int32_t ptinfoId = 0;
};

// 整棵 TMX XML → 炮台出生点列表 (仅取 paota_point 对象层)。
inline bool ParseTmxTurretPoints(const std::string& xml, std::vector<TurretPointRaw>& points, std::string& err) {
    using namespace tmx_detail;
    points.clear();

    // 1) <map> 尺寸 (y 翻转需要 mapHeight)
    size_t mp = xml.find("<map ");
    if (mp == std::string::npos) { err = "无 <map> 标签"; return false; }
    size_t me = xml.find('>', mp);
    if (me == std::string::npos) { err = "<map> 未闭合"; return false; }
    std::string mtag = xml.substr(mp, me - mp + 1);
    int mh = std::atoi(AttrOf(mtag, "height").c_str());
    int th = std::atoi(AttrOf(mtag, "tileheight").c_str());
    if (mh <= 0 || th <= 0) { err = "<map> 尺寸非法"; return false; }
    int mapHeight = mh * th;

    // 2) 遍历 objectgroup 找 paota_point
    size_t pos = 0;
    while ((pos = xml.find("<objectgroup ", pos)) != std::string::npos) {
        size_t openEnd = xml.find('>', pos);
        if (openEnd == std::string::npos) { err = "<objectgroup> 未闭合"; return false; }
        std::string gtag = xml.substr(pos, openEnd - pos + 1);
        std::string gname = AttrOf(gtag, "name");
        size_t groupEnd = xml.find("</objectgroup>", openEnd);
        if (groupEnd == std::string::npos) groupEnd = xml.size();

        if (gname == "paota_point") {
            size_t q = openEnd + 1;
            while ((q = xml.find("<object ", q)) != std::string::npos && q < groupEnd) {
                size_t oend = xml.find('>', q);
                if (oend == std::string::npos || oend >= groupEnd) break;
                std::string otag = xml.substr(q, oend - q + 1);
                bool selfClose = otag.size() >= 2 && otag[otag.size() - 2] == '/';

                std::string nm = AttrOf(otag, "name");
                int x = std::atoi(AttrOf(otag, "x").c_str());
                int y = std::atoi(AttrOf(otag, "y").c_str());
                int h = std::atoi(AttrOf(otag, "height").c_str());

                size_t objEnd = oend + 1;
                int32_t ptinfoId = 0;
                if (!selfClose) {
                    objEnd = xml.find("</object>", oend);
                    if (objEnd == std::string::npos || objEnd >= groupEnd) objEnd = oend + 1;
                    // 内嵌 <property name="plist" value="ptinfo_N"/>
                    size_t p = xml.find("<property ", oend + 1);
                    if (p != std::string::npos && p < objEnd) {
                        size_t pend = xml.find('>', p);
                        std::string ptag = xml.substr(p, (pend == std::string::npos ? p : pend) - p + 1);
                        if (AttrOf(ptag, "name") == "plist") {
                            std::string pv = AttrOf(ptag, "value");
                            if (pv.compare(0, 7, "ptinfo_") == 0)
                                ptinfoId = (int32_t)std::strtol(pv.c_str() + 7, nullptr, 10);
                        }
                    }
                }

                TurretPointRaw t;
                if (nm.compare(0, 3, "pt_") == 0) t.nameIndex = (int32_t)std::strtol(nm.c_str() + 3, nullptr, 10);
                t.x = x;
                t.y = mapHeight - y - h;
                t.ptinfoId = ptinfoId;
                points.push_back(t);

                q = objEnd;
            }
        }
        pos = (groupEnd != xml.size()) ? groupEnd + 1 : xml.size();
    }

    if (points.empty()) { err = "无 paota_point 对象层数据"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
// 炮台出生点列表 → turret_point 表 (行 id 自增, seq = 文件序)
// schema: id/levelId/nameIndex/seq/x/y/ptinfoId
// ---------------------------------------------------------------------------
inline void TurretPointsToTables(uint32_t levelId, const std::vector<TurretPointRaw>& points,
                                 TableData& turretPointTable, uint32_t idBase = 1) {
    auto I32 = [](int64_t v) { Cell c; c.type = kTypeI32; c.i = v; return c; };

    turretPointTable = TableData();
    turretPointTable.name = "turret_point";
    turretPointTable.fields = {
        FieldDesc("id",        kTypeI32),
        FieldDesc("levelId",   kTypeI32),
        FieldDesc("nameIndex", kTypeI32),
        FieldDesc("seq",       kTypeI32),
        FieldDesc("x",         kTypeI32),
        FieldDesc("y",         kTypeI32),
        FieldDesc("ptinfoId",  kTypeI32),
    };

    uint32_t tid = idBase;
    int32_t seq = 0;
    for (const auto& t : points) {
        Row r; r.id = tid++;
        r.cells = { I32((int64_t)r.id), I32((int64_t)levelId), I32((int64_t)t.nameIndex), I32((int64_t)seq++),
                    I32((int64_t)t.x), I32((int64_t)t.y), I32((int64_t)t.ptinfoId) };
        turretPointTable.rows.push_back(std::move(r));
    }
}

} // namespace rsconfig
