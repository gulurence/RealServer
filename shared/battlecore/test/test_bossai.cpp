// =============================================================================
// test_bossai.cpp — P2 大怪 (boss) 状态机 + 伤害/HP 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_bossai.exe test_bossai.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_bossai.exe  (退出码 0=通过)
//
// 验证点 (P2 第一步「boss 大怪状态机 → 伤害/HP」):
//   1) 确定性: 同种子同输入跑 400 帧 (spTeacher 完整 egg/fenlie/重生 + 掉落 RNG),
//      两端 HashState 逐位一致。
//   2) spTeacher hp→0 → 变蛋 (variationCount 0→1, ToEgg)。
//   3) guard 过 70% 阈值 → EQUIP (residueTimeOrCount=300)。
//   4) teacher Troops 每 20 帧出 1 个 teacherChild。
// 对应 Sp01.cpp 的 changeToHurt/animationDone/ai_big_guard/ai_big_teacher/
// ai_big_spTeacher 与 Sp01Boss.cpp 的 createCoins/createSunshine/createASp01_SPTeacherChild,
// 定点 Q16.16。
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

// 构造「单英雄 + 单 boss」场景 (boss 在英雄左侧 40px)
static void SetupBoss(CBattleCore& c, uint8_t classId, int32_t hp, int32_t maxHp,
                      int32_t variation, uint8_t action) {
    std::vector<int64_t> ids = { 1001 };
    c.Init(0xABCD, ids);
    HeroState& h = c.MutableHeroes()[0];
    h.x = FxFromInt(300); h.y = FxFromInt(240);
    c.MutableCollision().mapWidth = 1024;
    c.MutableCollision().mapHeight = 640;

    Sp01State s;
    s.id = 1; s.classId = classId; s.idInSp01Info = 0;
    s.x = FxFromInt(260); s.y = FxFromInt(240);
    s.radians = 0;
    s.hp = hp; s.maxHp = maxHp; s.hpPre = hp;
    s.action = action; s.animDir = 0; s.remove = false;
    s.walkSpeed = FxFromInt(5);
    s.hurtToHero = FxFromInt(1);
    s.variationCount = variation;
    s.residueTimeOrCount = 0;
    s.troopsTiming = 0;
    c.MutableSp01s().push_back(s);
}

int main() {
    printf("== boss 大怪状态机 + 伤害/HP 单元测试 (P2) ==\n");

    // 测试 1: 确定性 — 同种子同输入 400 帧 → 同哈希 (最关键; 覆盖 egg/fenlie/重生/掉落 RNG)
    {
        CBattleCore c1, c2;
        SetupBoss(c1, kClassSp01BigSPTeacher, 5000, 10000, 0, kSp01Walk);
        SetupBoss(c2, kClassSp01BigSPTeacher, 5000, 10000, 0, kSp01Walk);
        for (uint32_t f = 1; f <= 400; ++f) {
            c1.HurtSp01ById(1, kClassBulletNormal);
            c2.HurtSp01ById(1, kClassBulletNormal);
            c1.Tick(f);
            c2.Tick(f);
        }
        CHECK(c1.HashState() == c2.HashState(), "同种子同输入 400 帧后哈希一致");
        CHECK(c1.GetFrame() == 400 && c2.GetFrame() == 400, "帧号推进到 400");
    }

    // 测试 2: spTeacher hp→0 → 变蛋 (variationCount 0→1, ToEgg)
    {
        CBattleCore c;
        SetupBoss(c, kClassSp01BigSPTeacher, 1, 10000, 0, kSp01Walk);
        c.HurtSp01ById(1, kClassBulletNormal);
        const Sp01State& s = c.GetSp01s()[0];
        CHECK(s.hp == 0, "spTeacher hp 归 0 (实际 %d)", s.hp);
        CHECK(s.variationCount == 1, "spTeacher variationCount 0→1 (实际 %d)", s.variationCount);
        CHECK(s.action == kSp01BSpTeacherToEgg, "spTeacher 转 ToEgg (action=%d)", (int)s.action);
    }

    // 测试 3: guard 过 70% 阈值 → EQUIP (residueTimeOrCount=300)
    {
        CBattleCore c;
        SetupBoss(c, kClassSp01BigGuard, 7100, 10000, 0, kSp01Walk);
        c.HurtSp01ById(1, kClassBulletNormal);
        const Sp01State& s = c.GetSp01s()[0];
        CHECK(s.action == kSp01BGuardEquip, "guard 过 70%% 阈值转 EQUIP (action=%d)", (int)s.action);
        CHECK(s.residueTimeOrCount == 300, "guard EQUIP 时长 300 帧 (实际 %d)", s.residueTimeOrCount);
    }

    // 测试 4: teacher Troops 每 20 帧出 1 个 teacherChild
    {
        CBattleCore c;
        SetupBoss(c, kClassSp01BigTeacher, 10000, 10000, 0, kSp01BTeacherTroops);
        Sp01State& t = c.MutableSp01s()[0];
        t.residueTimeOrCount = 5;
        t.troopsTiming = 0;
        for (uint32_t f = 1; f <= 20; ++f) c.Tick(f);
        bool foundChild = false;
        for (const auto& s : c.GetSp01s())
            if (s.classId == kClassSp01BigTeacherChild) foundChild = true;
        CHECK(foundChild, "teacher Troops 20 帧出 1 个 teacherChild");
        CHECK(c.GetSp01s().size() >= 2, "teacher + 子实体至少 2 个 (实际 %zu)", c.GetSp01s().size());
    }

    // 测试 5: 小怪攻击英雄 → 英雄掉血 (回归: 原动画 KeyFrameEvent 命中回调在核心里未接线,
    // 怪物贴脸攻击英雄不掉血。修复: Sp01AnimationDone 的攻击分支接 HeroDoHurt)
    {
        CBattleCore c;
        SetupBoss(c, kClassSp01Zombie, 5000, 5000, 0, kSp01Attack);   // 怪在英雄左 40px, 已贴脸
        const int32_t initHp = c.GetHeroes()[0].hp;
        for (uint32_t f = 1; f <= 200; ++f) c.Tick(f);   // 20 帧/击, 约 10 击; 5% 闪避也必中
        const HeroState& h = c.GetHeroes()[0];
        CHECK(h.hp < initHp, "小怪攻击英雄掉血 (初始 %d → 实际 %d)", initHp, h.hp);
        CHECK(h.hasGetHurtNum > 0, "英雄受击计数 > 0 (实际 %d)", h.hasGetHurtNum);
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
