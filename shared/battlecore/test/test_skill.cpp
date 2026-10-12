// =============================================================================
// test_skill.cpp — 全屏炸弹技能 (P2 full-screen bomb) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_skill.exe test_skill.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_skill.exe  (退出码 0=通过)
//
// 验证点:
//   1) 引信倒计时: 按下 skill_id=全屏炸弹 → bombFuse=10 每帧 -1, 第 10 帧结算伤害。
//   2) 半径判定: 以英雄为圆心半径 500 内受伤, 半径外不受伤。
//   3) 确定性: 同种子同技能输入 → 逐位一致哈希。
//   4) 回滚安全: 引信 (bombFuse) 入快照, 中途恢复后继续推进与不恢复一致。
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

// 全屏炸弹伤害 (BattleCore.cpp 的 kFullScreenBombHurt = 30 * kHpScale = 3000)
static const int32_t kBombHurt = 3000;

static void SetupCore(CBattleCore& c, uint32_t seed) {
    c.Init(seed, { 101 });                       // 英雄 0 出生在 (160, 240)
    c.MutableCollision().mapWidth = 1024;
    c.MutableCollision().mapHeight = 640;
}

// 放置一只可被命中的小怪 (action=walk 可受伤; walkSpeed=0 保持原地, 便于半径判定)
static void PlaceSp01(CBattleCore& c, uint32_t id, fx_t x, fx_t y) {
    Sp01State s;
    s.id = id;
    s.classId = kClassSp01Zombie;
    s.x = x; s.y = y;
    s.hp = 10000; s.maxHp = 10000; s.hpPre = 10000;
    s.action = kSp01Walk;
    s.walkSpeed = 0;   // 不动
    c.MutableSp01s().push_back(s);
}

static PlayerInput SkillInput() {
    PlayerInput in;
    in.player_id = 101;
    in.skill_id  = kSkillFullScreenBomb;
    return in;
}

int main() {
    printf("== 全屏炸弹技能 (P2 full-screen bomb) 单元测试 ==\n");

    // ---- 测试 1: 引信倒计时 (按下后第 10 帧结算) ----
    {
        CBattleCore c;
        SetupCore(c, 0x111);
        PlaceSp01(c, 1000, FxFromInt(260), FxFromInt(240));   // 距英雄 100, 半径内

        c.SubmitInput(SkillInput());
        c.Tick(1);
        CHECK(c.GetHeroes()[0].bombFuse == 9, "按下后第1帧引信=9 (实际 %d)", c.GetHeroes()[0].bombFuse);
        CHECK(c.GetSp01s()[0].hp == 10000, "引信未到, 怪未受伤");

        for (uint32_t f = 2; f <= 9; ++f) c.Tick(f);
        CHECK(c.GetHeroes()[0].bombFuse == 1, "第9帧引信=1 (实际 %d)", c.GetHeroes()[0].bombFuse);
        CHECK(c.GetSp01s()[0].hp == 10000, "引信未到, 怪未受伤");

        c.Tick(10);
        CHECK(c.GetHeroes()[0].bombFuse == 0, "第10帧引信=0 已结算 (实际 %d)", c.GetHeroes()[0].bombFuse);
        CHECK(c.GetSp01s()[0].hp < 10000, "第10帧炸弹结算, 怪受伤 (实际 hp=%d)", c.GetSp01s()[0].hp);
    }

    // ---- 测试 2: 半径判定 (500 内受伤 / 500 外不受伤) ----
    {
        CBattleCore c;
        SetupCore(c, 0x222);
        PlaceSp01(c, 1000, FxFromInt(260), FxFromInt(240));   // 距英雄 100 < 500
        PlaceSp01(c, 1001, FxFromInt(760), FxFromInt(240));   // 距英雄 600 > 500

        c.SubmitInput(SkillInput());
        for (uint32_t f = 1; f <= 10; ++f) c.Tick(f);

        CHECK(c.GetSp01s()[0].hp < 10000, "半径内怪受伤 (实际 hp=%d)", c.GetSp01s()[0].hp);
        CHECK(c.GetSp01s()[1].hp == 10000, "半径外怪不受伤 (实际 hp=%d)", c.GetSp01s()[1].hp);
    }

    // ---- 测试 3: 确定性 (同种子同技能 → 逐位一致) ----
    {
        CBattleCore a, b;
        SetupCore(a, 0x333);
        SetupCore(b, 0x333);
        PlaceSp01(a, 1000, FxFromInt(260), FxFromInt(240));
        PlaceSp01(b, 1000, FxFromInt(260), FxFromInt(240));
        a.SubmitInput(SkillInput());
        b.SubmitInput(SkillInput());
        for (uint32_t f = 1; f <= 15; ++f) { a.Tick(f); b.Tick(f); }
        CHECK(a.HashState() == b.HashState(), "同种子同技能逐位一致");
    }

    // ---- 测试 4: 回滚安全 (引信入快照, 恢复后继续推进一致) ----
    {
        CBattleCore a;
        SetupCore(a, 0x444);
        PlaceSp01(a, 1000, FxFromInt(260), FxFromInt(240));
        a.SubmitInput(SkillInput());
        for (uint32_t f = 1; f <= 5; ++f) a.Tick(f);   // 引信减到 5, 尚未结算
        CHECK(a.GetHeroes()[0].bombFuse == 5, "第5帧引信=5 (实际 %d)", a.GetHeroes()[0].bombFuse);
        std::string snap = a.SerializeState();
        uint64_t hashA = a.HashState();

        CBattleCore b;
        SetupCore(b, 0x444);
        CHECK(b.DeserializeState(snap), "DeserializeState 恢复快照");
        CHECK(b.HashState() == hashA, "恢复后哈希一致");
        CHECK(b.GetHeroes()[0].bombFuse == 5, "恢复后引信=5 (实际 %d)", b.GetHeroes()[0].bombFuse);

        for (uint32_t f = 6; f <= 15; ++f) { a.Tick(f); b.Tick(f); }
        CHECK(a.HashState() == b.HashState(), "恢复后继续推进逐位一致");
        CHECK(a.GetSp01s()[0].hp == b.GetSp01s()[0].hp, "两路炸弹伤害一致");
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
