// =============================================================================
// test_heromove.cpp — P2 英雄移动 + 地图边界 + tile 碰撞 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_heromove.exe test_heromove.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_heromove.exe  (退出码 0=通过)
//
// 验证点 (P2 第一步「英雄移动 + tile 碰撞」):
//   1) 无碰撞矩形时按 walkSpeed 直线移动 (每帧精确 10px)。
//   2) 撞墙被阻挡, 坐标不变 (点撞矩形 = CCRect 闭区间语义)。
//   3) 对角撞墙沿墙轴分离滑行 (x 挡 / y 滑)。
//   4) 地图边界裁剪 (maxX = mapWidth - 20)。
// 对应 Hero.cpp 的 changeHeroPos + checkCollidesToAddPos, 定点 Q16.16。
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

// 复用核心同款定点运算计算期望位移 (自文档化, 避免魔法数)
static void MoveDelta(uint32_t dir, fx_t walkSpeed, fx_t& addX, fx_t& addY) {
    fx_t ux = 0, uy = 0;
    DirToUnitVectorFx(dir, ux, uy);
    fx_t step = FxMul(walkSpeed, kFxOne);   // speedMul = 1.0
    addX = FxMul(ux, step);
    addY = FxMul(uy, step);
}

int main() {
    printf("== 英雄移动 + 碰撞 单元测试 (P2) ==\n");

    // --- 测试 1: 自由移动 (无碰撞矩形) ---
    {
        CBattleCore c;
        std::vector<int64_t> ids = { 1001 };
        CHECK(c.Init(0x1111, ids), "Init 1 玩家");
        HeroState& h = c.MutableHeroes()[0];
        h.x = FxFromInt(160); h.y = FxFromInt(240); h.walkSpeed = FxFromInt(10);
        c.MutableCollision().mapWidth = 1024;
        c.MutableCollision().mapHeight = 640;

        PlayerInput in; in.player_id = 1001; in.move_dir = 3;   // 右
        c.SubmitInput(in); c.Tick(1);
        CHECK(h.x == FxFromInt(170), "自由移动 x=170 (实际 %d)", FxToIntRound(h.x));
        CHECK(h.y == FxFromInt(240), "自由移动 y=240 (实际 %d)", FxToIntRound(h.y));
    }

    // --- 测试 2: 撞墙阻挡 (点撞矩形, 闭区间) ---
    {
        CBattleCore c;
        std::vector<int64_t> ids = { 1001 };
        c.Init(0x2222, ids);
        HeroState& h = c.MutableHeroes()[0];
        h.x = FxFromInt(290); h.y = FxFromInt(240); h.walkSpeed = FxFromInt(10);
        MapCollision& mc = c.MutableCollision();
        mc.mapWidth = 1024; mc.mapHeight = 640;
        FxRect wall; wall.x = FxFromInt(300); wall.y = FxFromInt(200); wall.w = FxFromInt(40); wall.h = FxFromInt(80);
        mc.people.push_back(wall);

        PlayerInput in; in.player_id = 1001; in.move_dir = 3;   // 右, 目标 300 进入墙
        c.SubmitInput(in); c.Tick(1);
        CHECK(h.x == FxFromInt(290), "撞墙 x 保持 290 (实际 %d)", FxToIntRound(h.x));
        CHECK(h.y == FxFromInt(240), "撞墙 y 保持 240 (实际 %d)", FxToIntRound(h.y));
    }

    // --- 测试 3: 对角撞墙沿墙轴分离滑行 ---
    {
        CBattleCore c;
        std::vector<int64_t> ids = { 1001 };
        c.Init(0x3333, ids);
        HeroState& h = c.MutableHeroes()[0];
        h.x = FxFromInt(295); h.y = FxFromInt(250); h.walkSpeed = FxFromInt(10);
        MapCollision& mc = c.MutableCollision();
        mc.mapWidth = 1024; mc.mapHeight = 640;
        FxRect wall; wall.x = FxFromInt(300); wall.y = FxFromInt(200); wall.w = FxFromInt(40); wall.h = FxFromInt(80);
        mc.people.push_back(wall);

        PlayerInput in; in.player_id = 1001; in.move_dir = 4;   // 右下 (撞 x, 沿 y 滑)
        c.SubmitInput(in); c.Tick(1);

        fx_t addX = 0, addY = 0;
        MoveDelta(4, FxFromInt(10), addX, addY);
        CHECK(h.x == FxFromInt(295), "滑动 x 被挡保持 295 (实际 %d)", FxToIntRound(h.x));
        CHECK(h.y == FxFromInt(250) + addY, "滑动 y 沿墙下移 (实际 %d)", FxToIntRound(h.y));
        CHECK(!mc.ContainsPeople(h.x, h.y), "滑动后未进入墙");
    }

    // --- 测试 4: 地图边界裁剪 (maxX = mapWidth - 20) ---
    {
        CBattleCore c;
        std::vector<int64_t> ids = { 1001 };
        c.Init(0x4444, ids);
        HeroState& h = c.MutableHeroes()[0];
        h.x = FxFromInt(1010); h.y = FxFromInt(240); h.walkSpeed = FxFromInt(10);
        c.MutableCollision().mapWidth = 1024;
        c.MutableCollision().mapHeight = 640;

        PlayerInput in; in.player_id = 1001; in.move_dir = 3;   // 右, 目标 1020 > 1004
        c.SubmitInput(in); c.Tick(1);
        CHECK(h.x == FxFromInt(1010), "边界 x 保持 1010 (实际 %d)", FxToIntRound(h.x));
        CHECK(h.y == FxFromInt(240), "边界 y 保持 240 (实际 %d)", FxToIntRound(h.y));
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
