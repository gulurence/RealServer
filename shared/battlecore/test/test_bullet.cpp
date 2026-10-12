// =============================================================================
// test_bullet.cpp — P2 子弹 (Bullet) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_bullet.exe test_bullet.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_bullet.exe  (退出码 0=通过)
//
// 验证点 (P2 第二步「子弹」):
//   1) 确定性: 同种子同输入 (英雄+怪+普通弹+火箭筒) 跑 200 帧, 两端 HashState 一致。
//   2) 普通弹直线飞行 (x 按 speed 递增)。
//   3) 撞墙 → 移除。
//   4) 命中怪 → 怪掉血 (changeToHurt) + 子弹移除。
//   5) 狙击穿透: 穿过两个怪, 都掉血。
//   6) 火箭筒: 追踪 + 爆炸 AOE 伤害附近怪。
//   7) 炮台弹: 命中英雄 → 英雄掉血。
// 对应 Bullet.cpp 的 update/doBullet_normal/doBullet_huojiantong2/doPtBullet_normal
// 与 Sp01Boss.cpp 的 hurtSp01_from_rpg, 定点 Q16.16。
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

static void AddSp01(CBattleCore& c, uint32_t id, uint8_t classId, int32_t x, int32_t y, int32_t hp) {
    Sp01State s;
    s.id = id; s.classId = classId; s.idInSp01Info = 0;
    s.x = FxFromInt(x); s.y = FxFromInt(y);
    s.radians = 0;
    s.hp = hp; s.maxHp = 10000; s.hpPre = hp;
    s.action = kSp01Walk; s.animDir = 0; s.remove = false;
    s.walkSpeed = FxFromInt(5);
    s.hurtToHero = FxFromInt(1);
    c.MutableSp01s().push_back(s);
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
    printf("== 子弹 (Bullet) 单元测试 (P2) ==\n");

    // 测试 1: 确定性 — 同种子同输入 200 帧 → 同哈希
    {
        CBattleCore c1, c2;
        InitMap(c1); InitMap(c2);
        AddSp01(c1, 1, kClassSp01Zombie, 300, 240, 5000);
        AddSp01(c2, 1, kClassSp01Zombie, 300, 240, 5000);
        AddSp01(c1, 2, kClassSp01Zombie, 400, 240, 5000);
        AddSp01(c2, 2, kClassSp01Zombie, 400, 240, 5000);
        AddBullet(c1, 1, kClassBulletNormal, 200, 240, 0, 10, 10);
        AddBullet(c2, 1, kClassBulletNormal, 200, 240, 0, 10, 10);
        AddBullet(c1, 2, kClassBulletHuojiantong, 150, 200, 0, 8, 10);
        AddBullet(c2, 2, kClassBulletHuojiantong, 150, 200, 0, 8, 10);
        for (uint32_t f = 1; f <= 200; ++f) { c1.Tick(f); c2.Tick(f); }
        CHECK(c1.HashState() == c2.HashState(), "同种子同输入 200 帧后哈希一致");
        CHECK(c1.GetFrame() == 200 && c2.GetFrame() == 200, "帧号推进到 200");
    }

    // 测试 2: 普通弹直线飞行 (x 按 speed 递增, 无墙无怪)
    {
        CBattleCore c; InitMap(c);
        AddBullet(c, 1, kClassBulletNormal, 200, 240, 0, 10, 10);
        c.Tick(1);
        const BulletState& b = c.GetBullets()[0];
        CHECK(b.x == FxFromInt(210), "普通弹 x 递增 10 (实际 %d)", FxToIntRound(b.x));
        CHECK(b.y == FxFromInt(240), "普通弹 y 不变 (实际 %d)", FxToIntRound(b.y));
    }

    // 测试 3: 撞墙 → 移除
    {
        CBattleCore c; InitMap(c);
        FxRect wall; wall.x = FxFromInt(300); wall.y = FxFromInt(200);
        wall.w = FxFromInt(40); wall.h = FxFromInt(80);
        c.MutableCollision().bullet.push_back(wall);
        AddBullet(c, 1, kClassBulletNormal, 290, 240, 0, 20, 10);   // 下一帧 310 进墙
        for (uint32_t f = 1; f <= 10; ++f) c.Tick(f);
        CHECK(c.GetBullets().empty(), "撞墙后子弹移除 (残留 %zu)", c.GetBullets().size());
    }

    // 测试 4: 命中怪 → 怪掉血 + 子弹移除
    {
        CBattleCore c; InitMap(c);
        AddSp01(c, 1, kClassSp01Zombie, 300, 240, 5000);
        AddBullet(c, 1, kClassBulletNormal, 280, 240, 0, 10, 10);   // 下一帧 290 进受击盒
        c.Tick(1);
        const Sp01State& s = c.GetSp01s()[0];
        CHECK(s.hp < 5000, "怪掉血 (实际 %d)", s.hp);
        for (uint32_t f = 2; f <= 10; ++f) c.Tick(f);
        CHECK(c.GetBullets().empty(), "命中怪后子弹移除 (残留 %zu)", c.GetBullets().size());
    }

    // 测试 5: 狙击穿透 — 穿过两个怪, 都掉血
    {
        CBattleCore c; InitMap(c);
        AddSp01(c, 1, kClassSp01Zombie, 300, 240, 5000);
        AddSp01(c, 2, kClassSp01Zombie, 400, 240, 5000);
        AddBullet(c, 1, kClassBulletJuji, 280, 240, 0, 20, 10);   // 一路穿过两个怪
        for (uint32_t f = 1; f <= 12; ++f) c.Tick(f);
        bool allHurt = true;
        for (const auto& s : c.GetSp01s()) if (s.hp >= 5000) allHurt = false;
        CHECK(allHurt, "狙击穿透命中两个怪都掉血");
    }

    // 测试 6: 火箭筒 — 命中爆炸 AOE 伤害附近怪
    {
        CBattleCore c; InitMap(c);
        AddSp01(c, 1, kClassSp01Zombie, 320, 240, 5000);   // 爆炸半径 150 内
        AddBullet(c, 1, kClassBulletHuojiantong, 290, 240, 0, 10, 10);  // 下一帧进怪受击盒 → 爆炸
        for (uint32_t f = 1; f <= 10; ++f) c.Tick(f);
        const Sp01State& s = c.GetSp01s()[0];
        CHECK(s.hp < 5000, "火箭筒爆炸 AOE 命中附近怪 (实际 hp=%d)", s.hp);
        CHECK(c.GetBullets().empty(), "火箭筒爆炸后移除 (残留 %zu)", c.GetBullets().size());
    }

    // 测试 7: 炮台弹 — 命中英雄 → 英雄掉血
    {
        CBattleCore c; InitMap(c);
        AddBullet(c, 1, kClassBulletPaota, 580, 240, 0, 10, 5);   // 朝英雄(600,240) 飞
        for (uint32_t f = 1; f <= 5; ++f) c.Tick(f);
        const HeroState& h = c.GetHeroes()[0];
        CHECK(h.hp < 100 * 100, "炮台弹命中英雄掉血 (实际 %d)", h.hp);
    }

    // 测试 8: 近身开火 — 子弹出生点已在怪受击盒内 (枪口贴脸), 仍应命中 (回归: 整帧一步会
    // 先把出生点推出受击盒再检测, 导致贴脸永远打不中)
    {
        CBattleCore c; InitMap(c);
        AddSp01(c, 1, kClassSp01Zombie, 300, 240, 5000);   // 受击盒 [284,316]×[224,256]
        AddBullet(c, 1, kClassBulletNormal, 300, 240, 0, 20, 10);   // 出生点即怪中心 (盒内)
        c.Tick(1);
        const Sp01State& s = c.GetSp01s()[0];
        CHECK(s.hp < 5000, "出生点在受击盒内的子弹命中怪 (实际 hp=%d)", s.hp);
    }

    // 测试 9: 高速子弹隧道 — speed 40 > 受击盒 32, 整帧一步会「跳」过受击盒, 子步扫掠应命中
    {
        CBattleCore c; InitMap(c);
        AddSp01(c, 1, kClassSp01Zombie, 320, 240, 5000);   // 受击盒 [304,336]
        AddBullet(c, 1, kClassBulletNormal, 300, 240, 0, 40, 10);   // 300→340, 一步越过 [304,336]
        c.Tick(1);
        const Sp01State& s = c.GetSp01s()[0];
        CHECK(s.hp < 5000, "高速子弹(40px/帧)子步扫掠命中怪 (实际 hp=%d)", s.hp);
    }

    // 测试 10: 英雄开火 → 子弹出生在枪口 (回归: 原硬编码枪口偏移 20 过小, 子弹出生在枪身里,
    // 造成「武器与子弹初始位置不对齐」。修复: 枪型 → 枪口偏移, 手枪 55 / 狙击 68)
    {
        CBattleCore c;
        InitMap(c);
        HeroState& h = c.MutableHeroes()[0];
        h.x = FxFromInt(600); h.y = FxFromInt(240);
        h.gunType = kGunShouQiang;   // 手枪 → 枪口偏移 55
        PlayerInput in;
        in.player_id = 1001; in.move_dir = 0; in.shoot_dx = kFxOne; in.shoot_dy = 0; in.fire = true;
        c.SubmitInput(in);
        c.Tick(1);
        CHECK(c.GetBullets().size() == 1, "英雄开火产 1 发子弹 (实际 %zu)", c.GetBullets().size());
        if (!c.GetBullets().empty()) {
            const BulletState& b = c.GetBullets()[0];
            CHECK(b.x == FxFromInt(655), "手枪子弹出生在枪口 x=600+55=655 (实际 %d)", FxToIntRound(b.x));
            CHECK(b.y == FxFromInt(240), "手枪子弹 y 不变 (实际 %d)", FxToIntRound(b.y));
        }
    }
    {
        // 狙击 (kGunJuJi=3) → 枪口偏移 68
        CBattleCore c;
        InitMap(c);
        HeroState& h = c.MutableHeroes()[0];
        h.x = FxFromInt(600); h.y = FxFromInt(240);
        h.gunType = kGunJuJi;
        PlayerInput in;
        in.player_id = 1001; in.move_dir = 0; in.shoot_dx = kFxOne; in.shoot_dy = 0; in.fire = true;
        c.SubmitInput(in);
        c.Tick(1);
        if (!c.GetBullets().empty()) {
            const BulletState& b = c.GetBullets()[0];
            CHECK(b.x == FxFromInt(668), "狙击子弹出生在枪口 x=600+68=668 (实际 %d)", FxToIntRound(b.x));
        }
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
