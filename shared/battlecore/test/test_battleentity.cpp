// =============================================================================
// test_battleentity.cpp — BattleEntity.h / CBattleCore 数据模型 + 序列化单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /Fe:test_battleentity.exe test_battleentity.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_battleentity.exe  (退出码 0=通过)
//
// 验证点 (P1 验收):
//   1) 全实体类型填充 → SerializeState → DeserializeState 往返逐字段一致。
//   2) 往返前后 HashState 逐位一致。
//   3) 坏数据 (魔数错/截断) 被 DeserializeState 拒绝。
// =============================================================================
#include "../BattleCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

// 填充一个含全部实体类型的核心 (确定性内容)
static void PopulateCore(CBattleCore& core) {
    std::vector<int64_t> ids = { 1001, 1002 };
    CHECK(core.Init(0xDEADBEEF, ids), "Init 2 玩家");

    // 地图碰撞 (对象层矩形)
    MapCollision& mc = core.MutableCollision();
    mc.mapWidth = 1024; mc.mapHeight = 640;
    {
        FxRect r; r.x = FxFromInt(200); r.y = FxFromInt(100); r.w = FxFromInt(64); r.h = FxFromInt(32);
        mc.people.push_back(r);
    }
    {
        FxRect r; r.x = FxFromInt(500); r.y = FxFromInt(300); r.w = FxFromInt(128); r.h = FxFromInt(96);
        mc.people.push_back(r);
        mc.bullet.push_back(r);
    }

    // 波次状态
    WaveState& wv = core.MutableWave();
    wv.waveNum = 3; wv.xianShiWaveNum = 3; wv.gamePauseTime = FxFromInt(2) + kFxHalf;
    wv.waveTimer = 42; wv.allWavesCleared = true;

    // 小怪
    {
        Sp01State s;
        s.id = 100; s.classId = kClassSp01Zombie; s.idInSp01Info = 9;
        s.x = FxFromFloat(123.5f); s.y = FxFromFloat(-45.25f); s.radians = kFxPiOver4;
        s.hp = 60; s.maxHp = 100; s.hpPre = 60;
        s.action = kSp01Walk; s.animDir = 3; s.remove = false;
        s.walkSpeed = FxFromFloat(3.75f); s.moveBackSpeed = FxFromFloat(8.0f);
        s.moveBackRadians = FxNeg(kFxPiOver2); s.moveBackAllCount = 12;
        s.moveBackFromGun = true; s.stopMoveTemp = false;
        s.sleepWalkingCount = 40; s.variationCount = 2; s.residueTimeOrCount = 7;
        s.troopsTiming = 30; s.sp01PauseCount = 5; s.stopTimeCount = 0;
        s.stopFindHeroUntilGetNextPoint = true; s.isFindHeroInThisFPS = false;
        s.hurtToHero = FxFromFloat(1.5f);
        s.dropCoinsProbability = 50; s.dropCoinsMax = 20; s.dropCoinsMin = 5;
        s.dropHuoJianTongPro = 10; s.dropJiaTeLinPro = 15; s.dropSunShinePro = 8;
        s.dropSunShineMax = 3; s.dropSunShineMin = 1; s.andLight = 0;
        s.nextX = FxFromInt(200); s.nextY = FxFromInt(100); s.hasNextPoint = true;
        core.MutableSp01s().push_back(s);
    }

    // 子弹 (含狙击已命中列表)
    {
        BulletState b;
        b.id = 200; b.classId = kClassBulletJuji; b.action = kBulletMove;
        b.x = FxFromFloat(64.5f); b.y = FxFromFloat(32.125f); b.rotate = kFxPi;
        b.speed = FxFromFloat(12.5f); b.hurt = FxFromFloat(45.0f);
        b.lifeLength = 60; b.sleepTime = 0; b.remove = false;
        b.hasHurtIds = { 100, 101, 102 };
        core.MutableBullets().push_back(b);
    }

    // 炮台
    {
        TurretState t;
        t.id = 300; t.x = FxFromInt(400); t.y = FxFromInt(300); t.aim = FxNeg(kFxPiOver4);
        t.hp = 80; t.maxHp = 80; t.action = kTurretAttack;
        t.hurtHeroDis = 10; t.delay = 0; t.active = true; t.remove = false;
        core.MutableTurrets().push_back(t);
    }

    // 道具
    {
        PropState p;
        p.id = 400; p.classId = kClassPropCoins; p.action = kPropMoveToHero;
        p.x = FxFromInt(90); p.y = FxFromInt(80); p.endX = FxFromInt(160); p.endY = FxFromInt(240);
        p.value = 25; p.velocity = 4; p.acceleration = 1; p.delay = 0; p.life = 120; p.remove = false;
        core.MutableProps().push_back(p);
    }
}

// 逐字段校验两个核心状态一致 (通过 GetX 只读访问器)
static void CheckStateEqual(const CBattleCore& a, const CBattleCore& b) {
    CHECK(a.HashState() == b.HashState(), "往返后 HashState 逐位一致");
    CHECK(a.GetSeed() == b.GetSeed() && a.GetFrame() == b.GetFrame(), "seed/frame 一致");

    const auto& ha = a.GetHeroes(); const auto& hb = b.GetHeroes();
    CHECK(ha.size() == hb.size() && ha.size() == 2, "英雄数量 2");
    if (ha.size() == hb.size()) {
        for (size_t i = 0; i < ha.size(); ++i) {
            CHECK(ha[i].id == hb[i].id && ha[i].player_id == hb[i].player_id, "英雄 %d id/player_id", (int)i);
            CHECK(ha[i].x == hb[i].x && ha[i].y == hb[i].y, "英雄 %d 坐标", (int)i);
            CHECK(ha[i].hp == hb[i].hp && ha[i].maxHp == hb[i].maxHp, "英雄 %d HP", (int)i);
        }
    }

    const auto& sa = a.GetSp01s(); const auto& sb = b.GetSp01s();
    CHECK(sa.size() == sb.size() && sa.size() == 1, "小怪数量 1");
    if (sa.size() == 1 && sb.size() == 1) {
        const Sp01State& x = sa[0]; const Sp01State& y = sb[0];
        CHECK(x.x == y.x && x.y == y.y && x.radians == y.radians, "小怪 坐标/弧度");
        CHECK(x.hp == y.hp && x.hpPre == y.hpPre && x.maxHp == y.maxHp, "小怪 HP");
        CHECK(x.walkSpeed == y.walkSpeed && x.hurtToHero == y.hurtToHero, "小怪 速度/伤害");
        CHECK(x.dropCoinsProbability == y.dropCoinsProbability &&
              x.dropSunShineMin == y.dropSunShineMin, "小怪 掉落配置");
        CHECK(x.nextX == y.nextX && x.nextY == y.nextY && x.hasNextPoint == y.hasNextPoint, "小怪 寻路点");
    }

    const auto& ba = a.GetBullets(); const auto& bb = b.GetBullets();
    CHECK(ba.size() == bb.size() && ba.size() == 1, "子弹数量 1");
    if (ba.size() == 1 && bb.size() == 1) {
        const BulletState& x = ba[0]; const BulletState& y = bb[0];
        CHECK(x.rotate == y.rotate && x.speed == y.speed && x.hurt == y.hurt, "子弹 角度/速度/伤害");
        CHECK(x.hasHurtIds.size() == y.hasHurtIds.size() && x.hasHurtIds == y.hasHurtIds, "子弹 已命中列表");
    }

    const auto& ta = a.GetTurrets(); const auto& tb = b.GetTurrets();
    CHECK(ta.size() == tb.size() && ta.size() == 1, "炮台数量 1");
    if (ta.size() == 1 && tb.size() == 1) {
        CHECK(ta[0].aim == tb[0].aim && ta[0].hurtHeroDis == tb[0].hurtHeroDis, "炮台 瞄准/距离");
        CHECK(ta[0].active == tb[0].active && ta[0].remove == tb[0].remove, "炮台 状态");
    }

    const auto& pa = a.GetProps(); const auto& pb = b.GetProps();
    CHECK(pa.size() == pb.size() && pa.size() == 1, "道具数量 1");
    if (pa.size() == 1 && pb.size() == 1) {
        CHECK(pa[0].value == pb[0].value && pa[0].velocity == pb[0].velocity, "道具 value/velocity");
        CHECK(pa[0].endX == pb[0].endX && pa[0].endY == pb[0].endY, "道具 终点");
    }

    const MapCollision& ga = a.GetCollision(); const MapCollision& gb = b.GetCollision();
    CHECK(ga.mapWidth == gb.mapWidth && ga.mapHeight == gb.mapHeight, "碰撞 地图尺寸");
    CHECK(ga.people.size() == gb.people.size() && ga.bullet.size() == gb.bullet.size(), "碰撞 矩形数量");
    if (ga.people.size() == gb.people.size() && !ga.people.empty()) {
        CHECK(ga.people[0].x == gb.people[0].x && ga.people[0].w == gb.people[0].w, "碰撞 矩形字段");
    }
    CHECK(ga.ContainsPeople(FxFromInt(210), FxFromInt(110)), "碰撞 点查询命中");
    CHECK(!ga.ContainsPeople(FxFromInt(10), FxFromInt(10)), "碰撞 点查询未命中");
    CHECK(ga.IntersectsPeople(FxRect{FxFromInt(210), FxFromInt(110), FxFromInt(8), FxFromInt(8)}), "碰撞 矩形相交");
    CHECK(ga.InMap(FxFromInt(500), FxFromInt(300)) && !ga.InMap(0, 0), "碰撞 地图边界");

    const WaveState& wa = a.GetWave(); const WaveState& wb = b.GetWave();
    CHECK(wa.waveNum == wb.waveNum && wa.gamePauseTime == wb.gamePauseTime, "波次 序号/计时");
    CHECK(wa.waveTimer == wb.waveTimer && wa.allWavesCleared == wb.allWavesCleared, "波次 计时/胜利标志");
}

int main() {
    printf("== BattleEntity / CBattleCore 序列化单元测试 ==\n");

    // 1) 填充 → 序列化 → 反序列化 → 往返一致
    CBattleCore core1;
    PopulateCore(core1);
    std::string blob = core1.SerializeState();
    printf("[序列化字节数: %d]\n", (int)blob.size());

    CBattleCore core2;
    CHECK(core2.DeserializeState(blob), "DeserializeState 成功");
    CheckStateEqual(core1, core2);

    // 2) 再序列化一次, 字节级稳定 (幂等: 反序列化结果与原串完全一致)
    std::string blob2 = core2.SerializeState();
    CHECK(blob == blob2, "序列化幂等 (重序列化字节一致)");

    // 3) 坏数据拒绝
    CBattleCore core3;
    CHECK(!core3.DeserializeState(std::string("short")), "拒绝截断数据");
    std::string badMagic = blob;
    badMagic[0] = (char)0xFF;   // 破坏魔数
    CHECK(!core3.DeserializeState(badMagic), "拒绝错误魔数");

    // 4) Tick 推进后 (玩具逻辑) 状态仍可序列化往返
    {
        PlayerInput in;
        in.player_id = 1001; in.move_dir = 3; in.shoot_dx = kFxOne; in.shoot_dy = 0; in.fire = true;
        core1.SubmitInput(in);
        core1.Tick(1);
        std::string s = core1.SerializeState();
        CBattleCore c4;
        CHECK(c4.DeserializeState(s), "Tick 后往返成功");
        CHECK(c4.HashState() == core1.HashState(), "Tick 后往返 hash 一致");
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
