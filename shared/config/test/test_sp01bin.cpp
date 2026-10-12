// =============================================================================
// test_sp01bin.cpp — S3 小怪属性 enemyInfo plist → bin → 内核 Sp01Config 单测
//
// 覆盖:
//   1) EnemyInfoToSp01Table: 合成 plist → sp01 表 (classId/末段 sp01Id/ptinfo 跳过)。
//   2) bin 往返: 表 → BuildBin → CSp01Table.GetSp01Config == 手写期望 (maxHp/walkSpeed×3/hurtToHero)。
//   3) 难度增量: difficulty 0/1/2 的 hp+{0,50,100} / hurt+{0,5,15} / speed+{0,0.1,0.1}。
//   4) 组合键碰撞: boy_511 与 girl_511 (同 sp01Id=511) 各自取各自属性, 不互相覆盖。
//   5) BuildConfigMap: 键 = Sp01ConfigKey(classId, sp01Id), 供 SetSp01Config 注入。
//   6) (argv 传真实 enemyInfo.plist) 真实行抽查: boy_11/boy_511/girl_511/boy_max_53 + ptinfo 跳过。
//
// 编译: build_sp01bin.bat (MSVC, /std:c++14 /utf-8)。
// =============================================================================
#include "../EnemyInfoPlist.h"
#include "../CSp01Table.h"
#include "../CBinWriter.h"
#include "../../battlecore/BattleEntity.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rsconfig;

// 合成 enemyInfo plist: 含同 sp01Id 不同 classId (boy_511/girl_511) 与 _max 双下划线 (boy_max_53) + ptinfo(跳过)
static const char* kPlist = R"plist(<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
	<key>boy_11</key><dict>
		<key>hp</key><string>30</string>
		<key>moveSpeed</key><string>1.2</string>
		<key>hurtHPToHero</key><string>0.5</string>
	</dict>
	<key>boy_511</key><dict>
		<key>hp</key><string>250</string>
		<key>moveSpeed</key><string>1.2</string>
		<key>hurtHPToHero</key><string>0.5</string>
	</dict>
	<key>girl_511</key><dict>
		<key>hp</key><string>200</string>
		<key>moveSpeed</key><string>1.2</string>
		<key>hurtHPToHero</key><string>0.5</string>
	</dict>
	<key>boy_max_53</key><dict>
		<key>hp</key><string>2200</string>
		<key>moveSpeed</key><string>1.2</string>
		<key>hurtHPToHero</key><string>7</string>
	</dict>
	<key>ptinfo_0</key><dict>
		<key>hp</key><string>500</string>
		<key>moveSpeed</key><string>2</string>
		<key>hurtHPToHero</key><string>1</string>
	</dict>
</dict>
</plist>
)plist";

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

// 合成 plist → bin → CSp01Table (一次到位)
static CSp01Table BuildReader() {
    static std::vector<uint8_t> g_bin;
    PValue root; std::string err;
    ParsePlist(kPlist, root, err);
    TableData sp01; int skipped = 0;
    EnemyInfoToSp01Table(root, sp01, err, &skipped);
    std::vector<TableData> tables; tables.push_back(std::move(sp01));
    BuildBin(tables, g_bin, err);
    static CBinReader g_rd; g_rd.Load(g_bin.data(), g_bin.size());
    return CSp01Table(&g_rd);
}

int main(int argc, char** argv) {
    std::printf("== S3 小怪属性 enemyInfo→bin→Sp01Config 单测 ==\n");

    CSp01Table tab = BuildReader();

    // 测试 1: boy_11 基础属性 (难度 0), 走速 ×3、hurt 小数
    {
        battlecore::Sp01Config c;
        Check(tab.GetSp01Config(battlecore::kClassSp01Boy, 11, c), "GetSp01Config(boy,11)");
        Check(c.maxHp == 30, "boy_11 maxHp==30", std::to_string(c.maxHp));
        Check(c.walkSpeed == battlecore::FxFromFloat(1.2f * 3.0f), "boy_11 walkSpeed==FxFromFloat(3.6)");
        Check(c.hurtToHero == battlecore::FxFromFloat(0.5f), "boy_11 hurtToHero==FxFromFloat(0.5)");
    }

    // 测试 2: 难度增量 (0/1/2)
    {
        battlecore::Sp01Config d0, d1, d2;
        Check(tab.GetSp01Config(battlecore::kClassSp01Boy, 11, 0, d0) &&
              tab.GetSp01Config(battlecore::kClassSp01Boy, 11, 1, d1) &&
              tab.GetSp01Config(battlecore::kClassSp01Boy, 11, 2, d2), "三难度取 boy_11");
        Check(d1.maxHp == 30 + 50  && d2.maxHp == 30 + 100, "难度 hp +{0,50,100}");
        Check(d1.hurtToHero == battlecore::FxFromFloat(0.5f + 5.0f) &&
              d2.hurtToHero == battlecore::FxFromFloat(0.5f + 15.0f), "难度 hurt +{0,5,15}");
        Check(d1.walkSpeed == battlecore::FxFromFloat((1.2f + 0.1f) * 3.0f) &&
              d2.walkSpeed == battlecore::FxFromFloat((1.2f + 0.1f) * 3.0f), "难度 speed +{0,0.1,0.1} ×3");
    }

    // 测试 3: 组合键碰撞 (boy_511 vs girl_511, 同 sp01Id=511)
    {
        battlecore::Sp01Config boy, girl;
        Check(tab.GetSp01Config(battlecore::kClassSp01Boy, 511, boy) &&
              tab.GetSp01Config(battlecore::kClassSp01Girl, 511, girl), "同 sp01Id 取 boy/girl");
        Check(boy.maxHp == 250 && girl.maxHp == 200, "boy_511=250 vs girl_511=200 不覆盖",
              std::to_string(boy.maxHp) + "/" + std::to_string(girl.maxHp));
    }

    // 测试 4: _max 双下划线 → sp01Id 取末段 53
    {
        battlecore::Sp01Config c;
        Check(tab.GetSp01Config(battlecore::kClassSp01BoyMax, 53, c), "GetSp01Config(boy_max,53)");
        Check(c.maxHp == 2200, "boy_max_53 maxHp==2200", std::to_string(c.maxHp));
        Check(c.hurtToHero == battlecore::FxFromFloat(7.0f), "boy_max_53 hurtToHero==FxFromFloat(7)");
    }

    // 测试 5: BuildConfigMap 键 = Sp01ConfigKey(classId, sp01Id), 且 ptinfo 被跳过
    {
        std::map<uint32_t, battlecore::Sp01Config> cfg;
        tab.BuildConfigMap(0, cfg);
        Check(cfg.size() == 4, "BuildConfigMap 4 行 (ptinfo 跳过)", std::to_string(cfg.size()));
        Check(cfg.count(battlecore::Sp01ConfigKey(battlecore::kClassSp01Boy, 11)) == 1, "键 (boy,11) 存在");
        Check(cfg.count(battlecore::Sp01ConfigKey(battlecore::kClassSp01Girl, 511)) == 1, "键 (girl,511) 存在");
        Check(cfg[battlecore::Sp01ConfigKey(battlecore::kClassSp01Boy, 511)].maxHp == 250, "map 内 boy_511=250");
        Check(cfg[battlecore::Sp01ConfigKey(battlecore::kClassSp01Girl, 511)].maxHp == 200, "map 内 girl_511=200");
    }

    // 测试 6 (真实 enemyInfo): 抽查 + ptinfo 跳过计数
    if (argc >= 2) {
        std::string xml;
        if (ReadFile(argv[1], xml)) {
            PValue root; std::string err;
            Check(ParsePlist(xml, root, err), "ParsePlist 真实 enemyInfo", err);
            TableData sp01; int skipped = 0;
            Check(EnemyInfoToSp01Table(root, sp01, err, &skipped), "EnemyInfoToSp01Table 真实", err);
            unsigned rowCount = (unsigned)sp01.rows.size();

            std::vector<TableData> tables; tables.push_back(std::move(sp01));
            std::vector<uint8_t> bin;
            BuildBin(tables, bin, err);
            static CBinReader rd; rd.Load(bin.data(), bin.size());
            CSp01Table rtab(&rd);

            battlecore::Sp01Config c;
            Check(rtab.GetSp01Config(battlecore::kClassSp01Boy, 11, c) && c.maxHp == 30,
                  "真实 boy_11 hp=30", std::to_string(c.maxHp));
            Check(c.hurtToHero == battlecore::FxFromFloat(0.5f), "真实 boy_11 hurt=0.5");
            battlecore::Sp01Config b, g;
            rtab.GetSp01Config(battlecore::kClassSp01Boy, 511, b);
            rtab.GetSp01Config(battlecore::kClassSp01Girl, 511, g);
            Check(b.maxHp == 250 && g.maxHp == 200, "真实 boy_511=250 / girl_511=200",
                  std::to_string(b.maxHp) + "/" + std::to_string(g.maxHp));
            std::printf("    [info] 真实行数=%u 跳过=%d\n", rowCount, skipped);
        } else {
            std::printf("  [WARN] 打不开 %s, 跳过真实数据测试\n", argv[1]);
        }
    }

    std::printf(g_fail ? "\n结果: FAIL (%d)\n" : "\n结果: ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
