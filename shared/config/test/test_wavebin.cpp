// =============================================================================
// test_wavebin.cpp — S1 波次 plist → bin → 内核 GetWavePlan 单测
//
// 覆盖:
//   1) PlistToWavePlan: 合成 plist → 手写期望逐字段一致 (异质 duilie 拆组 /
//      pt_N 转炮台激活组 / drop_/dialog_ 跳过 / 秒→帧、60fps→20Hz 转换 / classId+sp01Id 解析)。
//   2) bin 往返: 合成 plist → WavePlanToTables → BuildBin → CBinReader →
//      CWaveTable.GetWavePlan == 手写期望 (逐字段)。
//   3) (可选, argv 传真实 plist 路径) 真实关卡 direct 解析 == bin 往返, 验证
//      bin 管线对真实数据的自洽性。
//
// 编译: build_wavebin.bat (MSVC, /std:c++14 /utf-8)。
// =============================================================================
#include "../WavePlist.h"
#include "../CWaveTable.h"
#include "../CBinWriter.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rsconfig;

// 手写期望: 见合成 plist 逐字段推导 (一个出生组)
static battlecore::WaveSpawnGroup G(uint8_t cls, int32_t sp01Id, int32_t count, int32_t interval,
                                    int32_t entry, int32_t start) {
    battlecore::WaveSpawnGroup g;
    g.classId = cls; g.sp01Id = sp01Id; g.count = count;
    g.interval = interval; g.entryPoint = entry; g.startTime = start;
    return g;
}

static bool WaveEq(const std::vector<battlecore::WaveDef>& a,
                   const std::vector<battlecore::WaveDef>& b, std::string& diff) {
    if (a.size() != b.size()) { diff = "波数 " + std::to_string(a.size()) + " != " + std::to_string(b.size()); return false; }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].isKey != b[i].isKey) { diff = "波" + std::to_string(i) + " isKey 不同"; return false; }
        if (a[i].groups.size() != b[i].groups.size()) {
            diff = "波" + std::to_string(i) + " 组数不同"; return false;
        }
        for (size_t j = 0; j < a[i].groups.size(); ++j) {
            const auto& x = a[i].groups[j];
            const auto& y = b[i].groups[j];
            if (x.classId != y.classId || x.sp01Id != y.sp01Id || x.count != y.count ||
                x.interval != y.interval || x.entryPoint != y.entryPoint || x.startTime != y.startTime) {
                diff = "波" + std::to_string(i) + "组" + std::to_string(j) +
                       " 不同: classId=" + std::to_string(x.classId) + "/" + std::to_string(y.classId) +
                       " sp01Id=" + std::to_string(x.sp01Id) + "/" + std::to_string(y.sp01Id) +
                       " count=" + std::to_string(x.count) + "/" + std::to_string(y.count) +
                       " interval=" + std::to_string(x.interval) + "/" + std::to_string(y.interval) +
                       " entry=" + std::to_string(x.entryPoint) + "/" + std::to_string(y.entryPoint) +
                       " start=" + std::to_string(x.startTime) + "/" + std::to_string(y.startTime);
                return false;
            }
        }
    }
    return true;
}

// 合成 plist: 覆盖多波 / 异质 duilie / pt_/drop_/dialog_/boss / 不同 delay/startTime
static const char* kPlist = R"plist(<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
	<key>outNum</key><integer>3</integer>
	<key>waveNum</key><integer>2</integer>
	<key>wave1</key>
	<dict>
		<key>timelines</key>
		<array>
			<dict>
				<key>out1</key><dict><key>delay</key><string>60</string><key>duilie</key><array>
					<string>boy_10</string><string>boy_10</string><string>boy_511</string>
				</array></dict>
				<key>out2</key><dict><key>delay</key><string>40</string><key>duilie</key><array>
					<string>pt_1</string><string>girl_11</string><string>girl_11</string>
				</array></dict>
				<key>startTime</key><string>0.1</string>
			</dict>
			<dict>
				<key>out3</key><dict><key>delay</key><string>80</string><key>duilie</key><array>
					<string>drop_1</string><string>boss01_0</string><string>dialog_7,8</string>
				</array></dict>
				<key>startTime</key><string>5</string>
			</dict>
		</array>
	</dict>
	<key>wave2</key>
	<dict>
		<key>timelines</key>
		<array>
			<dict>
				<key>out1</key><dict><key>delay</key><string>60</string><key>duilie</key><array>
					<string>dog_0</string><string>dog_0</string><string>boy_max_53</string>
				</array></dict>
				<key>startTime</key><string>1</string>
			</dict>
		</array>
	</dict>
</dict>
</plist>
)plist";

static std::vector<battlecore::WaveDef> Expected() {
    std::vector<battlecore::WaveDef> e;

    // wave1 (5 组):
    //   out1 delay=60→interval20, entry=1: boy_10×2 (start=2), boy_511×1 (start=2+40=42)
    //   out2 delay=40→interval13, entry=2: pt_1 转炮台激活组 (classId=kClassTurret, sp01Id=1,
    //       count=1, interval=rel=0, start=2), girl_11×2 (start=2+13=15)
    //   out3 delay=80→interval27, entry=3: drop_1 跳过, boss01_0×1 (start=100+27=127), dialog 跳过
    battlecore::WaveDef w1; w1.isKey = false;
    w1.groups.push_back(G(battlecore::kClassSp01Boy, 10, 2, 20, 1, 2));
    w1.groups.push_back(G(battlecore::kClassSp01Boy, 511, 1, 20, 1, 42));
    w1.groups.push_back(G(battlecore::kClassTurret, 1, 1, 0, 2, 2));   // pt_1 炮台激活
    w1.groups.push_back(G(battlecore::kClassSp01Girl, 11, 2, 13, 2, 15));
    w1.groups.push_back(G(battlecore::kClassSp01BigGuard, 0, 1, 27, 3, 127));
    e.push_back(w1);

    // wave2 (2 组): out1 delay=60→interval20, entry=1: dog_0×2 (start=20),
    //   boy_max_53×1 (start=20+round(2*60/3)=60; 验 _max 双下划线 sp01Id 取末段 53)
    battlecore::WaveDef w2; w2.isKey = false;
    w2.groups.push_back(G(battlecore::kClassSp01Dog, 0, 2, 20, 1, 20));
    w2.groups.push_back(G(battlecore::kClassSp01BoyMax, 53, 1, 20, 1, 60));
    e.push_back(w2);

    return e;
}

static int g_fail = 0;
static void Check(bool ok, const char* what, const std::string& diff = std::string()) {
    if (ok) { std::printf("  [PASS] %s\n", what); }
    else    { std::printf("  [FAIL] %s: %s\n", what, diff.c_str()); ++g_fail; }
}

int main(int argc, char** argv) {
    std::printf("== S1 波次 plist→bin→GetWavePlan 单测 ==\n");

    std::vector<battlecore::WaveDef> expected = Expected();

    // 测试 1: plist → WavePlan
    {
        PValue root;
        std::string err;
        Check(ParsePlist(kPlist, root, err), "ParsePlist 合成 plist", err);
        if (root.IsDict()) {
            std::vector<battlecore::WaveDef> got;
            Check(PlistToWavePlan(root, 11, got, err), "PlistToWavePlan", err);
            std::string diff;
            Check(WaveEq(got, expected, diff), "PlistToWavePlan == 手写期望", diff);
        }
    }

    // 测试 2: plist → tables → bin → CWaveTable.GetWavePlan == 手写期望
    {
        PValue root;
        std::string err;
        ParsePlist(kPlist, root, err);
        std::vector<battlecore::WaveDef> plan;
        PlistToWavePlan(root, 11, plan, err);

        TableData wt, wst;
        WavePlanToTables(11, plan, wt, wst);
        std::vector<TableData> tables;
        tables.push_back(wt);
        tables.push_back(wst);

        std::vector<uint8_t> bin;
        Check(BuildBin(tables, bin, err), "BuildBin", err);

        CBinReader rd;
        Check(rd.Load(bin.data(), bin.size()), "CBinReader.Load", rd.Error());

        CWaveTable cw(&rd);
        std::vector<battlecore::WaveDef> got;
        Check(cw.GetWavePlan(11, got), "CWaveTable.GetWavePlan(11)");
        std::string diff;
        Check(WaveEq(got, expected, diff), "bin 往返 == 手写期望", diff);

        // 回滚安全: wave/wave_spawn 是配置表, 不参与内核快照 —— 此处验证 GetWavePlan
        // 结果不含任何运行时状态 (纯 plan), 且缺关卡返回 false
        std::vector<battlecore::WaveDef> none;
        Check(!cw.GetWavePlan(99, none), "不存在的关卡返回 false");
    }

    // 测试 3 (可选): 真实 plist direct == bin 往返 (自洽)
    if (argc >= 3) {
        std::FILE* f = std::fopen(argv[1], "rb");
        if (f) {
            std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
            std::string xml(n > 0 ? (size_t)n : 0, '\0');
            if (n > 0) std::fread(&xml[0], 1, (size_t)n, f);
            std::fclose(f);

            int levelId = std::atoi(argv[2]);
            PValue root; std::string err;
            std::vector<battlecore::WaveDef> direct;
            Check(ParsePlist(xml, root, err) && PlistToWavePlan(root, (uint32_t)levelId, direct, err),
                  ("ParsePlist+PlistToWavePlan 真实关卡 " + std::to_string(levelId)).c_str(), err);

            TableData wt, wst;
            WavePlanToTables((uint32_t)levelId, direct, wt, wst);
            std::vector<TableData> tables; tables.push_back(wt); tables.push_back(wst);
            std::vector<uint8_t> bin;
            BuildBin(tables, bin, err);
            CBinReader rd; rd.Load(bin.data(), bin.size());
            std::vector<battlecore::WaveDef> roundtrip;
            CWaveTable(&rd).GetWavePlan((uint32_t)levelId, roundtrip);
            std::string diff;
            Check(WaveEq(direct, roundtrip, diff),
                  ("真实关卡 bin 往返自洽 (levelId=" + std::to_string(levelId) + ")").c_str(), diff);
            std::printf("    [info] 波数=%u\n", (unsigned)direct.size());
        } else {
            std::printf("  [WARN] 打不开 %s, 跳过真实数据测试\n", argv[1]);
        }
    }

    std::printf(g_fail ? "\n结果: FAIL (%d)\n" : "\n结果: ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
