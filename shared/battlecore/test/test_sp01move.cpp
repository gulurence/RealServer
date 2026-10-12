// =============================================================================
// test_sp01move.cpp — P2 小怪 (Sp01) AI / 移动 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_sp01move.exe test_sp01move.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_sp01move.exe  (退出码 0=通过)
//
// 验证点 (P2 第二步「敌人 AI/移动」, normal 小怪):
//   1) 确定性: 同种子同输入跑 60 帧, 两端 HashState 逐位一致。
//   2) appear → walk 迁移 + 向最近英雄移动 (x 增大)。
//   3) 攻击距离内转 attack 且不移动 (bHurtHero 判定, 无 RNG)。
// 对应 Sp01.cpp 的 update/do_walk/do_attack/moveToNext/findHero, 定点 Q16.16。
// =============================================================================
#include "../BattleCore.h"

#include <cstdio>
#include <cstdlib>

using namespace battlecore;

static int g_fail = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            ++g_fail;                                       \
            printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); \
            if (g_fail > 20) { printf("... 已超过 20 处失败, 提前退出\n"); exit(1); } \
        }                                                   \
    } while (0)

// 构造「单英雄 + 单小怪」追踪场景 (小怪出生在英雄左侧, 延迟 0)
static void SetupChase(CBattleCore& c) {
    std::vector<int64_t> ids = { 1001 };
    c.Init(0xABCD, ids);
    HeroState& h = c.MutableHeroes()[0];
    h.x = FxFromInt(300); h.y = FxFromInt(240);
    c.MutableCollision().mapWidth = 1024;
    c.MutableCollision().mapHeight = 640;

    Sp01State s;
    s.id = 1; s.classId = kClassSp01Zombie; s.idInSp01Info = 0;
    s.x = FxFromInt(100); s.y = FxFromInt(240);
    s.radians = 0;
    s.hp = 60; s.maxHp = 100; s.hpPre = 60;
    s.action = kSp01Appear; s.animDir = 0; s.remove = false;
    s.walkSpeed = FxFromInt(5);
    s.hurtToHero = FxFromInt(1);
    c.MutableSp01s().push_back(s);
}

int main() {
    printf("== 小怪 AI/移动 单元测试 (P2) ==\n");

    // 测试 1: 确定性 — 同种子同输入 → 同哈希 (最关键)
    {
        CBattleCore c1, c2;
        SetupChase(c1); SetupChase(c2);
        for (uint32_t f = 1; f <= 60; ++f) { c1.Tick(f); c2.Tick(f); }
        CHECK(c1.HashState() == c2.HashState(), "同种子同输入 60 帧后哈希一致");
        CHECK(c1.GetFrame() == 60 && c2.GetFrame() == 60, "帧号推进到 60");
    }

    // 测试 2: appear → walk + 向英雄移动
    {
        CBattleCore c;
        SetupChase(c);
        c.Tick(1);
        CHECK(c.GetSp01s()[0].action == kSp01Walk, "appear(0 延迟) 一帧后转 walk (action=%d)",
              (int)c.GetSp01s()[0].action);
        const fx_t x1 = c.GetSp01s()[0].x;
        c.Tick(2);
        const Sp01State& s = c.GetSp01s()[0];
        CHECK(s.x > x1, "向英雄移动 x 增大 (%d -> %d)", FxToIntRound(x1), FxToIntRound(s.x));
        CHECK(FxAbs(s.y - FxFromInt(240)) <= FxFromInt(1), "直线追踪 y 基本不变");
    }

    // 测试 3: 攻击距离内 → 转 attack 且不移动 (无 RNG)
    {
        CBattleCore c;
        std::vector<int64_t> ids = { 1001 };
        c.Init(0x1234, ids);
        c.MutableHeroes()[0].x = FxFromInt(300); c.MutableHeroes()[0].y = FxFromInt(240);
        c.MutableCollision().mapWidth = 1024; c.MutableCollision().mapHeight = 640;

        Sp01State s;
        s.id = 1; s.classId = kClassSp01Zombie; s.idInSp01Info = 0;
        s.x = FxFromInt(270); s.y = FxFromInt(240);   // 距英雄 30 < 45
        s.radians = 0; s.hp = 60; s.maxHp = 100; s.hpPre = 60;
        s.action = kSp01Walk; s.walkSpeed = FxFromInt(5); s.hurtToHero = FxFromInt(1);
        c.MutableSp01s().push_back(s);

        c.Tick(1);
        const Sp01State& r = c.GetSp01s()[0];
        CHECK(r.action == kSp01Attack, "攻击距离内转 attack (action=%d)", (int)r.action);
        CHECK(r.x == FxFromInt(270), "转 attack 不移动 (x 保持 270)");
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
