// =============================================================================
// test_wave.cpp — 波次刷怪 (P2 wave spawn) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_wave.exe test_wave.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_wave.exe  (退出码 0=通过)
//
// 验证点:
//   1) 刷怪节奏: 按 startTime/interval 确定性出生 (出生进度由 waveTimer 唯一推导)。
//   2) 波次推进 + 胜利: 本波清场 → 下一波; 最后一波清空 → allWavesCleared。
//   3) 确定性: 同种子同波次计划 → 逐位一致哈希。
//   4) 回滚安全: 中途快照恢复后继续推进与不恢复一致 (waveTimer 入快照)。
// =============================================================================
#include "../BattleCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace battlecore;

static int g_fail = 0;

#define CHECK(cond, ...)                                          \
    do {                                                          \
        if (!(cond)) {                                            \
            ++g_fail;                                             \
            printf("FAIL: "); printf(__VA_ARGS__); printf("\n");   \
            if (g_fail > 20) { printf("... 已超过 20 处失败, 提前退出\n"); exit(1); } \
        }                                                         \
    } while (0)

static void SetupCore(CBattleCore& c, uint32_t seed) {
    c.Init(seed, { 101 });
    c.MutableCollision().mapWidth = 1024;
    c.MutableCollision().mapHeight = 640;
    FxPoint e1; e1.x = FxFromInt(512); e1.y = FxFromInt(100);
    FxPoint e2; e2.x = FxFromInt(200); e2.y = FxFromInt(300);
    c.MutableCollision().entries.push_back(e1);
    c.MutableCollision().entries.push_back(e2);
}

static WaveSpawnGroup G(uint8_t cls, int32_t count, int32_t interval, int32_t entry, int32_t start) {
    WaveSpawnGroup g;
    g.classId = cls; g.count = count; g.interval = interval; g.entryPoint = entry; g.startTime = start;
    return g;
}

// 清场: 把场上所有小怪标记 remove (等价于全部阵亡, 用于测试推进/胜利判定)
static void ClearSp01s(CBattleCore& c) {
    for (auto& s : c.MutableSp01s()) s.remove = true;
}

int main() {
    printf("== 波次刷怪 (P2 wave spawn) 单元测试 ==\n");

    // ---- 测试 1: 刷怪节奏 (startTime/interval) ----
    {
        CBattleCore c;
        SetupCore(c, 0x111);
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Zombie, 3, 10, 1, 0));  // 3 只, 每 10 帧, 第 0 帧起

        c.SetWavePlan(plan);

        c.Tick(1);
        CHECK(c.GetSp01s().size() == 1, "第0帧刷第1只 (实际 %u)", (unsigned)c.GetSp01s().size());
        if (!c.GetSp01s().empty())
            CHECK(c.GetSp01s()[0].x == FxFromInt(512) && c.GetSp01s()[0].y == FxFromInt(100),
                  "出生点取自 entries[0]");

        for (uint32_t f = 2; f <= 11; ++f) c.Tick(f);
        CHECK(c.GetSp01s().size() == 2, "第10帧刷第2只 (实际 %u)", (unsigned)c.GetSp01s().size());
        for (uint32_t f = 12; f <= 21; ++f) c.Tick(f);
        CHECK(c.GetSp01s().size() == 3, "第20帧刷第3只 (实际 %u)", (unsigned)c.GetSp01s().size());
        CHECK(c.GetWave().waveNum == 1 && !c.GetWave().allWavesCleared, "小怪在场不推进/不胜利");
    }

    // ---- 测试 2: 波次推进 + 胜利 ----
    {
        CBattleCore c;
        SetupCore(c, 0x222);
        std::vector<WaveDef> plan(2);
        plan[0].isKey = true; plan[0].groups.push_back(G(kClassSp01Zombie, 1, 0, 1, 0));
        plan[1].isKey = true; plan[1].groups.push_back(G(kClassSp01Boy, 2, 0, 1, 0));
        c.SetWavePlan(plan);

        c.Tick(1);
        CHECK(c.GetSp01s().size() == 1 && c.GetWave().waveNum == 1, "第1波出生 1 只");
        ClearSp01s(c);
        c.Tick(2);   // 清场移除
        c.Tick(3);   // 推进到第2波 (出生推迟到下一帧, 对齐原 darklayerba::initWaveData)
        CHECK(c.GetWave().waveNum == 2, "推进到第2波");
        CHECK(c.GetWave().xianShiWaveNum == 2, "显示波次 == 2");
        c.Tick(4);   // 第2波出生
        CHECK(c.GetSp01s().size() == 2 && c.GetWave().waveNum == 2, "第2波出生 2 只");
        ClearSp01s(c);
        c.Tick(5);   // 清场移除
        c.Tick(6);   // 最后一波清空 → 胜利
        CHECK(c.GetWave().allWavesCleared && c.GetSp01s().empty(), "全部波次清空 (胜利)");
    }

    // ---- 测试 3: 确定性 (同种子同计划 → 同哈希) ----
    {
        std::vector<WaveDef> plan(2);
        plan[0].isKey = true; plan[0].groups.push_back(G(kClassSp01Zombie, 2, 10, 1, 0));
        plan[1].isKey = true; plan[1].groups.push_back(G(kClassSp01Boy, 3, 5, 2, 3));
        CBattleCore a, b;
        SetupCore(a, 0x333);
        SetupCore(b, 0x333);
        a.SetWavePlan(plan);
        b.SetWavePlan(plan);
        for (uint32_t f = 1; f <= 60; ++f) {
            a.Tick(f);
            b.Tick(f);
            if (f == 30) { ClearSp01s(a); ClearSp01s(b); }
        }
        CHECK(a.HashState() == b.HashState(), "同种子同计划逐位一致");
    }

    // ---- 测试 4: 回滚安全 (中途快照恢复后继续推进一致) ----
    {
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Zombie, 3, 10, 1, 0));
        CBattleCore a;
        SetupCore(a, 0x444);
        a.SetWavePlan(plan);
        for (uint32_t f = 1; f <= 15; ++f) a.Tick(f);   // 已刷 2 只 (T=0,10)
        std::string snap = a.SerializeState();
        uint64_t hashA = a.HashState();

        CBattleCore b;
        SetupCore(b, 0x444);
        b.SetWavePlan(plan);          // 波次计划不入快照, 需重注入
        CHECK(b.DeserializeState(snap), "DeserializeState 恢复快照");
        CHECK(b.HashState() == hashA, "恢复后哈希一致");
        CHECK(b.GetWave().waveTimer == a.GetWave().waveTimer, "waveTimer 恢复一致");

        for (uint32_t f = 16; f <= 30; ++f) { a.Tick(f); b.Tick(f); }
        CHECK(a.HashState() == b.HashState(), "快照恢复后继续推进逐位一致");
        CHECK(a.GetSp01s().size() == 3 && b.GetSp01s().size() == 3, "第20帧刷第3只 (两路都到 3)");
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
