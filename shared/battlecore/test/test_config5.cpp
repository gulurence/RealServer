// =============================================================================
// test_config5.cpp — 配置体系 C5 (帧同步内核实例化改从配置读) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_config5.exe test_config5.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_config5.exe  (退出码 0=通过)
//
// 验证点:
//   1) 配置注入: 波次组带 sp01Id, spawn 按 Sp01ConfigKey(classId, idInSp01Info) 查 Sp01Config 注入 hp/走速/碰撞伤害。
//   2) 回退默认: 无配置行 (sp01Id 不存在) → 回退旧占位默认, 行为不变。
//   3) 确定性: 同配置同种子 → 逐位一致哈希。
//   4) 回滚安全: Sp01Config 不入快照 (解析值随 Sp01State 序列化), 重注入后恢复继续推进一致。
// =============================================================================
#include "../BattleCore.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

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
    c.MutableCollision().entries.push_back(e1);
}

// 模拟配置层解析 Sp01Info 后注入的属性表 (键 = Sp01ConfigKey(classId, sp01Id))
static std::map<uint32_t, Sp01Config> MakeConfig() {
    Sp01Config zombie;
    zombie.maxHp = 300;
    zombie.walkSpeed = 229376;        // 3.5 << 16
    zombie.hurtToHero = FxFromInt(3); // 3 HP
    Sp01Config guard;
    guard.maxHp = 1000;
    guard.walkSpeed = 131072;         // 2.0 << 16
    guard.hurtToHero = FxFromInt(1);  // 1 HP
    std::map<uint32_t, Sp01Config> m;
    m[Sp01ConfigKey(kClassSp01Zombie, 1)] = zombie;
    m[Sp01ConfigKey(kClassSp01Boy,    2)] = guard;   // 守卫属性挂 (boy,2) 键, 测组合键取各自属性
    return m;
}

static WaveSpawnGroup G(uint8_t cls, int32_t sp01Id, int32_t count, int32_t interval) {
    WaveSpawnGroup g;
    g.classId = cls; g.sp01Id = sp01Id; g.count = count; g.interval = interval;
    g.entryPoint = 1; g.startTime = 0;
    return g;
}

int main() {
    printf("== 配置体系 C5 (内核实例化从配置读) 单元测试 ==\n");

    // ---- 测试 1: 配置注入 (sp01Id → Sp01Config 属性) ----
    {
        CBattleCore c;
        SetupCore(c, 0x111);
        c.SetSp01Config(MakeConfig());
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Zombie, 1, 1, 0));   // sp01Id=1 (僵尸)
        c.SetWavePlan(plan);
        c.Tick(1);

        CHECK(c.GetSp01s().size() == 1, "刷 1 只 (实际 %u)", (unsigned)c.GetSp01s().size());
        if (!c.GetSp01s().empty()) {
            const Sp01State& s = c.GetSp01s()[0];
            CHECK(s.idInSp01Info == 1, "idInSp01Info == 1 (实际 %d)", s.idInSp01Info);
            CHECK(s.classId == kClassSp01Zombie, "classId 仍来自波次组");
            CHECK(s.hp == 300 * 100, "hp == 30000 来自配置 (实际 %d)", s.hp);
            CHECK(s.maxHp == 300 * 100 && s.hpPre == s.hp, "maxHp/hpPre 同步");
            CHECK(s.walkSpeed == 229376, "walkSpeed == 3.5<<16 来自配置 (实际 %d)", s.walkSpeed);
            CHECK(s.hurtToHero == FxFromInt(3), "hurtToHero == FxFromInt(3) 来自配置");
        }
    }

    // ---- 测试 2: 回退默认 (sp01Id 无配置行) ----
    {
        CBattleCore c;
        SetupCore(c, 0x222);
        c.SetSp01Config(MakeConfig());
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Zombie, 99, 1, 0));   // sp01Id=99 不存在
        c.SetWavePlan(plan);
        c.Tick(1);

        CHECK(c.GetSp01s().size() == 1, "回退刷 1 只");
        if (!c.GetSp01s().empty()) {
            const Sp01State& s = c.GetSp01s()[0];
            CHECK(s.idInSp01Info == 99, "idInSp01Info 仍记录 sp01Id=99 (实际 %d)", s.idInSp01Info);
            CHECK(s.hp == 100 * 100, "回退 hp == 10000 占位默认 (实际 %d)", s.hp);
            CHECK(s.walkSpeed == FxFromInt(5), "回退 walkSpeed == FxFromInt(5)");
            CHECK(s.hurtToHero == FxFromInt(100), "回退 hurtToHero == FxFromInt(kHpScale)");
        }
    }

    // ---- 测试 3: 多 sp01Id 各取各的属性 ----
    {
        CBattleCore c;
        SetupCore(c, 0x333);
        c.SetSp01Config(MakeConfig());
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Zombie, 1, 1, 0));
        plan[0].groups.push_back(G(kClassSp01Boy, 2, 1, 0));   // sp01Id=2 (守卫)
        c.SetWavePlan(plan);
        c.Tick(1);

        CHECK(c.GetSp01s().size() == 2, "两组各刷 1 只 (实际 %u)", (unsigned)c.GetSp01s().size());
        if (c.GetSp01s().size() == 2) {
            CHECK(c.GetSp01s()[0].hp == 300 * 100 && c.GetSp01s()[0].idInSp01Info == 1,
                  "第1只 = 僵尸配置");
            CHECK(c.GetSp01s()[1].hp == 1000 * 100 && c.GetSp01s()[1].idInSp01Info == 2,
                  "第2只 = 守卫配置 (hp=%d)", c.GetSp01s()[1].hp);
        }
    }

    // ---- 测试 4: 确定性 (同配置同种子 → 同哈希) ----
    {
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Zombie, 1, 3, 10));
        CBattleCore a, b;
        SetupCore(a, 0x444);
        SetupCore(b, 0x444);
        a.SetSp01Config(MakeConfig());
        b.SetSp01Config(MakeConfig());
        a.SetWavePlan(plan);
        b.SetWavePlan(plan);
        for (uint32_t f = 1; f <= 30; ++f) { a.Tick(f); b.Tick(f); }
        CHECK(a.HashState() == b.HashState(), "同配置同种子逐位一致");
    }

    // ---- 测试 5: 回滚安全 (配置不入快照, 解析值随状态; 重注入后恢复一致) ----
    {
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Zombie, 1, 3, 10));
        CBattleCore a;
        SetupCore(a, 0x555);
        a.SetSp01Config(MakeConfig());
        a.SetWavePlan(plan);
        for (uint32_t f = 1; f <= 15; ++f) a.Tick(f);   // 已刷 2 只 (T=0,10)
        std::string snap = a.SerializeState();
        uint64_t hashA = a.HashState();

        CBattleCore b;
        SetupCore(b, 0x555);
        b.SetSp01Config(MakeConfig());   // 本局常量, 重注入 (与波次计划同理)
        b.SetWavePlan(plan);
        CHECK(b.DeserializeState(snap), "DeserializeState 恢复快照");
        CHECK(b.HashState() == hashA, "恢复后哈希一致");
        CHECK(b.GetSp01s().size() == a.GetSp01s().size(), "恢复后小怪数一致");

        for (uint32_t f = 16; f <= 30; ++f) { a.Tick(f); b.Tick(f); }
        CHECK(a.HashState() == b.HashState(), "恢复后继续推进逐位一致");
        CHECK(b.GetSp01s().size() == 3, "第20帧刷第3只 (实际 %u)", (unsigned)b.GetSp01s().size());
    }

    // ---- 测试 6: 组合键碰撞解析 (同 sp01Id 不同 classId → 各自配置, 不再互相覆盖) ----
    {
        std::map<uint32_t, Sp01Config> cfg;
        Sp01Config boyC;  boyC.maxHp  = 250; boyC.walkSpeed  = FxFromFloat(3.6f); boyC.hurtToHero  = FxFromFloat(0.5f);
        Sp01Config girlC; girlC.maxHp = 200; girlC.walkSpeed = FxFromFloat(3.6f); girlC.hurtToHero = FxFromFloat(0.5f);
        cfg[Sp01ConfigKey(kClassSp01Boy,  511)] = boyC;
        cfg[Sp01ConfigKey(kClassSp01Girl, 511)] = girlC;   // 同 sp01Id=511, 不同 classId → 不覆盖

        CBattleCore c;
        SetupCore(c, 0x666);
        c.SetSp01Config(cfg);
        std::vector<WaveDef> plan(1);
        plan[0].isKey = true;
        plan[0].groups.push_back(G(kClassSp01Boy,  511, 1, 0));
        plan[0].groups.push_back(G(kClassSp01Girl, 511, 1, 0));
        c.SetWavePlan(plan);
        c.Tick(1);

        CHECK(c.GetSp01s().size() == 2, "碰撞用例刷 2 只 (实际 %u)", (unsigned)c.GetSp01s().size());
        if (c.GetSp01s().size() == 2) {
            CHECK(c.GetSp01s()[0].hp == 250 * 100, "boy_511 hp=25000 (实际 %d)", c.GetSp01s()[0].hp);
            CHECK(c.GetSp01s()[1].hp == 200 * 100, "girl_511 hp=20000 (实际 %d)", c.GetSp01s()[1].hp);
        }
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
