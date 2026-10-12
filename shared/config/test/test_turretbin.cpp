// =============================================================================
// test_turretbin.cpp — S4 炮台 enemyInfo(ptinfo) + TMX(paota_point) → bin → 内核 单测
//
// 覆盖:
//   1) PtInfoToTurretTable: 合成 plist → turret 表 (ptinfoId/hp/moveSpeed/hurtHPToHero/attackdis;
//      非 ptinfo_ 键跳过)。
//   2) turret bin 往返: 表 → BuildBin → CTurretTable.BuildTurretConfigMap == 手写期望
//      (maxHp / hurtToHero=FxFromFloat / bulletSpeed=FxFromFloat(moveSpeed×3) / hurtHeroDis)。
//   3) ParseTmxTurretPoints: 合成 TMX (非自闭合 object + 内嵌 <property name="plist">) →
//      nameIndex / x / y(翻转) / ptinfoId 逐字段一致。
//   4) turret_point bin 往返: 表 → BuildBin → CTurretTable.GetTurretSpawns(levelId) == 手写期望。
//   5) 内核端到端: turret + turret_point 同 bin → 注入 SetTurretConfig/SetTurretSpawns +
//      波次 kClassTurret 哨兵 → Tick 首帧 SpawnInitialTurrets 出生 + pt_N 激活同名炮台。
//   6) (argv 传真实 enemyInfo.plist / level12.tmx) 真实行抽查。
//
// 编译: build_turretbin.bat (MSVC, /std:c++14 /utf-8; 链接 BattleCore.cpp)。
// =============================================================================
#include "../EnemyInfoPlist.h"
#include "../TmxCollision.h"
#include "../CTurretTable.h"
#include "../CBinWriter.h"
#include "../../battlecore/BattleCore.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace rsconfig;
using namespace battlecore;

// 合成 enemyInfo plist: ptinfo_0 / ptinfo_1 (炮台属性) + boy_11 (小怪, 跳过)
static const char* kPlist = R"plist(<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
	<key>ptinfo_0</key><dict>
		<key>hp</key><string>500</string>
		<key>moveSpeed</key><string>2</string>
		<key>hurtHPToHero</key><string>1</string>
		<key>attackdis</key><string>300</string>
	</dict>
	<key>ptinfo_1</key><dict>
		<key>hp</key><string>5000</string>
		<key>moveSpeed</key><string>2.5</string>
		<key>hurtHPToHero</key><string>2</string>
		<key>attackdis</key><string>400</string>
	</dict>
	<key>boy_11</key><dict>
		<key>hp</key><string>30</string>
		<key>moveSpeed</key><string>1.2</string>
		<key>hurtHPToHero</key><string>0.5</string>
	</dict>
</dict>
</plist>
)plist";

// 合成 TMX: 4x3 tile(64) → 256x192; paota_point 2 炮台 (非自闭合 + 内嵌 plist 属性)
static const char* kTmx = R"tmx(<?xml version="1.0" encoding="UTF-8"?>
<map version="1.0" orientation="orthogonal" width="4" height="3" tilewidth="64" tileheight="64">
 <objectgroup name="paota_point" width="4" height="3">
  <object name="pt_1" x="100" y="50" width="40" height="40">
   <property name="plist" value="ptinfo_0"/>
  </object>
  <object name="pt_2" x="200" y="100" width="40" height="40">
   <property name="plist" value="ptinfo_1"/>
  </object>
 </objectgroup>
</map>
)tmx";

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

// turret + turret_point 两张表合成一个 bin (CTurretTable 按表名查找, 顺序无关)
static bool BuildTurretBin(std::vector<uint8_t>& bin, std::string& err) {
    PValue root;
    ParsePlist(kPlist, root, err);
    TableData turret;
    int ts = 0;
    PtInfoToTurretTable(root, turret, err, &ts);

    std::vector<TurretPointRaw> pts;
    ParseTmxTurretPoints(kTmx, pts, err);
    TableData tp;
    TurretPointsToTables(12, pts, tp);

    std::vector<TableData> tables;
    tables.push_back(std::move(turret));
    tables.push_back(std::move(tp));
    return BuildBin(tables, bin, err);
}

int main(int argc, char** argv) {
    std::printf("== S4 炮台 enemyInfo/TMX→bin→内核 单测 ==\n");

    // 测试 1: PtInfoToTurretTable → turret 表 (2 行, boy_11 跳过)
    {
        PValue root; std::string err;
        Check(ParsePlist(kPlist, root, err), "ParsePlist 合成 enemyInfo", err);
        TableData turret; int skipped = 0;
        Check(PtInfoToTurretTable(root, turret, err, &skipped), "PtInfoToTurretTable", err);
        Check(turret.rows.size() == 2, "turret 表 2 行", std::to_string(turret.rows.size()));
        Check(skipped == 1, "boy_11 跳过 1 键", std::to_string(skipped));
        Check(turret.fields.size() == 6 && turret.fields[1].name == "ptinfoId" &&
              turret.fields[5].name == "attackdis", "turret schema 字段序正确");
    }

    // 测试 2: turret bin 往返 → BuildTurretConfigMap == 手写期望
    {
        std::string err;
        std::vector<uint8_t> bin;
        Check(BuildTurretBin(bin, err), "BuildTurretBin", err);
        static CBinReader rd;
        Check(rd.Load(bin.data(), bin.size()), "CBinReader.Load", rd.Error());

        std::map<int32_t, TurretConfig> cfg;
        CTurretTable(&rd).BuildTurretConfigMap(cfg);
        Check(cfg.size() == 2, "BuildTurretConfigMap 2 键", std::to_string(cfg.size()));

        Check(cfg.count(0) == 1 && cfg.count(1) == 1, "键 ptinfoId 0/1 存在");
        Check(cfg[0].maxHp == 500, "ptinfo_0 maxHp==500", std::to_string(cfg[0].maxHp));
        Check(cfg[0].hurtToHero == FxFromFloat(1.0f), "ptinfo_0 hurtToHero==FxFromFloat(1)");
        Check(cfg[0].bulletSpeed == FxFromFloat(6.0f), "ptinfo_0 bulletSpeed==FxFromFloat(2×3)");
        Check(cfg[0].hurtHeroDis == 300, "ptinfo_0 hurtHeroDis==300", std::to_string(cfg[0].hurtHeroDis));
        Check(cfg[1].maxHp == 5000 && cfg[1].bulletSpeed == FxFromFloat(7.5f) && cfg[1].hurtHeroDis == 400,
              "ptinfo_1 属性 5000/7.5/400");
    }

    // 测试 3: ParseTmxTurretPoints → 逐字段一致 (y 翻转)
    {
        std::string err;
        std::vector<TurretPointRaw> pts;
        Check(ParseTmxTurretPoints(kTmx, pts, err), "ParseTmxTurretPoints", err);
        Check(pts.size() == 2, "paota_point 2 炮台", std::to_string(pts.size()));
        if (pts.size() == 2) {
            Check(pts[0].nameIndex == 1 && pts[0].x == 100 && pts[0].y == 102 && pts[0].ptinfoId == 0,
                  "pt_1 → (nameIndex=1, x=100, y=102, ptinfo=0)",
                  "(" + std::to_string(pts[0].nameIndex) + "," + std::to_string(pts[0].x) + "," +
                  std::to_string(pts[0].y) + "," + std::to_string(pts[0].ptinfoId) + ")");
            Check(pts[1].nameIndex == 2 && pts[1].x == 200 && pts[1].y == 52 && pts[1].ptinfoId == 1,
                  "pt_2 → (nameIndex=2, x=200, y=52, ptinfo=1)",
                  "(" + std::to_string(pts[1].nameIndex) + "," + std::to_string(pts[1].x) + "," +
                  std::to_string(pts[1].y) + "," + std::to_string(pts[1].ptinfoId) + ")");
        }
    }

    // 测试 4: turret_point bin 往返 → GetTurretSpawns == 手写期望
    {
        std::string err;
        std::vector<uint8_t> bin;
        Check(BuildTurretBin(bin, err), "BuildTurretBin(4)", err);
        static CBinReader rd;
        rd.Load(bin.data(), bin.size());
        std::vector<TurretSpawn> spawns;
        Check(CTurretTable(&rd).GetTurretSpawns(12, spawns), "GetTurretSpawns(12)");
        Check(spawns.size() == 2, "spawns 2 个", std::to_string(spawns.size()));
        if (spawns.size() == 2) {
            Check(spawns[0].nameIndex == 1 && spawns[0].x == FxFromInt(100) && spawns[0].y == FxFromInt(102) &&
                  spawns[0].ptinfoId == 0, "spawn[0] == (1,100,102,0)");
            Check(spawns[1].nameIndex == 2 && spawns[1].x == FxFromInt(200) && spawns[1].y == FxFromInt(52) &&
                  spawns[1].ptinfoId == 1, "spawn[1] == (2,200,52,1)");
        }
        std::vector<TurretSpawn> none;
        Check(!CTurretTable(&rd).GetTurretSpawns(99, none), "无炮台关卡返回 false");
        Check(!CTurretTable(nullptr).GetTurretSpawns(12, none), "空 reader 返回 false");
    }

    // 测试 5: 内核端到端 — 注入配置 + 波次激活 → 首帧出生 + pt_N 激活同名炮台
    {
        std::string err;
        std::vector<uint8_t> bin;
        Check(BuildTurretBin(bin, err), "BuildTurretBin(5)", err);
        static CBinReader rd;
        rd.Load(bin.data(), bin.size());

        CBattleCore core;
        std::vector<int64_t> ids = { 1001 };
        core.Init(0xABCD, ids);
        core.MutableHeroes()[0].x = FxFromInt(600);
        core.MutableHeroes()[0].y = FxFromInt(240);
        core.MutableCollision().mapWidth = 1024;
        core.MutableCollision().mapHeight = 640;

        std::map<int32_t, TurretConfig> cfg;
        CTurretTable(&rd).BuildTurretConfigMap(cfg);
        core.SetTurretConfig(cfg);
        std::vector<TurretSpawn> spawns;
        CTurretTable(&rd).GetTurretSpawns(12, spawns);
        core.SetTurretSpawns(spawns);

        // 波次: 第 1 波 1 组 kClassTurret 哨兵, 激活 nameIndex=1, appear 延迟 5 帧
        WaveDef wd;
        wd.isKey = false;
        WaveSpawnGroup g;
        g.classId = kClassTurret;
        g.sp01Id = 1;       // pt_1
        g.count = 1;
        g.interval = 5;     // appear 延迟
        g.entryPoint = 1;
        g.startTime = 0;
        wd.groups.push_back(g);
        std::vector<WaveDef> plan;
        plan.push_back(wd);
        core.SetWavePlan(plan);

        core.Tick(1);

        const std::vector<TurretState>& ts = core.GetTurrets();
        Check(ts.size() == 2, "Tick 首帧出生 2 炮台", std::to_string(ts.size()));
        if (ts.size() == 2) {
            Check(ts[0].nameIndex == 1 && ts[0].x == FxFromInt(100) && ts[0].y == FxFromInt(102),
                  "炮台[0] 位置/名 == (pt_1,100,102)");
            Check(ts[0].hp == 500 * 100 && ts[0].maxHp == 500 * 100, "炮台[0] hp==500×100",
                  std::to_string(ts[0].hp));
            Check(ts[0].hurtToHero == FxFromFloat(1.0f) && ts[0].bulletSpeed == FxFromFloat(6.0f) &&
                  ts[0].hurtHeroDis == 300, "炮台[0] 属性注入正确");
            Check(ts[0].action == kTurretAppear, "pt_1 被波次激活 → appear", std::to_string((int)ts[0].action));
            Check(ts[1].nameIndex == 2 && ts[1].action == kTurretInit, "pt_2 未激活保持 init",
                  std::to_string((int)ts[1].action));
            Check(ts[1].hp == 5000 * 100, "炮台[1] hp==5000×100", std::to_string(ts[1].hp));
        }
    }

    // 测试 6 (真实数据): argv[1]=enemyInfo.plist, argv[2]=某含 paota_point 的 level*.tmx
    if (argc >= 2) {
        std::string xml;
        if (ReadFile(argv[1], xml)) {
            PValue root; std::string err;
            Check(ParsePlist(xml, root, err), "ParsePlist 真实 enemyInfo", err);
            TableData turret; int skipped = 0;
            Check(PtInfoToTurretTable(root, turret, err, &skipped), "PtInfoToTurretTable 真实", err);
            std::printf("    [info] 真实 turret 行=%u 跳过=%d\n", (unsigned)turret.rows.size(), skipped);
        } else {
            std::printf("  [WARN] 打不开 %s, 跳过真实 enemyInfo 测试\n", argv[1]);
        }
    }
    if (argc >= 3) {
        std::string xml;
        if (ReadFile(argv[2], xml)) {
            std::string err;
            std::vector<TurretPointRaw> pts;
            bool has = ParseTmxTurretPoints(xml, pts, err);
            std::printf("    [info] 真实 paota_point=%s, 炮台=%u\n",
                        has ? "有" : "无", (unsigned)pts.size());
            for (size_t i = 0; i < pts.size(); ++i)
                std::printf("       pt_%d @(%d,%d) plist=ptinfo_%d\n",
                            pts[i].nameIndex, pts[i].x, pts[i].y, pts[i].ptinfoId);
        } else {
            std::printf("  [WARN] 打不开 %s, 跳过真实 TMX 测试\n", argv[2]);
        }
    }

    std::printf(g_fail ? "\n结果: FAIL (%d)\n" : "\n结果: ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
