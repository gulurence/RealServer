// =============================================================================
// test_prop.cpp — P2 道具/掉落 (Prop) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_prop.exe test_prop.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_prop.exe  (退出码 0=通过)
//
// 验证点 (P2 第四步「道具/掉落」):
//   1) 确定性: 同种子同输入 (英雄+怪+可掉落+金币道具+击杀弹) 跑 100 帧, 哈希一致。
//   2) 金币拾取: 金币进 90px → 英雄拾取 → coins+=5 + 道具移除。
//   3) 掉落物品回血: DROPGOODSTYPE_AND_HP 进 50px → 回 30% 最大生命。
//   4) 超时淡出: 远处金币 wait 生命耗尽 → disappear → 淡出后移除。
//   5) 掉落: 怪死亡 → 按掉落概率产金币/阳光道具 (RNG 顺序一致)。
//   6) 炸弹下落: bomb 速度+加速度下落 → 触底爆炸移除。
// 对应 Prop.cpp 的 update/aiProp_coins/aiProp_dropGoods/aiProp_bomb + Hero::hitProps
// 与 Sp01Boss.cpp 的 createCoins/createSunshine, 定点 Q16.16。
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

static void InitMap(CBattleCore& c) {
    std::vector<int64_t> ids = { 1001 };
    c.Init(0xB0B0, ids);
    c.MutableHeroes()[0].x = FxFromInt(600); c.MutableHeroes()[0].y = FxFromInt(240);
    c.MutableCollision().mapWidth = 1024;
    c.MutableCollision().mapHeight = 640;
}

// 带掉落配置的小怪 (drop 概率 100% → 必掉 3 金币 + 2 阳光)
static void AddDropper(CBattleCore& c, uint32_t id, int32_t x, int32_t y, int32_t hp) {
    Sp01State s;
    s.id = id; s.classId = kClassSp01Zombie; s.idInSp01Info = 0;
    s.x = FxFromInt(x); s.y = FxFromInt(y);
    s.radians = 0;
    s.hp = hp; s.maxHp = 10000; s.hpPre = hp;
    s.action = kSp01Walk; s.animDir = 0; s.remove = false;
    s.walkSpeed = FxFromInt(5);
    s.hurtToHero = FxFromInt(1);
    s.dropCoinsProbability = 100;
    s.dropCoinsMax = 3; s.dropCoinsMin = 3;
    s.dropSunShinePro = 100;
    s.dropSunShineMax = 2; s.dropSunShineMin = 2;
    c.MutableSp01s().push_back(s);
}

static void AddProp(CBattleCore& c, uint32_t id, uint8_t classId,
                    int32_t x, int32_t y, int32_t value) {
    PropState p;
    p.id = id; p.classId = classId; p.action = kPropInit;
    p.x = FxFromInt(x); p.y = FxFromInt(y); p.endX = p.x; p.endY = p.y;
    p.value = value; p.life = 500; p.fade = 0; p.remove = false;
    c.MutableProps().push_back(p);
}

static void AddBullet(CBattleCore& c, uint32_t id, uint8_t classId,
                      int32_t x, int32_t y, fx_t rotate, int32_t speed, int32_t hurt) {
    BulletState b;
    b.id = id; b.classId = classId; b.action = kBulletMove;
    b.x = FxFromInt(x); b.y = FxFromInt(y);
    b.rotate = rotate; b.speed = FxFromInt(speed); b.hurt = FxFromInt(hurt);
    c.MutableBullets().push_back(b);
}

int main() {
    printf("== 道具/掉落 (Prop) 单元测试 (P2) ==\n");

    // 测试 1: 确定性 — 英雄 + 可掉落怪 + 击杀弹 + 金币道具, 100 帧 → 同哈希
    {
        CBattleCore c1, c2;
        InitMap(c1); InitMap(c2);
        AddDropper(c1, 1, 300, 240, 1);
        AddDropper(c2, 1, 300, 240, 1);
        AddProp(c1, 1, kClassPropCoins, 610, 240, 5);
        AddProp(c2, 1, kClassPropCoins, 610, 240, 5);
        AddBullet(c1, 1, kClassBulletNormal, 280, 240, 0, 10, 10);
        AddBullet(c2, 1, kClassBulletNormal, 280, 240, 0, 10, 10);
        for (uint32_t f = 1; f <= 100; ++f) { c1.Tick(f); c2.Tick(f); }
        CHECK(c1.HashState() == c2.HashState(), "同种子同输入 100 帧后哈希一致");
    }

    // 测试 2: 金币拾取 — 英雄 90px 内 → coins+=5 + 道具移除
    {
        CBattleCore c; InitMap(c);
        AddProp(c, 1, kClassPropCoins, 610, 240, 5);
        c.Tick(1);
        CHECK(c.GetGame().coins == 5, "金币拾取 coins+=5 (实际 %d)", c.GetGame().coins);
        CHECK(c.GetProps().empty(), "金币拾取后道具移除 (残留 %zu)", c.GetProps().size());
    }

    // 测试 3: 掉落物品回血 — DROPGOODSTYPE_AND_HP 回 30% 最大生命
    {
        CBattleCore c; InitMap(c);
        c.MutableHeroes()[0].hp = 5000;   // 半血
        AddProp(c, 1, kDropGoodsHp, 610, 240, 0);
        c.Tick(1);
        CHECK(c.GetHeroes()[0].hp == 8000, "回血 30%% 最大生命 (实际 %d)", c.GetHeroes()[0].hp);
        CHECK(c.GetProps().empty(), "回血道具拾取后移除 (残留 %zu)", c.GetProps().size());
    }

    // 测试 4: 超时淡出 — 远处金币 life 耗尽 → disappear → 淡出后移除
    {
        CBattleCore c; InitMap(c);
        AddProp(c, 1, kClassPropCoins, 100, 100, 5);   // 距英雄 ~520px, 不会被拾取
        c.MutableProps()[0].life = 3;                  // 缩短 wait 生命加速测试
        for (uint32_t f = 1; f <= 220; ++f) c.Tick(f);
        CHECK(c.GetProps().empty(), "金币超时淡出后移除 (残留 %zu)", c.GetProps().size());
        CHECK(c.GetGame().coins == 0, "远处金币未被拾取 (实际 coins=%d)", c.GetGame().coins);
    }

    // 测试 5: 掉落 — 怪死亡 → 按掉落概率产 3 金币 + 2 阳光
    {
        CBattleCore c; InitMap(c);
        AddDropper(c, 1, 300, 240, 1);   // hp=1, 一击致死
        c.HurtSp01ById(1, kClassBulletNormal);   // 触发 changeToHurt → die → 掉落
        c.Tick(1);
        int coins = 0, sunshine = 0;
        for (const auto& p : c.GetProps()) {
            if (p.classId == kClassPropCoins) ++coins;
            else if (p.classId == kClassPropSunShine) ++sunshine;
        }
        CHECK(coins == 3, "掉落 3 金币 (实际 %d)", coins);
        CHECK(sunshine == 2, "掉落 2 阳光 (实际 %d)", sunshine);
    }

    // 测试 6: 炸弹下落 — velocity+acceleration 下落 → 触底爆炸移除
    {
        CBattleCore c; InitMap(c);
        PropState p;
        p.id = 1; p.classId = kClassPropBomb; p.action = kPropMove;
        p.x = FxFromInt(400); p.y = FxFromInt(300); p.endY = FxFromInt(200);
        p.velocity = 0; p.acceleration = 1; p.remove = false;
        c.MutableProps().push_back(p);
        c.Tick(1);
        CHECK(!c.GetProps().empty() && c.GetProps()[0].y < FxFromInt(300), "炸弹 y 下移 (实际 %d)", FxToIntRound(c.GetProps()[0].y));
        for (uint32_t f = 2; f <= 30; ++f) c.Tick(f);
        CHECK(c.GetProps().empty(), "炸弹触底爆炸移除 (残留 %zu)", c.GetProps().size());
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
