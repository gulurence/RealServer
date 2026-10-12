// =============================================================================
// test_turret.cpp — P2 炮台 (Turret) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_turret.exe test_turret.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_turret.exe  (退出码 0=通过)
//
// 验证点 (P2 第三步「炮台」):
//   1) 确定性: 同种子同输入 (英雄+炮台, 炮台朝英雄持续开火) 跑 60 帧, 两端哈希一致。
//   2) 激活: appear(delay) → show → stand。
//   3) 开火: stand 且英雄在 hurtHeroDis 内 → attack + 产炮台弹 → 英雄掉血。
//   4) 反怪弹命中炮台 → 炮台掉血 (changeToHurt)。
//   5) 炮台 hp→0 → die → disappear → 移除。
//   6) 火箭筒爆炸 AOE 伤害附近炮台。
// 对应 Turret.cpp 的 update/KeyFrameEvent/animationDone/changeToHurt 与
// Sp01Boss.cpp 的 hurtSp01_from_rpg, 定点 Q16.16。
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

static void AddTurret(CBattleCore& c, uint32_t id, int32_t x, int32_t y, int32_t hp,
                      uint8_t action, int32_t hurtHeroDis, int32_t hurtToHero,
                      int32_t bulletSpeed, int32_t delay) {
    TurretState t;
    t.id = id; t.x = FxFromInt(x); t.y = FxFromInt(y);
    t.hp = hp; t.maxHp = 10000;
    t.action = action;
    t.hurtHeroDis = hurtHeroDis;
    t.hurtToHero = FxFromInt(hurtToHero);
    t.bulletSpeed = FxFromInt(bulletSpeed);
    t.delay = delay; t.active = true; t.remove = false;
    c.MutableTurrets().push_back(t);
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
    printf("== 炮台 (Turret) 单元测试 (P2) ==\n");

    // 测试 1: 确定性 — 英雄 + 炮台 (朝英雄持续开火) 60 帧 → 同哈希
    {
        CBattleCore c1, c2;
        InitMap(c1); InitMap(c2);
        AddTurret(c1, 1, 300, 240, 5000, kTurretStand, 500, 5, 10, 0);
        AddTurret(c2, 1, 300, 240, 5000, kTurretStand, 500, 5, 10, 0);
        for (uint32_t f = 1; f <= 60; ++f) { c1.Tick(f); c2.Tick(f); }
        CHECK(c1.HashState() == c2.HashState(), "同种子同输入 60 帧后哈希一致");
        CHECK(c1.GetFrame() == 60 && c2.GetFrame() == 60, "帧号推进到 60");
    }

    // 测试 2: 激活 appear(delay=5) → show → stand
    {
        CBattleCore c; InitMap(c);
        AddTurret(c, 1, 300, 240, 5000, kTurretAppear, 0, 5, 10, 5);
        for (uint32_t f = 1; f <= 5; ++f) c.Tick(f);
        CHECK(c.GetTurrets()[0].action == kTurretShow, "delay 5 帧后转 show (action=%d)", (int)c.GetTurrets()[0].action);
        for (uint32_t f = 6; f <= 20; ++f) c.Tick(f);
        CHECK(c.GetTurrets()[0].action == kTurretStand, "show 播完后转 stand (action=%d)", (int)c.GetTurrets()[0].action);
    }

    // 测试 3: 开火 — stand 且英雄在范围内 → attack + 炮台弹 → 英雄掉血
    {
        CBattleCore c; InitMap(c);
        AddTurret(c, 1, 300, 240, 5000, kTurretStand, 500, 5, 10, 0);
        c.Tick(1);
        CHECK(c.GetTurrets()[0].action == kTurretAttack, "stand 索敌命中转 attack (action=%d)", (int)c.GetTurrets()[0].action);
        CHECK(!c.GetBullets().empty(), "attack 起始开火产炮台弹 (弹数 %zu)", c.GetBullets().size());
        for (uint32_t f = 2; f <= 60; ++f) c.Tick(f);
        CHECK(c.GetHeroes()[0].hp < 100 * 100, "炮台弹命中英雄掉血 (实际 %d)", c.GetHeroes()[0].hp);
    }

    // 测试 4: 反怪弹命中炮台 → 炮台掉血
    {
        CBattleCore c; InitMap(c);
        AddTurret(c, 1, 300, 240, 5000, kTurretStand, 0, 5, 10, 0);   // hurtHeroDis=0 不反击
        AddBullet(c, 1, kClassBulletNormal, 280, 240, 0, 10, 10);
        c.Tick(1);
        CHECK(c.GetTurrets()[0].hp < 5000, "反怪弹命中炮台掉血 (实际 %d)", c.GetTurrets()[0].hp);
    }

    // 测试 5: 炮台 hp→0 → die → 移除
    {
        CBattleCore c; InitMap(c);
        AddTurret(c, 1, 300, 240, 1, kTurretStand, 0, 5, 10, 0);      // hp=1, 一击致死
        AddBullet(c, 1, kClassBulletNormal, 280, 240, 0, 10, 10);
        c.Tick(1);
        CHECK(c.GetTurrets()[0].hp <= 0, "炮台 hp 归 0 (实际 %d)", c.GetTurrets()[0].hp);
        CHECK(c.GetTurrets()[0].action == kTurretDie, "炮台转 die (action=%d)", (int)c.GetTurrets()[0].action);
        for (uint32_t f = 2; f <= 40; ++f) c.Tick(f);
        CHECK(c.GetTurrets().empty(), "炮台 die→disappear 后移除 (残留 %zu)", c.GetTurrets().size());
    }

    // 测试 6: 火箭筒爆炸 AOE 伤害附近炮台
    {
        CBattleCore c; InitMap(c);
        AddTurret(c, 1, 320, 240, 5000, kTurretStand, 0, 5, 10, 0);   // 爆炸半径 150 内
        AddBullet(c, 1, kClassBulletHuojiantong, 290, 240, 0, 10, 10);
        for (uint32_t f = 1; f <= 10; ++f) c.Tick(f);
        CHECK(c.GetTurrets()[0].hp < 5000, "火箭筒 AOE 命中附近炮台 (实际 hp=%d)", c.GetTurrets()[0].hp);
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
