// =============================================================================
// test_mapbin.cpp — S2 碰撞 TMX → bin → 内核 GetMapCollision 单测
//
// 覆盖:
//   1) ParseTmxCollision: 合成 TMX → 手写期望逐字段一致 (矩形 y 底左换算 / entry 顶边
//      换算 + 名字数字序排序)。
//   2) bin 往返: 合成 TMX → MapCollisionToTables → BuildBin → CBinReader →
//      CMapTable.GetMapCollision == 手写期望 (逐字段)。
//   3) (argv 传真实 level11.tmx) 真实关卡 direct 解析维度正确 (1408x832 / people29 /
//      bullet35 / entry3), 且 bin 往返 == direct (真实关卡数据验证)。
//
// 编译: build_mapbin.bat (MSVC, /std:c++14 /utf-8)。
// =============================================================================
#include "../TmxCollision.h"
#include "../CMapTable.h"
#include "../CBinWriter.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rsconfig;

static bool CollisionEq(const battlecore::MapCollision& a, const battlecore::MapCollision& b, std::string& diff) {
    if (a.mapWidth != b.mapWidth || a.mapHeight != b.mapHeight) {
        diff = "map 尺寸 " + std::to_string(a.mapWidth) + "x" + std::to_string(a.mapHeight) +
               " != " + std::to_string(b.mapWidth) + "x" + std::to_string(b.mapHeight);
        return false;
    }
    auto rectEq = [](const std::vector<battlecore::FxRect>& x, const std::vector<battlecore::FxRect>& y, std::string& d, const char* tag) {
        if (x.size() != y.size()) { d = std::string(tag) + " 数量 " + std::to_string(x.size()) + " != " + std::to_string(y.size()); return false; }
        for (size_t i = 0; i < x.size(); ++i) {
            if (x[i].x != y[i].x || x[i].y != y[i].y || x[i].w != y[i].w || x[i].h != y[i].h) {
                d = std::string(tag) + "[" + std::to_string(i) + "] 不同: (" +
                    std::to_string(x[i].x) + "," + std::to_string(x[i].y) + "," +
                    std::to_string(x[i].w) + "," + std::to_string(x[i].h) + ") != (" +
                    std::to_string(y[i].x) + "," + std::to_string(y[i].y) + "," +
                    std::to_string(y[i].w) + "," + std::to_string(y[i].h) + ")";
                return false;
            }
        }
        return true;
    };
    if (!rectEq(a.people, b.people, diff, "people")) return false;
    if (!rectEq(a.bullet, b.bullet, diff, "bullet")) return false;
    if (a.entries.size() != b.entries.size()) { diff = "entry 数量不同"; return false; }
    for (size_t i = 0; i < a.entries.size(); ++i) {
        if (a.entries[i].x != b.entries[i].x || a.entries[i].y != b.entries[i].y) {
            diff = "entry[" + std::to_string(i) + "] 不同";
            return false;
        }
    }
    return true;
}

// 合成 TMX: 4x3 tile(64) → 256x192; 2 people + 1 bullet + 2 entry (乱序 entry2 先, 测排序)
static const char* kTmx = R"tmx(<?xml version="1.0" encoding="UTF-8"?>
<map version="1.0" orientation="orthogonal" width="4" height="3" tilewidth="64" tileheight="64">
 <objectgroup name="collide_people" width="4" height="3">
  <object name="collide1" type="collide" x="10" y="20" width="30" height="40"/>
  <object name="collide2" type="collide" x="100" y="60" width="15" height="25"/>
 </objectgroup>
 <objectgroup name="collide_bullet" width="4" height="3">
  <object name="collide1" type="collide_bullet" x="5" y="5" width="8" height="9"/>
 </objectgroup>
 <objectgroup name="entry" width="4" height="3">
  <object name="entry2" type="entry" x="50" y="30" width="4" height="4"/>
  <object name="entry1" type="entry" x="200" y="10" width="4" height="4"/>
 </objectgroup>
</map>
)tmx";

static battlecore::MapCollision Expected() {
    battlecore::MapCollision c;
    c.mapWidth = 256;   // 4*64
    c.mapHeight = 192;  // 3*64
    c.people.push_back(battlecore::FxRect{battlecore::FxFromInt(10),  battlecore::FxFromInt(132), battlecore::FxFromInt(30), battlecore::FxFromInt(40)});  // y=192-20-40
    c.people.push_back(battlecore::FxRect{battlecore::FxFromInt(100), battlecore::FxFromInt(107), battlecore::FxFromInt(15), battlecore::FxFromInt(25)});  // y=192-60-25
    c.bullet.push_back(battlecore::FxRect{battlecore::FxFromInt(5),   battlecore::FxFromInt(178), battlecore::FxFromInt(8),  battlecore::FxFromInt(9)});   // y=192-5-9
    c.entries.push_back(battlecore::FxPoint{battlecore::FxFromInt(200), battlecore::FxFromInt(182)}); // entry1: y=192-10
    c.entries.push_back(battlecore::FxPoint{battlecore::FxFromInt(50),  battlecore::FxFromInt(162)}); // entry2: y=192-30
    return c;
}

static int g_fail = 0;
static void Check(bool ok, const char* what, const std::string& diff = std::string()) {
    if (ok) { std::printf("  [PASS] %s\n", what); }
    else    { std::printf("  [FAIL] %s: %s\n", what, diff.c_str()); ++g_fail; }
}

static bool ReadFile(const char* path, std::string& out) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    if (n > 0) std::fread(&out[0], 1, (size_t)n, f);
    std::fclose(f);
    return true;
}

// 合成 plist 测试用: 解析 + 建表 + 打包 + 解码一次到位
static void RoundTrip(uint32_t levelId, const battlecore::MapCollision& col,
                      std::vector<battlecore::MapCollision>& decoded, std::string& err) {
    TableData mt, rt, et;
    MapCollisionToTables(levelId, col, mt, rt, et);
    std::vector<TableData> tables; tables.push_back(mt); tables.push_back(rt); tables.push_back(et);
    std::vector<uint8_t> bin;
    BuildBin(tables, bin, err);
    CBinReader rd; rd.Load(bin.data(), bin.size());
    battlecore::MapCollision out;
    CMapTable(&rd).GetMapCollision(levelId, out);
    decoded.push_back(out);
}

int main(int argc, char** argv) {
    std::printf("== S2 碰撞 TMX→bin→GetMapCollision 单测 ==\n");

    // 测试 1: 合成 TMX → MapCollision
    {
        std::string err;
        battlecore::MapCollision got;
        Check(ParseTmxCollision(kTmx, got, err), "ParseTmxCollision 合成 TMX", err);
        std::string diff;
        Check(CollisionEq(got, Expected(), diff), "ParseTmxCollision == 手写期望", diff);
    }

    // 测试 2: 合成 TMX → bin → GetMapCollision == 手写期望
    {
        std::string err;
        battlecore::MapCollision got;
        ParseTmxCollision(kTmx, got, err);
        std::vector<battlecore::MapCollision> decoded;
        RoundTrip(11, got, decoded, err);
        std::string diff;
        Check(!decoded.empty() && CollisionEq(decoded[0], Expected(), diff), "bin 往返 == 手写期望", diff);
        battlecore::MapCollision none;
        Check(!CMapTable(nullptr).GetMapCollision(11, none), "空 reader 返回 false");
    }

    // 测试 3 (真实 level11): direct 解析维度正确 (1408x832 / people29 / bullet35 / entry3), 且 bin 往返 == direct
    if (argc >= 2) {
        std::string xml;
        if (ReadFile(argv[1], xml)) {
            std::string err;
            battlecore::MapCollision direct;
            Check(ParseTmxCollision(xml, direct, err), "ParseTmxCollision level11", err);
            std::string diff;
            Check(direct.mapWidth == 1408 && direct.mapHeight == 832, "level11 地图 1408x832",
                  std::to_string(direct.mapWidth) + "x" + std::to_string(direct.mapHeight));
            Check(direct.people.size() == 29 && direct.bullet.size() == 35 && direct.entries.size() == 3,
                  "level11 people29/bullet35/entry3", "count");

            std::vector<battlecore::MapCollision> decoded;
            RoundTrip(11, direct, decoded, err);
            Check(!decoded.empty() && CollisionEq(decoded[0], direct, diff),
                  "level11 bin 往返 == direct", diff);
            std::printf("    [info] people=%u bullet=%u entry=%u\n",
                        (unsigned)direct.people.size(), (unsigned)direct.bullet.size(), (unsigned)direct.entries.size());
        } else {
            std::printf("  [WARN] 打不开 %s, 跳过真实数据测试\n", argv[1]);
        }
    }

    std::printf(g_fail ? "\n结果: FAIL (%d)\n" : "\n结果: ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
