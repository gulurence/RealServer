/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: BattleGame.cpp
      Purpose:   战斗数据模块实现(全部玩法规则)

      BattleGame 实现: 英雄移动开火、波次刷怪、Sp01 普通小怪与 boss 状态机、子弹扫掠命中、道具拾取、炮台与全屏炸弹
      含快照序列化与 FNV-1a 哈希的游戏状态部分, PRNG 与 id 分配器经 Step/HurtSp01ById 参数与引擎解耦
  *****************************************************************/

// =============================================================================
// BattleGame.cpp — 战斗数据模块实现 (实体数据 + 配置 + 全部玩法规则)
//
// 从原 CBattleCore (BattleCore.cpp) 整体迁出: 移动/开火/波次/AI/子弹/道具/炮台/技能
// 的全部确定性规则 + 实体容器 + 配置注入 + 快照序列化/哈希的「游戏状态」部分。
// 帧同步引擎标量 (种子/PRNG/实体 id 分配器/输入队列) 留在 CBattleCore (见 BattleCore.cpp),
// 经 Step/HurtSp01ById 的参数 (CBattleRng&, uint32_t&) 与本模块解耦。
// =============================================================================
#include "BattleGame.h"
#include "FixedTrig.h"

#include <cstring>
#include <algorithm>

namespace battlecore {

// ---------------------------------------------------------------------------
// P2 占位配置 (伤害/HP/时长数值待 C5 从配置 bin 读取, 见 docs/config-system-design.md)
// ---------------------------------------------------------------------------
// HP/伤害统一 ×100 (centi): 原 Hero 是 float HP、Sp01 是 int HP, 移植后统一 int32 并按
// 100 缩放, 使 0.5/0.15/0.7/0.4 等伤害系数可整数表示 (design §1.3)。注意: 这是对原
// 代码「怪物伤害 alter_m_nHP(int) 截断到 1 HP」的有意精度提升 (R1 差分记入有意差异)。
static const int32_t kHpScale = 100;

static const int32_t kGunHurtValue       = 10 * kHpScale;   // 占位: 普通枪械单发伤害 (10 HP)
static const int32_t kFullScreenBombHurt = 30 * kHpScale;   // 占位: 全屏炸弹伤害 (30 HP)
static const int32_t kBulletHurt         = 0;               // 占位: 子弹附加伤害 (Skill_Update_BulletHurt)
static const int32_t kCritChance         = 10;              // 暴击率 % (Skill_Update_HeroHurt)
static const int32_t kStopChance         = 10;              // 子弹停顿 % (Skill_Update_BulletStopSp01)
static const int32_t kHeroDodgeChance    = 5;               // 英雄闪避 % (Skill_Update_HeroDodge)
static const int32_t kTeacherTouchHurt   = kHpScale / 2;    // 老师接触伤害 0.5 HP (centi)

// 时间(秒) → 帧 (固定 20Hz): 原 Cocos 用可变 dt, 定点核心固定步长, 一律折算成帧。
static const int32_t kGuardEquipFrames      = 300;  // 守卫变身/装备时长 15s
static const int32_t kTeacherTroopsInterval = 20;   // 老师出怪间隔 1.0s
static const int32_t kSpTeacherEggFrames    = 60;   // 体育老师蛋状态 3s

// 子弹常量 (占位数值, C5 从配置读)
static const int32_t kRpgExplodeRadius    = 150;  // 火箭筒爆炸半径 (px)
static const int32_t kRpgHomingMaxTurnDeg = 11;   // 火箭筒追踪每帧最大转向角 (度)
static const int32_t kRpgTargetRadius     = 600;  // 火箭筒索敌半径 (px)
static const int32_t kRpgReachDist        = 30;   // 到达目标重选距离 (px)
static const int32_t kBulletDieFrames     = 2;    // 子弹命中后残留帧 (0.1s; 占位, C5 从动画抽)

// 英雄子弹/开火常量 (占位数值, C5 从枪械配置 bin 读)
static const int32_t kHeroBulletSpeed       = 20;  // 英雄子弹速度 px/帧 (占位, 400 px/s)
static const int32_t kHeroFireIntervalFrames = 6;  // 开火间隔 0.3s→6 帧 (占位, 各枪真实射速来自配置)

// 道具常量 (占位数值, C5 从配置读; 帧值按 20Hz 折算)
static const int32_t kPropLifeFrames          = 500;   // 金币/阳光/武器 wait 存活帧 (原 m_nLifeLength=500)
static const int32_t kPropFadeFrames          = 200;   // disappear 淡出帧 (10s; 淡出期间仍可拾取)
static const int32_t kDropGoodsLifeFrames     = 12000; // 掉落物品存活帧 (原 m_fLifeLength=600s)
static const int32_t kPropPickupCoinsRange    = 90;    // 金币/阳光吸附拾取距离 (px)
static const int32_t kPropPickupWeaponRange   = 100;   // 武器/枪礼包拾取距离 (px)
static const int32_t kPropPickupDropGoodsRange = 50;   // 掉落物品拾取距离 (px)
static const int32_t kPropCoinsValue          = 5;     // 每枚金币值 (原 alterCoinsNum(5))

// 全屏炸弹技能常量 (原 StageScene::doFullScreenBomb / Sp01Boss::hurtSp01_from_fullScreenBomb)
static const int32_t kFullScreenBombRadius      = 500;  // 全屏炸弹伤害半径 (px; 原硬编码 500)
static const int32_t kFullScreenBombFuseFrames  = 10;   // 引信 0.5s → 10 帧 (固定 20Hz)

// 墓碑复活 (合作玩法): 队友站上墓碑范围倒计时复活, 多人站立加速。
// 范围 = 半角色高 (英雄受击盒高 40px, 见 Sp01TeacherTouchHero 的 b.h); 倒计时 5s × 20Hz。
static const int32_t kRespawnRangePx = 20;    // 墓碑复活范围 (px; 半角色高)
static const int32_t kRespawnFrames   = 100;  // 复活倒计时 (帧; 5s × 20Hz)

// ---------------------------------------------------------------------------
// 实体字段序列化 (单一字段顺序, serialize/deserialize/hash 三处严格对齐)
// ---------------------------------------------------------------------------
namespace {

void WriteHero(BinWriter& w, const HeroState& h) {
    w.u32(h.id); w.u64((uint64_t)h.player_id);
    w.fx(h.x); w.fx(h.y); w.fx(h.facing); w.fx(h.aimRadians);
    w.i32(h.hp); w.i32(h.maxHp);
    w.fx(h.walkSpeed); w.fx(h.speedMul);
    w.u8(h.action); w.u8(h.animDir); w.u8(h.moveDir); w.bl(h.alive);
    w.u8(h.invincible); w.i32(h.hasGetHurtNum);
    w.u8(h.gunType); w.u8(h.preGun); w.u8(h.reportedGun); w.i32(h.curHasShootNum);
    w.i32(h.numBulletShouldShoot); w.i32(h.numBulletHasShoot);
    w.fx(h.timeBulletInterval); w.u32(h.lastShootFrame); w.bl(h.waitingShuttleInterval);
    w.i32(h.bombFuse);
    w.i32(h.killCount);
    w.i32(h.deathCount); w.i32(h.respawnTimer);
}
bool ReadHero(BinReader& r, HeroState& h) {
    uint64_t pid = 0;
    uint32_t u = 0;
    if (!r.u32(h.id) || !r.u64(pid)) return false; h.player_id = (int64_t)pid;
    if (!r.fx(h.x) || !r.fx(h.y) || !r.fx(h.facing) || !r.fx(h.aimRadians)) return false;
    if (!r.i32(h.hp) || !r.i32(h.maxHp)) return false;
    if (!r.fx(h.walkSpeed) || !r.fx(h.speedMul) || !r.u8(u)) return false; h.action = (uint8_t)u;
    if (!r.u8(u)) return false; h.animDir = (uint8_t)u;
    if (!r.u8(u)) return false; h.moveDir = (uint8_t)u;
    if (!r.bl(h.alive) || !r.u8(u)) return false; h.invincible = (uint8_t)u;
    if (!r.i32(h.hasGetHurtNum)) return false;
    if (!r.u8(u)) return false; h.gunType = (uint8_t)u;
    if (!r.u8(u)) return false; h.preGun = (uint8_t)u;
    if (!r.u8(u)) return false; h.reportedGun = (uint8_t)u;
    if (!r.i32(h.curHasShootNum) || !r.i32(h.numBulletShouldShoot) || !r.i32(h.numBulletHasShoot)) return false;
    if (!r.fx(h.timeBulletInterval) || !r.u32(h.lastShootFrame) || !r.bl(h.waitingShuttleInterval)) return false;
    if (!r.i32(h.bombFuse)) return false;
    if (!r.i32(h.killCount)) return false;
    if (!r.i32(h.deathCount) || !r.i32(h.respawnTimer)) return false;
    return true;
}

void WriteSp01(BinWriter& w, const Sp01State& s) {
    w.u32(s.id); w.u8(s.classId); w.i32(s.idInSp01Info);
    w.fx(s.x); w.fx(s.y); w.fx(s.radians);
    w.i32(s.hp); w.i32(s.maxHp); w.i32(s.hpPre);
    w.u8(s.action); w.u8(s.animDir); w.bl(s.remove);
    w.fx(s.walkSpeed); w.fx(s.moveBackSpeed); w.fx(s.moveBackRadians);
    w.i32(s.moveBackAllCount); w.bl(s.moveBackFromGun); w.bl(s.stopMoveTemp);
    w.i32(s.sleepWalkingCount); w.i32(s.variationCount); w.i32(s.residueTimeOrCount);
    w.i32(s.troopsTiming); w.i32(s.actionElapsedFrames); w.i32(s.sp01PauseCount); w.i32(s.stopTimeCount);
    w.bl(s.stopFindHeroUntilGetNextPoint); w.bl(s.isFindHeroInThisFPS); w.bl(s.andRadiansOrMinus);
    w.fx(s.hurtToHero);
    w.i32(s.dropCoinsProbability); w.i32(s.dropCoinsMax); w.i32(s.dropCoinsMin);
    w.i32(s.dropHuoJianTongPro); w.i32(s.dropJiaTeLinPro); w.i32(s.dropSunShinePro);
    w.i32(s.dropSunShineMax); w.i32(s.dropSunShineMin); w.i32(s.andLight);
    w.fx(s.nextX); w.fx(s.nextY); w.bl(s.hasNextPoint);
}
bool ReadSp01(BinReader& r, Sp01State& s) {
    uint32_t u = 0;
    if (!r.u32(s.id) || !r.u8(u)) return false; s.classId = (uint8_t)u;
    if (!r.i32(s.idInSp01Info)) return false;
    if (!r.fx(s.x) || !r.fx(s.y) || !r.fx(s.radians)) return false;
    if (!r.i32(s.hp) || !r.i32(s.maxHp) || !r.i32(s.hpPre)) return false;
    if (!r.u8(u)) return false; s.action = (uint8_t)u;
    if (!r.u8(u)) return false; s.animDir = (uint8_t)u;
    if (!r.bl(s.remove)) return false;
    if (!r.fx(s.walkSpeed) || !r.fx(s.moveBackSpeed) || !r.fx(s.moveBackRadians)) return false;
    if (!r.i32(s.moveBackAllCount) || !r.bl(s.moveBackFromGun) || !r.bl(s.stopMoveTemp)) return false;
    if (!r.i32(s.sleepWalkingCount) || !r.i32(s.variationCount) || !r.i32(s.residueTimeOrCount)) return false;
    if (!r.i32(s.troopsTiming) || !r.i32(s.actionElapsedFrames) || !r.i32(s.sp01PauseCount) || !r.i32(s.stopTimeCount)) return false;
    if (!r.bl(s.stopFindHeroUntilGetNextPoint) || !r.bl(s.isFindHeroInThisFPS) || !r.bl(s.andRadiansOrMinus)) return false;
    if (!r.fx(s.hurtToHero)) return false;
    if (!r.i32(s.dropCoinsProbability) || !r.i32(s.dropCoinsMax) || !r.i32(s.dropCoinsMin)) return false;
    if (!r.i32(s.dropHuoJianTongPro) || !r.i32(s.dropJiaTeLinPro) || !r.i32(s.dropSunShinePro)) return false;
    if (!r.i32(s.dropSunShineMax) || !r.i32(s.dropSunShineMin) || !r.i32(s.andLight)) return false;
    if (!r.fx(s.nextX) || !r.fx(s.nextY) || !r.bl(s.hasNextPoint)) return false;
    return true;
}

void WriteBullet(BinWriter& w, const BulletState& b) {
    w.u32(b.id); w.u8(b.classId); w.u8(b.action);
    w.fx(b.x); w.fx(b.y); w.fx(b.rotate); w.fx(b.speed); w.fx(b.hurt);
    w.i32(b.lifeLength); w.i32(b.sleepTime); w.bl(b.remove);
    w.u32((uint32_t)b.hasHurtIds.size());
    for (uint32_t v : b.hasHurtIds) w.u32(v);
    w.fx(b.targetX); w.fx(b.targetY); w.bl(b.hasTarget);
    w.u64((uint64_t)b.ownerPlayerId);
}
bool ReadBullet(BinReader& r, BulletState& b) {
    uint32_t u = 0, cnt = 0;
    if (!r.u32(b.id) || !r.u8(u)) return false; b.classId = (uint8_t)u;
    if (!r.u8(u)) return false; b.action = (uint8_t)u;
    if (!r.fx(b.x) || !r.fx(b.y) || !r.fx(b.rotate) || !r.fx(b.speed) || !r.fx(b.hurt)) return false;
    if (!r.i32(b.lifeLength) || !r.i32(b.sleepTime) || !r.bl(b.remove)) return false;
    if (!r.u32(cnt)) return false;
    b.hasHurtIds.clear();
    for (uint32_t i = 0; i < cnt; ++i) { uint32_t v; if (!r.u32(v)) return false; b.hasHurtIds.push_back(v); }
    if (!r.fx(b.targetX) || !r.fx(b.targetY) || !r.bl(b.hasTarget)) return false;
    uint64_t owner = 0;
    if (!r.u64(owner)) return false; b.ownerPlayerId = (int64_t)owner;
    return true;
}

void WriteTurret(BinWriter& w, const TurretState& t) {
    w.u32(t.id); w.i32(t.nameIndex); w.fx(t.x); w.fx(t.y); w.fx(t.aim);
    w.i32(t.hp); w.i32(t.maxHp); w.u8(t.action); w.i32(t.hurtHeroDis);
    w.i32(t.delay); w.bl(t.active); w.bl(t.remove);
    w.fx(t.hurtToHero); w.fx(t.bulletSpeed); w.i32(t.actionElapsedFrames);
    w.i32(t.dropCoinsProbability); w.i32(t.dropCoinsMax); w.i32(t.dropCoinsMin);
    w.i32(t.dropSunShinePro); w.i32(t.dropSunShineMax); w.i32(t.dropSunShineMin);
}
bool ReadTurret(BinReader& r, TurretState& t) {
    uint32_t u = 0;
    if (!r.u32(t.id) || !r.i32(t.nameIndex) || !r.fx(t.x) || !r.fx(t.y) || !r.fx(t.aim)) return false;
    if (!r.i32(t.hp) || !r.i32(t.maxHp) || !r.u8(u)) return false; t.action = (uint8_t)u;
    if (!r.i32(t.hurtHeroDis) || !r.i32(t.delay) || !r.bl(t.active) || !r.bl(t.remove)) return false;
    if (!r.fx(t.hurtToHero) || !r.fx(t.bulletSpeed) || !r.i32(t.actionElapsedFrames)) return false;
    if (!r.i32(t.dropCoinsProbability) || !r.i32(t.dropCoinsMax) || !r.i32(t.dropCoinsMin)) return false;
    if (!r.i32(t.dropSunShinePro) || !r.i32(t.dropSunShineMax) || !r.i32(t.dropSunShineMin)) return false;
    return true;
}

void WriteProp(BinWriter& w, const PropState& p) {
    w.u32(p.id); w.u8(p.classId); w.u8(p.action);
    w.fx(p.x); w.fx(p.y); w.fx(p.endX); w.fx(p.endY);
    w.i32(p.value); w.i32(p.velocity); w.i32(p.acceleration); w.i32(p.delay); w.i32(p.life);
    w.i32(p.fade);
    w.bl(p.remove);
}
bool ReadProp(BinReader& r, PropState& p) {
    uint32_t u = 0;
    if (!r.u32(p.id) || !r.u8(u)) return false; p.classId = (uint8_t)u;
    if (!r.u8(u)) return false; p.action = (uint8_t)u;
    if (!r.fx(p.x) || !r.fx(p.y) || !r.fx(p.endX) || !r.fx(p.endY)) return false;
    if (!r.i32(p.value) || !r.i32(p.velocity) || !r.i32(p.acceleration)) return false;
    if (!r.i32(p.delay) || !r.i32(p.life) || !r.i32(p.fade) || !r.bl(p.remove)) return false;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// MoveHero — 英雄移动 + 地图边界 + tile 碰撞 (定点)
//
// 逐语句复刻 Hero::changeHeroPos + Hero::checkCollidesToAddPos (Hero.cpp), 把
// 8 方向单位向量 × 每帧位移 × 移速系数 × dt 与「边界裁剪 + 点撞矩形 + 轴分离」
// 全部定点化。坐标系 Y 向上 (与 DirToUnitVectorFx 一致)。
//
// 注意: 原代码轴分离 fallback 的边界判断写成 tempPos.x + addPos.x (即原坐标 +
// 2*addX 的「双加」), 是原实现的一处既有特性, 此处按原样复刻以保证单机/联网
// 逐帧一致 (R1)。若后续差分测试证明该特性在正常路径不可观测, 再统一修正。
// ---------------------------------------------------------------------------
static void MoveHero(HeroState& h, uint32_t moveDir, const MapCollision& col) {
    if (moveDir == 0) return;

    fx_t ux = 0, uy = 0;
    DirToUnitVectorFx(moveDir, ux, uy);
    const fx_t step = FxMul(h.walkSpeed, h.speedMul);   // 每帧位移量 (px/帧 × 移速系数)
    const fx_t addX = FxMul(ux, step);
    const fx_t addY = FxMul(uy, step);

    const fx_t kMargin = FxFromInt(20);                      // 原代码硬编码 20px 边界余量
    const fx_t maxX    = FxFromInt(col.mapWidth)  - kMargin;
    const fx_t maxY    = FxFromInt(col.mapHeight) - kMargin;

    // 第 1 步: 边界裁剪 → tempPos (原 checkCollidesToAddPos 前半段)
    fx_t tx = h.x, ty = h.y;
    const bool xIn = (h.x + addX) <= maxX && (h.x + addX) > kMargin;
    const bool yIn = (h.y + addY) <= maxY && (h.y + addY) > kMargin;
    if (xIn) tx = h.x + addX;
    if (yIn) ty = h.y + addY;

    if (!col.ContainsPeople(tx, ty)) {
        h.x = tx;
        h.y = ty;
    } else {
        // 第 2 步: 轴分离 (x 单独 / y 单独), 复刻原代码含「双加」的边界判断
        if (!col.ContainsPeople(h.x + addX, h.y) &&
            (tx + addX) <= maxX && (tx + addX) > kMargin) {
            h.x = h.x + addX;
        } else if (!col.ContainsPeople(h.x, h.y + addY) &&
                   (ty + addY) <= maxY && (ty + addY) > kMargin) {
            h.y = h.y + addY;
        }
    }
}

// ---------------------------------------------------------------------------
// 小怪 (Sp01) AI / 移动 — 定点复刻 Sp01.cpp 的 update/do_walk/moveToNext/findHero
// 等 (normal 小怪路径; boss 大怪状态机后续移植)。
//
// 坐标系 Y 向上; 角度 fx_t 弧度; 速度 px/frame (固定 20Hz 已折算)。
// RNG 只走传入的 CBattleRng (单一流), 调用顺序与原文一致, 保证两端逐位确定。
// ---------------------------------------------------------------------------

// 最近存活英雄 (按 heroes 数组顺序遍历, 距离相等取先遇到者, 确定性)
static const HeroState* NearestHeroTo(const std::vector<HeroState>& heroes, fx_t x, fx_t y) {
    const HeroState* best = nullptr;
    int64_t bestDist2 = 0;
    for (const auto& h : heroes) {
        if (!h.alive) continue;
        int64_t dx = (int64_t)h.x - x;
        int64_t dy = (int64_t)h.y - y;
        int64_t d2 = dx * dx + dy * dy;
        if (best == nullptr || d2 < bestDist2) { best = &h; bestDist2 = d2; }
    }
    return best;
}

// 两点距离 (像素, 向下取整; 对齐 (int)ccpDistance 截断语义)
static int32_t FxDistPx(fx_t ax, fx_t ay, fx_t bx, fx_t by) {
    int64_t dx = (int64_t)bx - ax;
    int64_t dy = (int64_t)by - ay;
    int64_t d2 = dx * dx + dy * dy;
    return (int32_t)(FxU64SqrtFloor((uint64_t)d2) >> 16);
}

// 小怪撞墙 (normal: 点撞 collide_people; teacher/guard boss 用 32x32 矩形相交)
static bool Sp01CollideWall(fx_t x, fx_t y, uint8_t classId, const MapCollision& col) {
    if (classId == kClassSp01BigTeacher || classId == kClassSp01BigGuard) {
        FxRect r;
        r.x = x - FxFromInt(16); r.y = y - FxFromInt(16);
        r.w = FxFromInt(32);     r.h = FxFromInt(32);
        return col.IntersectsPeople(r);
    }
    return col.ContainsPeople(x, y);
}

// 是否出地图 (对齐 Sp01::isGoOut: x<0 || x>mapW || y<0 || y>mapH, 闭区间)
static bool Sp01GoOut(fx_t x, fx_t y, const MapCollision& col) {
    return x < 0 || x > FxFromInt(col.mapWidth) || y < 0 || y > FxFromInt(col.mapHeight);
}

// 与最近英雄是否进入攻击判定 (bHurtHero: 距离 < 45 / 81 / 85)
// 比较在 fx 域做: (dx²+dy²) < (dis·2¹⁶)², 与浮点 sqrt(d²)<dis 等价, 无截断。
static bool Sp01HurtHero(const Sp01State& s, const HeroState& hero) {
    int32_t dis = 45;
    if (s.classId == kClassSp01BigGuard || s.classId == kClassSp01BigSPTeacher) dis = 66 + 15;
    else if (s.classId == kClassSp01BigTeacher) dis = 70 + 15;
    int64_t dx = (int64_t)hero.x - s.x;
    int64_t dy = (int64_t)hero.y - s.y;
    int64_t disFx = (int64_t)dis << 16;
    return dx * dx + dy * dy < disFx * disFx;
}

// 弧度归一到 [-π, π] (对齐 Sp01::checkRadians)
static void Sp01CheckRadians(Sp01State& s) {
    if (s.radians > kFxPi)             s.radians -= kFxTwoPi;
    else if (s.radians < FxNeg(kFxPi)) s.radians += kFxTwoPi;
}

// 朝向 → 8 方向 (渲染用; 复刻 changeDir 的 4 象限映射, 0..3)
static void Sp01UpdateAnimDir(Sp01State& s) {
    fx_t r = s.radians;
    if (r >= 0 && r <= kFxPiOver2)                     s.animDir = 0;   // RightUp
    else if (r < 0 && r >= FxNeg(kFxPiOver2))          s.animDir = 1;   // RightDown
    else if (r < FxNeg(kFxPiOver2) && r >= FxNeg(kFxPi)) s.animDir = 2; // LeftDown
    else                                               s.animDir = 3;   // LeftUp
}

// initSleepWalking: 游走计时 + 随机偏转角 (CCRANDOM_0_1 → NextFloat; 即使 time<=0 也消耗 1 次 RNG)
static void Sp01InitSleepWalking(Sp01State& s, CBattleRng& rng, int32_t time, fx_t deltaRadians) {
    if (time > 0) s.sleepWalkingCount = time;
    float u = rng.NextFloat();   // [0,1) 单精度, 两端一致
    fx_t du = FxMul(FxFromFloat(u), deltaRadians);
    s.radians = s.andRadiansOrMinus ? s.radians + du : s.radians - du;
    Sp01CheckRadians(s);
    Sp01UpdateAnimDir(s);
}

// findHero: 以最近英雄为目标点并转向 (changeActionToNext: atan2)
static void Sp01FindHero(Sp01State& s, const HeroState& hero) {
    if (s.isFindHeroInThisFPS || s.stopFindHeroUntilGetNextPoint) return;
    s.isFindHeroInThisFPS = true;
    s.nextX = hero.x;
    s.nextY = hero.y;
    s.hasNextPoint = true;
    s.radians = FxAtan2(hero.y - s.y, hero.x - s.x);
    Sp01UpdateAnimDir(s);
}

// moveToNext: 沿当前朝向移动 + 撞墙/出图/到达处理 + (boss/距离分档) 随机重寻路
static void Sp01MoveToNext(Sp01State& s, const HeroState& hero, const MapCollision& col, CBattleRng& rng) {
    if (s.stopMoveTemp) { s.stopMoveTemp = false; return; }

    const int32_t disWithHero = FxDistPx(s.x, s.y, hero.x, hero.y);

    // 守卫「装备」状态下移速加成 (原: m_fWalkSpeedNow += 0.3f*60*dt → +18 px/s ÷ 20Hz = +0.9 px/帧)
    fx_t walkSpeed = s.walkSpeed;
    if (s.classId == kClassSp01BigGuard &&
        (s.action == kSp01BGuardEquip || s.action == kSp01BGuardEquipWalk ||
         s.action == kSp01BGuardEquipAttack || s.action == kSp01BGuardUnload)) {
        walkSpeed = FxAdd(walkSpeed, FxFromFloat(0.9f));
    }

    // 游走/睡眠分支
    if (s.sleepWalkingCount > 0) {
        --s.sleepWalkingCount;
        if (disWithHero < 50) s.sleepWalkingCount = -1;
        fx_t cx = FxCos(s.radians), sy = FxSin(s.radians);
        fx_t nx = s.x + FxMul(cx, walkSpeed);
        fx_t ny = s.y + FxMul(sy, walkSpeed);
        if (!Sp01CollideWall(nx, ny, s.classId, col) && !Sp01GoOut(nx, ny, col)) {
            s.x = nx; s.y = ny;
        } else {
            Sp01InitSleepWalking(s, rng, -1, kFxTwoPi);   // DEGREEN360_TO_RADIANS
        }
        if (s.sleepWalkingCount <= 0) {
            s.stopFindHeroUntilGetNextPoint = false;
            Sp01FindHero(s, hero);
        }
        return;
    }

    // 正常移动
    fx_t cx = FxCos(s.radians), sy = FxSin(s.radians);
    fx_t nx = s.x + FxMul(cx, walkSpeed);
    fx_t ny = s.y + FxMul(sy, walkSpeed);
    if (Sp01CollideWall(nx, ny, s.classId, col) || Sp01GoOut(nx, ny, col)) {
        Sp01FindHero(s, hero);   // realy=true
        s.stopFindHeroUntilGetNextPoint = true;
        ++s.stopTimeCount;
        if (s.stopTimeCount >= 2) Sp01InitSleepWalking(s, rng, 130, kFxTwoPi);
        return;
    } else {
        s.x = nx; s.y = ny;
        s.stopTimeCount = 0;
    }

    // 到达 nextPoint (|dx|<=2 && |dy|<=2)
    if (FxAbs(s.nextX - s.x) <= FxFromInt(2) && FxAbs(s.nextY - s.y) <= FxFromInt(2)) {
        s.stopFindHeroUntilGetNextPoint = false;
        Sp01FindHero(s, hero);
        return;
    }

    // boss (守卫/体育老师/老师): 每帧 1/5 概率重寻路, 无距离分档 (原 switch 提前 return)
    if (s.classId == kClassSp01BigGuard || s.classId == kClassSp01BigSPTeacher ||
        s.classId == kClassSp01BigTeacher) {
        if (rng.NextU32() % 5 == 1) Sp01FindHero(s, hero);
        return;
    }

    // 距离分档随机重寻路 (RNG 调用顺序与原文一致: random(n)==1)
    if (disWithHero < 200) {
        if (rng.NextU32() % 10 == 1) Sp01FindHero(s, hero);
        return;
    } else if (disWithHero < 300) {
        if (rng.NextU32() % 30 == 1) Sp01FindHero(s, hero);
        if (rng.NextU32() % 400 == 1) Sp01InitSleepWalking(s, rng, 20, kFxTwoPi);
        return;
    } else if (disWithHero < 400) {
        if (rng.NextU32() % 40 == 1) Sp01FindHero(s, hero);
        if (rng.NextU32() % 300 == 1) Sp01InitSleepWalking(s, rng, 20, kFxTwoPi);
        return;
    } else if (disWithHero < 500) {
        if (rng.NextU32() % 100 == 1) Sp01FindHero(s, hero);
        if (rng.NextU32() % 400 == 1) Sp01InitSleepWalking(s, rng, 30, kFxTwoPi);
        return;
    } else {
        if (rng.NextU32() % 100 == 1) Sp01FindHero(s, hero);
        if (rng.NextU32() % 20 == 1) Sp01InitSleepWalking(s, rng, 50, kFxTwoPi);
        return;
    }
}

// checkMoveBack: 被散弹击退 (moveBackFromGun)
static void Sp01CheckMoveBack(Sp01State& s, const MapCollision& col) {
    if (!s.moveBackFromGun) return;
    --s.moveBackAllCount;
    if (s.moveBackAllCount <= 0) s.moveBackFromGun = false;
    fx_t cx = FxCos(s.moveBackRadians), sy = FxSin(s.moveBackRadians);
    fx_t nx = s.x + FxMul(cx, s.moveBackSpeed);
    fx_t ny = s.y + FxMul(sy, s.moveBackSpeed);
    if (!Sp01CollideWall(nx, ny, s.classId, col) && !Sp01GoOut(nx, ny, col)) {
        s.x = nx; s.y = ny;
    }
}

// ---------------------------------------------------------------------------
// 大怪 (boss) 状态机 + 伤害/HP 结算 — 定点复刻 Sp01.cpp 的 animationDone/changeAction/
// changeToHurt/ai_big_guard/ai_big_teacher/ai_big_spTeacher 与 Sp01Boss.cpp 的掉落/分裂。
//
// 关键约定:
//   - 动作时长: 原动画 clip 播完触发 animationDone, 定点核心用「动作时长表」
//     Sp01ActionFrames (占位值, C5 从动画数据抽取后替换) + actionElapsedFrames 计时。
//   - 时序: 每帧先跑 per-frame AI (原 update), 再检查动作计时触发 animationDone
//     (原动画系统在 update 之后回调), 与原文一致。
//   - changeAction(!force) 的「不可打断」列表 (die/attack/attack1/attack2/EQUIPATTACK)
//     在 Sp01SetAction 里复刻; 方向变化重播当前动作 (重置计时) 在 Sp01TurnToPoint。
// ---------------------------------------------------------------------------

// 动作时长表 (帧; 0 = 循环动作, 无 animationDone)
static int32_t Sp01ActionFrames(uint8_t action) {
    switch (action) {
    case kSp01Attack:              return 20;   // 攻击
    case kSp01Hurt:                return 10;   // 受击
    case kSp01Die:                 return 30;   // 死亡
    case kSp01Disappear:           return 10;   // 消失
    case kSp01BGuardAttack1:       return 20;
    case kSp01BGuardAttack2:       return 20;
    case kSp01BGuardEquip:         return 30;   // 装备(变身)
    case kSp01BGuardEquipAttack:   return 20;
    case kSp01BGuardUnload:        return 20;   // 卸装
    case kSp01BTeacherChange:      return 30;   // 变身
    case kSp01BTeacherResume:      return 20;   // 恢复
    case kSp01BSpTeacherToEgg:     return 20;   // 结蛋
    case kSp01BSpTeacherExitEgg:   return 20;   // 破蛋
    case kSp01BSpTeacherFenLie:    return 20;   // 分裂
    default:                       return 0;    // walk/appear/EQUIPWALK/Troops/WaitResume/Egg 循环
    }
}

// changeAction: 设置动作 + 重置动作计时 (force=false 时受「不可打断」列表保护)
static void Sp01SetAction(Sp01State& s, uint8_t action, bool force) {
    if (!force) {
        if (s.action == kSp01Die || s.action == kSp01Attack ||
            s.action == kSp01BGuardAttack1 || s.action == kSp01BGuardAttack2 ||
            s.action == kSp01BGuardEquipAttack) return;
    }
    s.action = action;
    s.actionElapsedFrames = 0;
}

// changeActionToNext: 面向目标点 (atan2); 4 象限方向变化 → 重播当前动作 (重置计时)
static void Sp01TurnToPoint(Sp01State& s, fx_t px, fx_t py) {
    s.radians = FxAtan2(py - s.y, px - s.x);
    uint8_t oldDir = s.animDir;
    Sp01UpdateAnimDir(s);
    if (s.animDir != oldDir) s.actionElapsedFrames = 0;
}

// do_attack: 面向 nextPoint (原 changeActionToNext(m_pNextPoint, ...), 用上次 findHero 的落点)
static void Sp01FaceNextPoint(Sp01State& s) {
    Sp01TurnToPoint(s, s.nextX, s.nextY);
}

// bCanNotHurt: appear/die/remove 或出地图边界 (5px 余量) 时不受伤
static bool Sp01CanNotHurt(const Sp01State& s, const MapCollision& col) {
    if (s.action == kSp01Appear || s.action == kSp01Die || s.remove) return true;
    const fx_t m = FxFromInt(5);
    return s.x < m || s.x > FxFromInt(col.mapWidth) - m ||
           s.y < m || s.y > FxFromInt(col.mapHeight) - m;
}

// isHpFromPass70/40/10: hpPre > maxHp*threshold && hp <= maxHp*threshold (整数缩放 num/den)
static bool Sp01HpPassed(int32_t hpPre, int32_t hp, int32_t maxHp, int32_t num, int32_t den) {
    int64_t th = (int64_t)maxHp * num / den;
    return (int64_t)hpPre > th && (int64_t)hp <= th;
}

// createARandomPointInCircle: 圆内随机点 (2 次 RNG: random(radius) + random(360))
static void CreateRandomPointInCircle(CBattleRng& rng, fx_t cx, fx_t cy, fx_t radius,
                                      int32_t disCenter, fx_t& ox, fx_t& oy) {
    int32_t randomRadius = (int32_t)(rng.NextU32() % (uint32_t)FxToIntFloor(radius)) + disCenter;
    int32_t deg = (int32_t)(rng.NextU32() % 360);
    fx_t rad = FxDiv(FxMul(FxFromInt(deg), kFxPi), FxFromInt(180));   // deg*π/180
    ox = cx + FxMul(FxCos(rad), FxFromInt(randomRadius));
    oy = cy + FxMul(FxSin(rad), FxFromInt(randomRadius));
}

// getARandomPointArroundSp01NotInCollides: 最多 500 次找一个不在碰撞内的圆内点
static bool GetARandomPointArroundNotInCollides(fx_t cx, fx_t cy, fx_t radius,
                                                fx_t& ox, fx_t& oy,
                                                const MapCollision& col, CBattleRng& rng) {
    for (int i = 0; i < 500; ++i) {
        CreateRandomPointInCircle(rng, cx, cy, radius, 10, ox, oy);
        if (!col.ContainsPeople(ox, oy)) return true;
    }
    return false;
}

// isSp01StillInStage: 场上是否仍有指定 classId 的存活实体
static bool AnySp01OfClass(const std::vector<Sp01State>& all, uint8_t classId) {
    for (const auto& s : all) if (s.classId == classId && !s.remove) return true;
    return false;
}

// 最近存活英雄 (可变引用版, 供老师接触伤害等直接改英雄状态)
static HeroState* NearestHeroToMutable(std::vector<HeroState>& heroes, fx_t x, fx_t y) {
    HeroState* best = nullptr;
    int64_t bestDist2 = 0;
    for (auto& h : heroes) {
        if (!h.alive) continue;
        int64_t dx = (int64_t)h.x - x;
        int64_t dy = (int64_t)h.y - y;
        int64_t d2 = dx * dx + dy * dy;
        if (best == nullptr || d2 < bestDist2) { best = &h; bestDist2 = d2; }
    }
    return best;
}

// 生成一个 Sp01 (子实体; 属性按 Sp01ConfigKey(classId, idInSp01Info) 查 Sp01Config, 无配置行回退默认)
static void SpawnSp01(std::vector<Sp01State>& out, uint32_t& nextId, uint8_t classId,
                      int32_t idInSp01Info, fx_t x, fx_t y, int32_t variationCount,
                      uint8_t action, CBattleRng& rng,
                      const std::map<uint32_t, Sp01Config>* cfg = nullptr) {
    Sp01State s;
    s.id = nextId++;
    s.classId = classId;
    s.idInSp01Info = idInSp01Info;
    s.x = x; s.y = y;

    // C5: 配置属性 (hp/walkSpeed/hurtToHero); 无配置行回退默认 (保持旧占位, 确定性)
    int32_t hp = 100 * kHpScale;
    fx_t walk = FxFromInt(5);
    fx_t hurt = FxFromInt(kHpScale);
    if (cfg) {
        std::map<uint32_t, Sp01Config>::const_iterator it = cfg->find(Sp01ConfigKey(classId, idInSp01Info));
        if (it != cfg->end()) {
            hp = it->second.maxHp * kHpScale;
            walk = it->second.walkSpeed;
            hurt = it->second.hurtToHero;
        }
    }
    s.hp = hp; s.maxHp = hp; s.hpPre = hp;
    s.action = action;
    s.variationCount = variationCount;
    s.walkSpeed = walk;
    s.hurtToHero = hurt;
    s.andRadiansOrMinus = (rng.NextU32() % 2 == 0);   // init random(2)
    out.push_back(s);
}

// 出生组在 waveTimer=T 时累计「应已出生」的数量 (纯函数; 刷怪进度由 waveTimer 唯一推导)
static int32_t WaveGroupSpawnedAt(const WaveSpawnGroup& g, int32_t T) {
    if (T < g.startTime) return 0;
    int32_t elapsed = T - g.startTime;
    if (g.interval <= 0) return g.count;   // 一次性全刷
    int32_t n = elapsed / g.interval + 1;  // startTime 刷第 1 个, 之后每 interval 一个
    return n < g.count ? n : g.count;
}

// 出生点坐标: entries[entryPoint-1]; 无 entries 时顶部横向兜底 (确定性)
static void WaveSpawnPos(const MapCollision& col, int32_t entryPoint, fx_t& x, fx_t& y) {
    if (entryPoint >= 1 && entryPoint <= (int32_t)col.entries.size()) {
        x = col.entries[(size_t)entryPoint - 1].x;
        y = col.entries[(size_t)entryPoint - 1].y;
    } else {
        int32_t c = (entryPoint <= 0) ? 0 : (entryPoint - 1);
        x = FxFromInt(80 + (c % 8) * 120);
        y = FxFromInt(80);
    }
}

// 生成一个道具 (金币/阳光; 更新逻辑见 prop 阶段)
static void SpawnProp(std::vector<PropState>& out, uint32_t& nextId, uint8_t classId,
                      fx_t x, fx_t y, int32_t value) {
    PropState p;
    p.id = nextId++;
    p.classId = classId;
    p.action = kPropInit;
    p.x = x; p.y = y; p.endX = x; p.endY = y;
    p.value = value;
    p.life = kPropLifeFrames;   // wait 存活帧数 (原 m_nLifeLength=500)
    p.fade = 0;
    p.remove = false;
    out.push_back(p);
}

// randomCreateProp: 掉落判定 (RNG 顺序与 createCoins/createSunshine 一致)
static void Sp01RandomCreateProp(const Sp01State& s, CBattleRng& rng,
                                 std::vector<PropState>& out, uint32_t& nextId) {
    if ((int32_t)(rng.NextU32() % 100) < s.dropCoinsProbability) {
        int32_t ch = s.dropCoinsMax - s.dropCoinsMin + 1;
        if (ch <= 0) ch = 1;
        int32_t num = s.dropCoinsMin + (int32_t)(rng.NextU32() % (uint32_t)ch);
        for (int32_t i = 0; i < num; ++i) {
            fx_t px, py;
            CreateRandomPointInCircle(rng, s.x, s.y, FxFromInt(60), 15, px, py);
            SpawnProp(out, nextId, kClassPropCoins, px, py, 5);
        }
    }
    if ((int32_t)(rng.NextU32() % 100) < s.dropSunShinePro) {
        int32_t ch = s.dropSunShineMax - s.dropSunShineMin + 1;
        if (ch <= 0) ch = 1;
        int32_t num = s.dropSunShineMin + (int32_t)(rng.NextU32() % (uint32_t)ch);
        for (int32_t i = 0; i < num; ++i) {
            fx_t px, py;
            CreateRandomPointInCircle(rng, s.x, s.y, FxFromInt(60), 15, px, py);
            SpawnProp(out, nextId, kClassPropSunShine, px, py, 1);
        }
    }
}

// ---------------------------------------------------------------------------
// 道具拾取效果 (Hero::hitProps, 定点): 金币/钻石/回血/切枪/移速 buff 等确定性效果
// ---------------------------------------------------------------------------
static void HeroHitProps(HeroState& h, uint8_t classId, int32_t value, GameState& game) {
    switch (classId) {
        case kClassPropCoins:       game.coins += kPropCoinsValue; break;
        case kClassPropSunShine:    game.diamond += 1; break;
        case kClassPropHuoJianTong:
        case kDropGoodsHuoJianTong: h.gunType = kGunHuoJianTong; break;
        case kClassPropJiaTeLin:
        case kDropGoodsJiaTeLin:    h.gunType = kGunJiaTeLin; break;
        case kDropGoodsHp: {
            // alter_m_nHP(m_fMaxHP*0.3f) → 回复 30% 最大生命 (centi)
            int64_t nhp = (int64_t)h.hp + (int64_t)h.maxHp * 30 / 100;
            h.hp = (nhp > h.maxHp) ? h.maxHp : (int32_t)nhp;
            break;
        }
        case kDropGoodsCoins:       game.coins += value; break;
        case kDropGoodsMoveSpeed:   h.speedMul = FxFromFloat(1.3f); break; // 移速 buff (1.3×)
        case kDropGoodsDamage:
        case kDropGoodsDefense:     break; // 攻击/防御 buff (autoSkill, 见波次/Buff 阶段)
        default:                    break; // gungift 等纯 UI, 无核心效果
    }
}

// ---------------------------------------------------------------------------
// 更新单个道具 (Prop::update/aiProp_*, 定点):
//   金币/阳光/武器/枪礼包 → wait 内英雄进范围即拾取, 超时进 disappear 淡出仍可拾取;
//   掉落物品 (DROPGOODSTYPE_*) → delay 表演后 wait 拾取;
//   炸弹 → 下落触底爆炸 (视觉, 伤害走全屏炸弹技能入口, 见 HurtSp01FullScreenBomb)。
// ---------------------------------------------------------------------------
static void UpdateProp(PropState& p, std::vector<HeroState>& heroes, GameState& game) {
    if (p.remove) return;

    // 炸弹: 下落 (velocity += acceleration, y -= velocity), 触底爆炸
    if (p.classId == kClassPropBomb || p.classId == kClassPropBomb2) {
        p.velocity += p.acceleration;
        p.y -= FxFromInt(p.velocity);
        if (p.y < p.endY) p.remove = true;   // 触底 → 爆炸 (原 moveOverToBombCallBack → baozha)
        return;
    }

    // 掉落物品: 先走 delay 表演, 再 wait 拾取
    if (p.classId <= kDropGoodsCoins) {
        if (p.delay > 0) { --p.delay; return; }
        if (p.action == kPropInit) { p.action = kPropWait; p.life = kDropGoodsLifeFrames; }
        HeroState* hero = NearestHeroToMutable(heroes, p.x, p.y);
        if (hero && hero->alive && FxDistPx(p.x, p.y, hero->x, hero->y) < kPropPickupDropGoodsRange) {
            HeroHitProps(*hero, p.classId, p.value, game);
            p.remove = true;
        } else if (p.life > 0 && --p.life <= 0) {
            p.remove = true;
        }
        return;
    }

    // 金币/阳光/武器/枪礼包: init → wait → (拾取 | 超时 disappear) → (拾取 | 淡出移除)
    if (p.action == kPropInit) p.action = kPropWait;
    int32_t range = (p.classId == kClassPropCoins || p.classId == kClassPropSunShine)
                    ? kPropPickupCoinsRange : kPropPickupWeaponRange;
    HeroState* hero = NearestHeroToMutable(heroes, p.x, p.y);
    if (p.action == kPropWait) {
        if (hero && hero->alive && FxDistPx(p.x, p.y, hero->x, hero->y) < range) {
            HeroHitProps(*hero, p.classId, p.value, game);
            p.remove = true;
        } else if (p.life > 0 && --p.life <= 0) {
            p.action = kPropDisappear;
            p.fade = kPropFadeFrames;
        }
    } else if (p.action == kPropDisappear) {
        if (hero && hero->alive && FxDistPx(p.x, p.y, hero->x, hero->y) < range) {
            HeroHitProps(*hero, p.classId, p.value, game);
            p.remove = true;
        } else if (p.fade > 0 && --p.fade <= 0) {
            p.remove = true;
        }
    }
}

// 进入死亡 (die 前掉落一次, 原 if(action != die) randomCreateProp)
static void Sp01OnDie(Sp01State& s, CBattleRng& rng,
                      std::vector<PropState>& out, uint32_t& nextId,
                      std::vector<HeroState>& heroes, int64_t ownerPlayerId) {
    if (s.action != kSp01Die) Sp01RandomCreateProp(s, rng, out, nextId);
    Sp01SetAction(s, kSp01Die, true);
    // 击杀归属 (确定性, 无 RNG): 记到开火英雄头上, 供战斗结算按击杀排名
    if (ownerPlayerId != 0) {
        for (auto& h : heroes) {
            if (h.player_id == ownerPlayerId) { ++h.killCount; break; }
        }
    }
}

// 英雄受击 (Hero::do_Hurt): 无敌/闪避/防御(占位关闭)/扣血/死亡
static void HeroDoHurt(HeroState& h, int32_t hurtCenti, CBattleRng& rng) {
    if (h.invincible > 0) return;                                   // 无敌
    if (rng.NextFloat() * 100.0f < (float)kHeroDodgeChance) return; // 闪避 (消耗 RNG)
    int64_t nhp = (int64_t)h.hp - hurtCenti;
    if (nhp < 0) nhp = 0;
    h.hp = (int32_t)nhp;
    ++h.hasGetHurtNum;
    if (h.hp <= 0) { h.hp = 0; h.alive = false; h.action = kHeroDie; ++h.deathCount; h.respawnTimer = 0; }
    else if (h.hp > 0) h.action = kHeroHurt;
}

// 老师接触伤害: 攻击盒 (占位矩形) 与英雄受击盒 (占位矩形) 相交
static bool Sp01TeacherTouchHero(const Sp01State& s, const HeroState& hero) {
    FxRect a;   // 老师攻击盒 (占位)
    a.x = s.x - FxFromInt(24); a.y = s.y - FxFromInt(24);
    a.w = FxFromInt(48);       a.h = FxFromInt(48);
    FxRect b;   // 英雄受击盒 (占位)
    b.x = hero.x - FxFromInt(12); b.y = hero.y - FxFromInt(20);
    b.w = FxFromInt(24);         b.h = FxFromInt(40);
    return a.Intersects(b);
}

// changeToHurt: 一次枪械/炸弹伤害结算 (暴击/停顿/减伤/HP 阈值/死亡)
static void Sp01ChangeToHurt(Sp01State& s, const MapCollision& col, CBattleRng& rng,
                             uint8_t bulletClassId, int32_t gunHurtValue,
                             std::vector<Sp01State>& outSpawns, uint32_t& nextId,
                             std::vector<PropState>& outProps,
                             std::vector<HeroState>& heroes, int64_t ownerPlayerId) {
    if (Sp01CanNotHurt(s, col)) return;

    s.hpPre = s.hp;

    bool crit = (rng.NextFloat() * 100.0f < (float)kCritChance);   // 暴击
    if (rng.NextFloat() * 100.0f < (float)kStopChance) {           // 子弹停顿 (仅非 boss)
        bool isBoss = (s.classId == kClassSp01BigGuard || s.classId == kClassSp01BigTeacher ||
                       s.classId == kClassSp01BigSPTeacher);
        if (!isBoss) s.sp01PauseCount = 100;
    }

    int32_t curHurt = (bulletClassId == kClassBulletFullScreenBomb) ? kFullScreenBombHurt : gunHurtValue;
    // 自动伤害加成 (AND_DAMAGE) 占位关闭: curHurt += curHurt/2
    int32_t hurtHP = crit ? (curHurt * 2 + kBulletHurt) : (curHurt + kBulletHurt);

    // boss 减伤 / spTeacher 蛋状态免疫 (原 *0.5 / return)
    if (s.classId == kClassSp01BigGuard) {
        if (s.action == kSp01BGuardEquip || s.action == kSp01BGuardEquipWalk ||
            s.action == kSp01BGuardEquipAttack || s.action == kSp01BGuardUnload)
            hurtHP = hurtHP / 2;
    } else if (s.classId == kClassSp01BigTeacher) {
        if (s.action == kSp01BTeacherChange || s.action == kSp01BTeacherTroops ||
            s.action == kSp01BTeacherWaitResume || s.action == kSp01BTeacherResume)
            hurtHP = hurtHP / 2;
    } else if (s.classId == kClassSp01BigSPTeacher) {
        if (s.action == kSp01BSpTeacherToEgg || s.action == kSp01BSpTeacherEgg ||
            s.action == kSp01BSpTeacherExitEgg)
            return;
    }
    // playInfo[1]==2 加成占位关闭: hurtHP += hurtHP*15/100

    int64_t nhp = (int64_t)s.hp - hurtHP;
    if (nhp < 0) nhp = 0;
    if (nhp > s.maxHp) nhp = s.maxHp;
    s.hp = (int32_t)nhp;

    if (s.classId == kClassSp01BigGuard) {
        if (Sp01HpPassed(s.hpPre, s.hp, s.maxHp, 7, 10) ||
            Sp01HpPassed(s.hpPre, s.hp, s.maxHp, 4, 10) ||
            Sp01HpPassed(s.hpPre, s.hp, s.maxHp, 1, 10)) {
            s.residueTimeOrCount = kGuardEquipFrames;
            Sp01SetAction(s, kSp01BGuardEquip, true);
        } else if (s.hp <= 0) {
            Sp01OnDie(s, rng, outProps, nextId, heroes, ownerPlayerId);
        }
    } else if (s.classId == kClassSp01BigTeacher) {
        if (Sp01HpPassed(s.hpPre, s.hp, s.maxHp, 7, 10) ||
            Sp01HpPassed(s.hpPre, s.hp, s.maxHp, 4, 10) ||
            Sp01HpPassed(s.hpPre, s.hp, s.maxHp, 1, 10)) {
            Sp01SetAction(s, kSp01BTeacherChange, true);
        } else if (s.hp <= 0) {
            Sp01OnDie(s, rng, outProps, nextId, heroes, ownerPlayerId);
        }
    } else if (s.classId == kClassSp01BigSPTeacher) {
        if (s.hp <= 0) {
            if (s.variationCount < 3) {
                ++s.variationCount;
                Sp01SetAction(s, kSp01BSpTeacherToEgg, true);
            } else {
                Sp01OnDie(s, rng, outProps, nextId, heroes, ownerPlayerId);
            }
        }
    } else {
        if (s.hp <= 0) Sp01OnDie(s, rng, outProps, nextId, heroes, ownerPlayerId);
        else Sp01SetAction(s, kSp01Hurt, false);   // changeAction(hurt, false), 受不可打断保护
    }
}

// ai_normal: walk→do_walk, attack→do_attack (normal / teacher_child / teacher / spTeacher 共用)
static void Sp01AiNormal(Sp01State& s, const HeroState* hero, const MapCollision& col, CBattleRng& rng) {
    if (!hero) return;
    switch (s.action) {
    case kSp01Walk:
        if (!Sp01HurtHero(s, *hero)) Sp01MoveToNext(s, *hero, col, rng);
        else Sp01SetAction(s, kSp01Attack, true);
        break;
    case kSp01Attack:
        if (!Sp01HurtHero(s, *hero)) Sp01SetAction(s, kSp01Walk, false);
        else Sp01FaceNextPoint(s);
        break;
    }
}

// ai_big_guard: 守卫 (walk/attack1/attack2/EQUIPWALK/EQUIPATTACK, 无 ai_normal/无击退)
static void Sp01AiBigGuard(Sp01State& s, const HeroState* hero, const MapCollision& col, CBattleRng& rng) {
    if (!hero) return;
    switch (s.action) {
    case kSp01Walk:
        if (!Sp01HurtHero(s, *hero)) Sp01MoveToNext(s, *hero, col, rng);
        else if (rng.NextU32() % 2 == 0) Sp01SetAction(s, kSp01BGuardAttack1, true);
        else Sp01SetAction(s, kSp01BGuardAttack2, true);
        break;
    case kSp01BGuardAttack1:
    case kSp01BGuardAttack2:
        if (!Sp01HurtHero(s, *hero)) Sp01SetAction(s, kSp01Walk, false);
        else Sp01FaceNextPoint(s);
        break;
    case kSp01BGuardEquipWalk:
        --s.residueTimeOrCount;
        if (s.residueTimeOrCount <= 0) { Sp01SetAction(s, kSp01BGuardUnload, false); return; }
        if (!Sp01HurtHero(s, *hero)) Sp01MoveToNext(s, *hero, col, rng);
        else Sp01SetAction(s, kSp01BGuardEquipAttack, false);
        break;
    case kSp01BGuardEquipAttack:
        --s.residueTimeOrCount;
        if (s.residueTimeOrCount <= 0) { Sp01SetAction(s, kSp01BGuardUnload, false); return; }
        if (!Sp01HurtHero(s, *hero)) Sp01SetAction(s, kSp01BGuardEquipWalk, false);
        else Sp01FaceNextPoint(s);
        break;
    }
}

// ai_big_teacher: 老师 (ai_normal + Troops 出怪 / WaitResume)
static void Sp01AiBigTeacher(Sp01State& s, std::vector<HeroState>& heroes,
                             const std::vector<Sp01State>& allSp01s, const MapCollision& col,
                             CBattleRng& rng, std::vector<Sp01State>& outSpawns, uint32_t& nextId,
                             const std::map<uint32_t, Sp01Config>* cfg) {
    const HeroState* hero = NearestHeroTo(heroes, s.x, s.y);
    Sp01AiNormal(s, hero, col, rng);

    switch (s.action) {
    case kSp01BTeacherTroops: {
        HeroState* nearest = NearestHeroToMutable(heroes, s.x, s.y);
        if (nearest && Sp01TeacherTouchHero(s, *nearest)) HeroDoHurt(*nearest, kTeacherTouchHurt, rng);
        ++s.troopsTiming;
        if (s.troopsTiming >= kTeacherTroopsInterval) {
            s.troopsTiming = 0;
            --s.residueTimeOrCount;
            if (s.residueTimeOrCount > 0) {
                SpawnSp01(outSpawns, nextId, kClassSp01BigTeacherChild, s.idInSp01Info,
                          s.x, s.y, 0, kSp01Appear, rng, cfg);
            } else {
                Sp01SetAction(s, kSp01BTeacherWaitResume, false);
            }
        }
        break;
    }
    case kSp01BTeacherWaitResume:
        if (HeroState* nearest = NearestHeroToMutable(heroes, s.x, s.y)) {
            if (Sp01TeacherTouchHero(s, *nearest)) HeroDoHurt(*nearest, kTeacherTouchHurt, rng);
        }
        if (!AnySp01OfClass(allSp01s, kClassSp01BigTeacherChild))
            Sp01SetAction(s, kSp01BTeacherResume, false);
        break;
    }
}

// ai_big_spTeacher: 体育老师 (ai_normal + Egg 倒计时)
static void Sp01AiBigSpTeacher(Sp01State& s, const HeroState* hero, const MapCollision& col, CBattleRng& rng) {
    Sp01AiNormal(s, hero, col, rng);
    if (s.action == kSp01BSpTeacherEgg) {
        --s.residueTimeOrCount;
        if (s.residueTimeOrCount <= 0) Sp01SetAction(s, kSp01BSpTeacherExitEgg, true);
    }
}

// animationDone: 动作完成回调 (原动画 clip 播完触发)
static void Sp01AnimationDone(Sp01State& s, std::vector<HeroState>& heroes, const MapCollision& col,
                              CBattleRng& rng, std::vector<Sp01State>& outSpawns, uint32_t& nextId,
                              std::vector<PropState>& outProps,
                              const std::map<uint32_t, Sp01Config>* cfg) {
    const HeroState* hero = NearestHeroTo(heroes, s.x, s.y);
    switch (s.action) {
    case kSp01Attack: {
        // 攻击动画播完一次 → 结算一次对英雄伤害 (对齐原 KeyFrameEvent 命中帧; 20 帧/击)。
        HeroState* nearest = NearestHeroToMutable(heroes, s.x, s.y);
        if (!nearest || !Sp01HurtHero(s, *nearest)) Sp01SetAction(s, kSp01Walk, true);
        else {
            HeroDoHurt(*nearest, FxToIntRound(FxMul(s.hurtToHero, FxFromInt(kHpScale))), rng);
            Sp01SetAction(s, kSp01Attack, true);
        }
        break;
    }
    case kSp01BGuardAttack1:
    case kSp01BGuardAttack2: {
        HeroState* nearest = NearestHeroToMutable(heroes, s.x, s.y);
        if (!nearest || !Sp01HurtHero(s, *nearest)) Sp01SetAction(s, kSp01Walk, true);
        else {
            HeroDoHurt(*nearest, FxToIntRound(FxMul(s.hurtToHero, FxFromInt(kHpScale))), rng);
            if (rng.NextU32() % 2 == 0) Sp01SetAction(s, kSp01BGuardAttack1, true);
            else Sp01SetAction(s, kSp01BGuardAttack2, true);
        }
        break;
    }
    case kSp01BGuardEquipAttack: {
        // 守卫装备态攻击 1.3 倍伤害 (原 KeyFrameEvent CLASS_ID_BIG_guard EQUIP* 分支)
        HeroState* nearest = NearestHeroToMutable(heroes, s.x, s.y);
        if (nearest && Sp01HurtHero(s, *nearest)) {
            int32_t hurt = FxToIntRound(FxMul(s.hurtToHero, FxFromInt(kHpScale)));
            hurt = hurt + hurt * 3 / 10;
            HeroDoHurt(*nearest, hurt, rng);
        }
        Sp01SetAction(s, kSp01BGuardEquipWalk, true);
        break;
    }
    case kSp01Die:
        Sp01SetAction(s, kSp01Disappear, true);
        s.remove = true;
        break;
    case kSp01Hurt:
        Sp01SetAction(s, kSp01Walk, true);
        break;
    case kSp01BGuardEquip:
        Sp01SetAction(s, kSp01BGuardEquipWalk, true);
        break;
    case kSp01BGuardUnload:
        Sp01SetAction(s, kSp01Walk, true);
        break;
    case kSp01BTeacherChange:
        s.residueTimeOrCount = 20;   // 原 m_fResidueTimeOrCount = 20 (出怪次数)
        s.troopsTiming = 0;
        Sp01SetAction(s, kSp01BTeacherTroops, true);
        break;
    case kSp01BTeacherResume:
        Sp01SetAction(s, kSp01Walk, true);
        break;
    case kSp01BSpTeacherToEgg:
        s.residueTimeOrCount = kSpTeacherEggFrames;   // 3s → 60 帧
        Sp01SetAction(s, kSp01BSpTeacherEgg, true);
        break;
    case kSp01BSpTeacherExitEgg: {
        // 分裂子体育老师 + 自身复活 (原 createASp01_SPTeacherChild + rebirth)
        fx_t cx, cy;
        if (!GetARandomPointArroundNotInCollides(s.x, s.y, FxFromInt(10), cx, cy, col, rng))
            { cx = s.x; cy = s.y; }
        SpawnSp01(outSpawns, nextId, kClassSp01BigSPTeacher, s.idInSp01Info, cx, cy,
                  s.variationCount, kSp01BSpTeacherFenLie, rng, cfg);
        // rebirth(0): 回满血 → appear
        s.hp = s.maxHp; s.hpPre = s.hp;
        s.remove = false;
        s.sp01PauseCount = 0;
        s.residueTimeOrCount = 0;
        s.troopsTiming = 0;
        s.stopFindHeroUntilGetNextPoint = false;
        Sp01SetAction(s, kSp01Appear, true);
        Sp01InitSleepWalking(s, rng, 20, kFxPiOver2);   // DEGREEN90_TO_RADIANS
        break;
    }
    case kSp01BSpTeacherFenLie:
        if (hero) Sp01FindHero(s, *hero);
        Sp01SetAction(s, kSp01Walk, true);
        Sp01InitSleepWalking(s, rng, 20, kFxPiOver2);
        break;
    default:
        break;
    }
}

// update: 单个 Sp01 每帧推进 (normal 小怪 + boss 大怪状态机)
static void UpdateSp01(Sp01State& s, std::vector<HeroState>& heroes,
                       const std::vector<Sp01State>& allSp01s, const MapCollision& col,
                       CBattleRng& rng, std::vector<Sp01State>& outSpawns, uint32_t& nextId,
                       std::vector<PropState>& outProps,
                       const std::map<uint32_t, Sp01Config>* cfg) {
    if (s.remove) return;

    // appear: 出生延迟倒计时 → 找英雄 → walk
    if (s.action == kSp01Appear) {
        if (s.sp01PauseCount > 0) {
            --s.sp01PauseCount;
        } else {
            const HeroState* hero = NearestHeroTo(heroes, s.x, s.y);
            if (hero) Sp01FindHero(s, *hero);
            Sp01SetAction(s, kSp01Walk, false);
        }
        return;
    }

    s.isFindHeroInThisFPS = false;

    // 子弹停顿冻结 (sp01PauseCount; 死亡不冻结)
    if (s.sp01PauseCount > 0) {
        --s.sp01PauseCount;
        if (s.action == kSp01Die) s.sp01PauseCount = -1;
        return;
    }

    const HeroState* hero = NearestHeroTo(heroes, s.x, s.y);

    // 每帧 AI 分派 (原 update 的 classId switch)
    if (s.classId == kClassSp01BigGuard) {
        Sp01AiBigGuard(s, hero, col, rng);
    } else if (s.classId == kClassSp01BigTeacher) {
        Sp01AiBigTeacher(s, heroes, allSp01s, col, rng, outSpawns, nextId, cfg);
    } else if (s.classId == kClassSp01BigSPTeacher) {
        Sp01AiBigSpTeacher(s, hero, col, rng);
    } else {
        // normal + teacher_child: 击退 + ai_normal
        Sp01CheckMoveBack(s, col);
        Sp01AiNormal(s, hero, col, rng);
    }

    // 动作计时 → animationDone (原动画系统在 update 之后回调)
    if (Sp01ActionFrames(s.action) > 0) {
        ++s.actionElapsedFrames;
        if (s.actionElapsedFrames >= Sp01ActionFrames(s.action))
            Sp01AnimationDone(s, heroes, col, rng, outSpawns, nextId, outProps, cfg);
    }
}

// 前向声明 (炮台, 定义在子弹逻辑之后; 子弹/火箭筒命中需在定义前使用)
static bool TurretCanNotHurt(const TurretState& t, const MapCollision& col);
static FxRect TurretGreenRect(const TurretState& t);
static void TurretChangeToHurt(TurretState& t, uint8_t bulletClassId, int32_t gunHurtValue,
                               const MapCollision& col, CBattleRng& rng,
                               std::vector<PropState>& outProps, uint32_t& nextId);

// ---------------------------------------------------------------------------
// 子弹 (Bullet) — 定点复刻 Bullet.cpp 的 update/doBullet_normal/doBullet_huojiantong2/
// doPtBullet_normal + Sp01Boss.cpp 的 hurtSp01_from_rpg / hurtSp01_from_fullScreenBomb。
//
// 关键约定:
//   - 怪/英雄受击盒用占位矩形 (C5 从动画抽取静态逻辑盒替换)。
//   - die_* 状态 = 命中后的表现动画 (纯视觉); 逻辑上命中即进入残留倒计时 (lifeLength)
//     后移除。例外: 火箭筒 die_baozha 到 0 先结算 AOE (0.1s=2 帧延迟) 再移除。
//   - 狙击/散弹穿透: hasHurtIds 记录已命中目标, 避免重复伤害。
// ---------------------------------------------------------------------------

// Sp01 受击盒 (green rect) — 脚底锚点向上生长的每类身体盒。
// 完整「动画帧 rectGreen 抽取」是 C5 遗留 (需离线解析 MigAnim XML), 先用 Sp01::initHPRect
// 的每类身体高度 (dog=50/girl=70/默认=70/BigGuard=105/BigTeacher=122/TeacherChild=50) 确定性近似。
// (s.x,s.y) 是脚底锚点 (HP 条 setPosition(ccp(-w/2, m_fHPHeight)) 从脚底往上画), 故盒子 y ∈ [s.y, s.y+h]。
static FxRect Sp01GreenRect(const Sp01State& s) {
    int32_t w = 36, h = 70;   // 默认 (chef/dama/boy/police/worker/zombie...)
    switch (s.classId) {
        case kClassSp01Dog:             w = 40; h = 50;  break;   // 狗: 宽矮
        case kClassSp01Girl:            w = 32; h = 70;  break;
        case kClassSp01BigGuard:        w = 48; h = 105; break;   // 大怪·守卫
        case kClassSp01BigTeacher:      w = 48; h = 122; break;   // 大怪·老师
        case kClassSp01BigTeacherChild: w = 32; h = 50;  break;   // 老师·小孩
        case kClassSp01BigSPTeacher:    w = 48; h = 122; break;   // 大怪·体育老师
        default:                        w = 36; h = 70;  break;
    }
    FxRect r;
    r.x = s.x - FxFromInt(w / 2);
    r.y = s.y;                       // 从脚底向上
    r.w = FxFromInt(w);
    r.h = FxFromInt(h);
    return r;
}

// 英雄受击盒 (body green rect) — 同样脚底锚点向上 (原 24x40 居中在 h.y-20 半截埋进脚下)
static FxRect HeroBodyGreenRect(const HeroState& h) {
    FxRect r;
    r.x = h.x - FxFromInt(12);
    r.y = h.y;                       // 从脚底向上
    r.w = FxFromInt(24);
    r.h = FxFromInt(48);
    return r;
}

// isGoOut: 出地图边界外 50px (原 Bullet::isGoOut)
static bool BulletGoOut(const BulletState& b, const MapCollision& col) {
    return b.x < FxFromInt(-50) || b.x > FxFromInt(col.mapWidth + 50) ||
           b.y > FxFromInt(col.mapHeight + 50) || b.y < FxFromInt(-50);
}

// isHitWall: 点撞 collide_bullet (原 Bullet::isHitWall)
static bool BulletHitWall(const BulletState& b, const MapCollision& col) {
    return col.ContainsBullet(b.x, b.y);
}

// 子弹扫掠子步长 (px): 单帧位移拆成若干 ≤ 该值的子步, 每个子步「先命中检测、后推进」。
// 受击盒最小边 32px, 子步长取 8px 保证高速子弹 (20px/帧, 乃至 >32px/帧) 不会跳过受击盒
// (隧道效应), 且近身开火 (枪口出生点已在受击盒内) 也能在首个检测点命中。
static const int32_t kBulletSweepStep = 8;

// addChangePos: 沿 rotate 方向前进一个子步 (≤ kBulletSweepStep), 返回本帧剩余未走完位移。
// 用子步扫掠替代「整帧一步 + 单点检测」, 每步终点都做命中检测, 消除高速子弹隧道穿透。
static fx_t BulletAddChangePos(BulletState& b, fx_t remain) {
    fx_t step = FxFromInt(kBulletSweepStep);
    if (remain < step) step = remain;
    b.x += FxMul(FxCos(b.rotate), step);
    b.y += FxMul(FxSin(b.rotate), step);
    return remain - step;
}

// 狙击/散弹穿透: 是否已命中该目标
static bool BulletAlreadyHurt(const BulletState& b, uint32_t sp01Id) {
    for (uint32_t v : b.hasHurtIds) if (v == sp01Id) return true;
    return false;
}

// 角度向目标收敛 (原 Tools::getNextTendDegreesCurToWill_180, 弧度版, 最多 maxStep)
static fx_t TendRadians(fx_t cur, fx_t will, fx_t maxStep) {
    fx_t diff = will - cur;
    while (diff > kFxPi)        diff -= kFxTwoPi;
    while (diff < FxNeg(kFxPi)) diff += kFxTwoPi;
    if (diff > maxStep)        diff = maxStep;
    else if (diff < FxNeg(maxStep)) diff = FxNeg(maxStep);
    return cur + diff;
}

// 火箭筒索敌 (原 Sp01Boss::getNearestSp01OfPoint, radius=kRpgTargetRadius)
static bool FindBulletTarget(const std::vector<Sp01State>& sp01s, const MapCollision& col,
                             fx_t bx, fx_t by, fx_t& tx, fx_t& ty) {
    bool found = false;
    int64_t best = INT64_MAX;
    int64_t r2 = (int64_t)kRpgTargetRadius << 16; r2 *= r2;
    for (const auto& s : sp01s) {
        if (Sp01CanNotHurt(s, col)) continue;
        int64_t dx = (int64_t)s.x - bx, dy = (int64_t)s.y - by;
        int64_t d2 = dx * dx + dy * dy;
        if (d2 < r2 && d2 <= best) { best = d2; tx = s.x; ty = s.y; found = true; }
    }
    return found;
}

// 全屏炸弹: 半径内所有怪 changeToHurt(全屏炸弹) (原 hurtSp01_from_fullScreenBomb)
static void HurtSp01FullScreenBomb(std::vector<Sp01State>& sp01s, fx_t px, fx_t py, int32_t radius,
                                   const MapCollision& col, CBattleRng& rng,
                                   std::vector<Sp01State>& outSpawns, uint32_t& nextId,
                                   std::vector<PropState>& outProps,
                                   std::vector<HeroState>& heroes, int64_t ownerPlayerId) {
    int64_t r2 = (int64_t)radius << 16; r2 *= r2;
    for (auto& s : sp01s) {
        if (Sp01CanNotHurt(s, col)) continue;
        int64_t dx = (int64_t)s.x - px, dy = (int64_t)s.y - py;
        if (dx * dx + dy * dy <= r2)
            Sp01ChangeToHurt(s, col, rng, kClassBulletFullScreenBomb, kGunHurtValue,
                             outSpawns, nextId, outProps, heroes, ownerPlayerId);
    }
}

// 火箭筒 AOE (原 Sp01Boss::hurtSp01_from_rpg, radius=kRpgExplodeRadius)
// 先打怪再打炮台 (顺序与原文一致)。
static void HurtSp01Rpg(std::vector<Sp01State>& sp01s, std::vector<TurretState>& turrets,
                        fx_t px, fx_t py,
                        const MapCollision& col, CBattleRng& rng,
                        std::vector<Sp01State>& outSpawns, uint32_t& nextId,
                        std::vector<PropState>& outProps,
                        std::vector<HeroState>& heroes, int64_t ownerPlayerId) {
    int64_t r2 = (int64_t)kRpgExplodeRadius << 16; r2 *= r2;
    for (auto& s : sp01s) {
        if (Sp01CanNotHurt(s, col)) continue;
        int64_t dx = (int64_t)s.x - px, dy = (int64_t)s.y - py;
        if (dx * dx + dy * dy <= r2)
            Sp01ChangeToHurt(s, col, rng, kClassBulletHuojiantong, kGunHurtValue,
                             outSpawns, nextId, outProps, heroes, ownerPlayerId);
    }
    for (auto& t : turrets) {
        if (TurretCanNotHurt(t, col)) continue;
        int64_t dx = (int64_t)t.x - px, dy = (int64_t)t.y - py;
        if (dx * dx + dy * dy <= r2)
            TurretChangeToHurt(t, kClassBulletHuojiantong, kGunHurtValue, col, rng,
                               outProps, nextId);
    }
}

// 子弹撞到 boss 挡弹状态 (守卫装备/老师变身/体育老师蛋) → 子弹撞墙而非命中
static bool Sp01BlocksBullet(const Sp01State& s) {
    if (s.classId == kClassSp01BigGuard &&
        (s.action == kSp01BGuardEquip || s.action == kSp01BGuardEquipWalk ||
         s.action == kSp01BGuardEquipAttack || s.action == kSp01BGuardUnload)) return true;
    if (s.classId == kClassSp01BigTeacher &&
        (s.action == kSp01BTeacherChange || s.action == kSp01BTeacherTroops ||
         s.action == kSp01BTeacherWaitResume || s.action == kSp01BTeacherResume)) return true;
    if (s.classId == kClassSp01BigSPTeacher &&
        (s.action == kSp01BSpTeacherToEgg || s.action == kSp01BSpTeacherEgg ||
         s.action == kSp01BSpTeacherExitEgg)) return true;
    return false;
}

// update: 单个子弹每帧推进 (反怪弹 / 火箭筒追踪 / 炮台弹)
static void UpdateBullet(BulletState& b, std::vector<Sp01State>& sp01s,
                         std::vector<TurretState>& turrets,
                         std::vector<HeroState>& heroes, const MapCollision& col,
                         CBattleRng& rng, std::vector<Sp01State>& outSpawns, uint32_t& nextId,
                         std::vector<PropState>& outProps) {
    if (b.remove) return;

    // 延迟起飞 (散弹/狙击 sleepTime)
    if (b.sleepTime > 0) { --b.sleepTime; return; }

    // die_* 状态统一: lifeLength 倒计时, 到 0 结算(火箭筒 AOE) + 移除
    if (b.action == kBulletDieHitWall || b.action == kBulletDieHitSp01 || b.action == kBulletDieBaozha) {
        if (b.lifeLength > 0) --b.lifeLength;
        if (b.lifeLength == 0) {
            if (b.action == kBulletDieBaozha) HurtSp01Rpg(sp01s, turrets, b.x, b.y, col, rng, outSpawns, nextId, outProps, heroes, b.ownerPlayerId);
            b.remove = true;
        }
        return;
    }

    // 非 move 状态不推进
    if (b.action != kBulletMove) return;

    switch (b.classId) {
    case kClassBulletHuojiantong: {
        // 到达目标 (双轴 <=30) 或首次 → 重选目标
        if (!b.hasTarget ||
            (FxAbs(b.targetX - b.x) <= FxFromInt(kRpgReachDist) &&
             FxAbs(b.targetY - b.y) <= FxFromInt(kRpgReachDist))) {
            b.hasTarget = FindBulletTarget(sp01s, col, b.x, b.y, b.targetX, b.targetY);
        }
        if (b.hasTarget) {
            fx_t will = FxAtan2(b.targetY - b.y, b.targetX - b.x);
            fx_t maxStep = FxDiv(FxMul(FxFromInt(kRpgHomingMaxTurnDeg), kFxPi), FxFromInt(180));
            b.rotate = TendRadians(b.rotate, will, maxStep);
        }
        // 扫掠推进: 每个子步终点都检测出界/撞墙/命中怪/命中炮台 (原单步检测会漏近距离目标)
        fx_t remain = b.speed;
        for (;;) {
            if (BulletGoOut(b, col)) { b.remove = true; return; }
            if (BulletHitWall(b, col)) { b.action = kBulletDieBaozha; b.lifeLength = kBulletDieFrames; return; }
            for (auto& s : sp01s) {
                if (Sp01CanNotHurt(s, col)) continue;
                if (Sp01GreenRect(s).Contains(b.x, b.y)) {
                    b.action = kBulletDieBaozha; b.lifeLength = kBulletDieFrames;
                    return;
                }
            }
            for (auto& t : turrets) {
                if (TurretCanNotHurt(t, col)) continue;
                if (TurretGreenRect(t).Contains(b.x, b.y)) {
                    b.action = kBulletDieBaozha; b.lifeLength = kBulletDieFrames;
                    return;
                }
            }
            if (remain <= 0) break;
            remain = BulletAddChangePos(b, remain);
        }
        // 每帧 1/30 概率重选目标
        if (rng.NextU32() % 30 == 0) b.hasTarget = FindBulletTarget(sp01s, col, b.x, b.y, b.targetX, b.targetY);
        break;
    }
    case kClassBulletPaota: {
        // 炮台弹: 命中英雄 (扫掠推进, 避免高速弹跳过英雄受击盒)
        fx_t remain = b.speed;
        for (;;) {
            if (BulletGoOut(b, col)) { b.remove = true; return; }
            if (BulletHitWall(b, col)) { b.action = kBulletDieHitWall; b.lifeLength = kBulletDieFrames; return; }
            HeroState* h = NearestHeroToMutable(heroes, b.x, b.y);
            if (h && HeroBodyGreenRect(*h).Contains(b.x, b.y)) {
                HeroDoHurt(*h, FxToIntRound(FxMul(b.hurt, FxFromInt(kHpScale))), rng);
                b.action = kBulletDieHitSp01; b.lifeLength = kBulletDieFrames;
                return;
            }
            if (remain <= 0) break;
            remain = BulletAddChangePos(b, remain);
        }
        break;
    }
    default: {
        // 反怪弹 (normal/juji/sandan/JiQiang/huopao): 扫掠推进, 每子步终点检测
        fx_t remain = b.speed;
        for (;;) {
            if (BulletGoOut(b, col)) { b.remove = true; return; }
            if (BulletHitWall(b, col)) { b.action = kBulletDieHitWall; b.lifeLength = kBulletDieFrames; return; }
            for (auto& s : sp01s) {
                if (Sp01CanNotHurt(s, col)) continue;
                if (!Sp01GreenRect(s).Contains(b.x, b.y)) continue;

                if (b.classId == kClassBulletJuji || b.classId == kClassBulletSandan) {
                    if (BulletAlreadyHurt(b, s.id)) continue;
                    b.hasHurtIds.push_back(s.id);
                    Sp01ChangeToHurt(s, col, rng, b.classId, kGunHurtValue, outSpawns, nextId, outProps, heroes, b.ownerPlayerId);
                    if (b.classId == kClassBulletSandan) {
                        // 散弹击退 (原 initMoveBackFromGun(rotate, 1, speed))
                        s.moveBackFromGun = true;
                        s.moveBackAllCount = 1;
                        s.moveBackRadians = b.rotate;
                        s.moveBackSpeed = b.speed;
                    }
                } else {
                    Sp01ChangeToHurt(s, col, rng, b.classId, kGunHurtValue, outSpawns, nextId, outProps, heroes, b.ownerPlayerId);
                    if (Sp01BlocksBullet(s)) { b.action = kBulletDieHitWall; b.lifeLength = kBulletDieFrames; return; }
                    b.action = kBulletDieHitSp01; b.lifeLength = kBulletDieFrames;
                    return;
                }
            }
            // 炮台受击 (原 doBullet_normal 对 m_pPTSp01s 的遍历)
            for (auto& t : turrets) {
                if (TurretCanNotHurt(t, col)) continue;
                if (!TurretGreenRect(t).Contains(b.x, b.y)) continue;

                if (b.classId == kClassBulletJuji || b.classId == kClassBulletSandan) {
                    if (BulletAlreadyHurt(b, t.id)) continue;
                    b.hasHurtIds.push_back(t.id);
                    TurretChangeToHurt(t, b.classId, kGunHurtValue, col, rng, outProps, nextId);
                } else {
                    TurretChangeToHurt(t, b.classId, kGunHurtValue, col, rng, outProps, nextId);
                    b.action = kBulletDieHitSp01; b.lifeLength = kBulletDieFrames;
                    return;
                }
            }
            if (remain <= 0) break;
            remain = BulletAddChangePos(b, remain);
        }
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// 炮台 (Turret) — 定点复刻 Turret.cpp 的 update/KeyFrameEvent/animationDone/
// changeToHurt + Sp01Boss.cpp 的 hurtSp01_from_rpg (对炮台的 AOE)。
//
// 关键约定:
//   - 动作时长用 TurretActionFrames 占位表 (C5 从动画数据抽取后替换)。
//   - stand 索敌命中英雄 → attack 并立即开火 (原 KeyFrameEvent 在攻击动画关键帧
//     触发, 此处简化为攻击起始即开火; C5 可改为固定关键帧帧数)。
//   - die → disappear → remove (原 die 播完转 disappear 后不再回收, 此处补移除)。
// ---------------------------------------------------------------------------

// 炮台动作时长表 (帧; 0 = 循环动作, 无 animationDone)
static int32_t TurretActionFrames(uint8_t action) {
    switch (action) {
    case kTurretShow:       return 15;
    case kTurretAttack:     return 20;
    case kTurretDie:        return 20;
    case kTurretHurt:       return 10;
    case kTurretDisappear:  return 10;
    default:                return 0;   // init/appear/stand 循环
    }
}

// 炮台受击盒 (green rect) — 放大到 48x40 居中 (炮台本体比 32x32 占位更大; 完整 rect 抽取留 C5)
static FxRect TurretGreenRect(const TurretState& t) {
    FxRect r;
    r.x = t.x - FxFromInt(24); r.y = t.y - FxFromInt(20);
    r.w = FxFromInt(48);     r.h = FxFromInt(40);
    return r;
}

// bCanNotHurtPT: init/appear/die/show/disappear/remove 或出地图边界时不可受伤
static bool TurretCanNotHurt(const TurretState& t, const MapCollision& col) {
    if (t.action == kTurretInit || t.action == kTurretAppear || t.action == kTurretDie ||
        t.action == kTurretShow || t.action == kTurretDisappear || t.remove) return true;
    const fx_t m = FxFromInt(5);
    return t.x < m || t.x > FxFromInt(col.mapWidth) - m ||
           t.y < m || t.y > FxFromInt(col.mapHeight) - m;
}

// randomCreateProp: 炮台死亡掉落 (RNG 顺序与 Sp01 版一致)
static void TurretRandomCreateProp(const TurretState& t, CBattleRng& rng,
                                   std::vector<PropState>& out, uint32_t& nextId) {
    if ((int32_t)(rng.NextU32() % 100) < t.dropCoinsProbability) {
        int32_t ch = t.dropCoinsMax - t.dropCoinsMin + 1;
        if (ch <= 0) ch = 1;
        int32_t num = t.dropCoinsMin + (int32_t)(rng.NextU32() % (uint32_t)ch);
        for (int32_t i = 0; i < num; ++i) {
            fx_t px, py;
            CreateRandomPointInCircle(rng, t.x, t.y, FxFromInt(60), 15, px, py);
            SpawnProp(out, nextId, kClassPropCoins, px, py, 5);
        }
    }
    if ((int32_t)(rng.NextU32() % 100) < t.dropSunShinePro) {
        int32_t ch = t.dropSunShineMax - t.dropSunShineMin + 1;
        if (ch <= 0) ch = 1;
        int32_t num = t.dropSunShineMin + (int32_t)(rng.NextU32() % (uint32_t)ch);
        for (int32_t i = 0; i < num; ++i) {
            fx_t px, py;
            CreateRandomPointInCircle(rng, t.x, t.y, FxFromInt(60), 15, px, py);
            SpawnProp(out, nextId, kClassPropSunShine, px, py, 1);
        }
    }
}

// changeToHurt: 炮台受击 (原 Turret::changeToHurt, 无暴击/停顿, 直接扣血)
static void TurretChangeToHurt(TurretState& t, uint8_t bulletClassId, int32_t gunHurtValue,
                               const MapCollision& col, CBattleRng& rng,
                               std::vector<PropState>& outProps, uint32_t& nextId) {
    if (TurretCanNotHurt(t, col)) return;
    int32_t curHurt = (bulletClassId == kClassBulletFullScreenBomb) ? kFullScreenBombHurt : gunHurtValue;
    t.hp -= curHurt;
    if (t.hp <= 0) {
        t.hp = 0;
        TurretRandomCreateProp(t, rng, outProps, nextId);
        t.action = kTurretDie;
        t.actionElapsedFrames = 0;
    } else {
        t.action = kTurretHurt;
        t.actionElapsedFrames = 0;
    }
}

// 炮台开火: 朝最近英雄打一发炮台弹 (原 KeyFrameEvent → createAPaoTaB)
static void TurretFire(TurretState& t, HeroState* h, std::vector<BulletState>& outBullets, uint32_t& nextId) {
    if (!h || !h->alive) return;
    BulletState b;
    b.id = nextId++;
    b.classId = kClassBulletPaota;
    b.action = kBulletMove;
    b.x = t.x; b.y = t.y;              // 占位: 炮口在炮台中心 (C5 从动画红框抽取)
    b.rotate = FxAtan2(h->y - t.y, h->x - t.x);
    b.speed = t.bulletSpeed;
    b.hurt = t.hurtToHero;
    outBullets.push_back(b);
}

// 枪型 → 子弹类 (复刻 WeaponManager::createABullet 的枪型分派: 狙击穿透/散弹穿透+击退/火箭筒追踪/
// 机枪/手枪。加特林走机枪弹类, 默认手枪)
static uint8_t GunToBulletClass(uint8_t gunType) {
    switch (gunType) {
        case kGunJuJi:        return kClassBulletJuji;        // 狙击 (穿透)
        case kGunSanDan:      return kClassBulletSandan;      // 散弹 (穿透 + 击退)
        case kGunHuoJianTong: return kClassBulletHuojiantong; // 火箭筒 (追踪 + AOE)
        case kGunJiQiang:
        case kGunJiaTeLin:    return kClassBulletJiQiang;     // 机枪 / 加特林
        case kGunShouQiang:
        default:              return kClassBulletNormal;      // 手枪
    }
}

// 枪型 → 枪口偏移表行索引 (0..5, 与 GunMuzzleOffset 表行序一致)。
static int32_t GunToMuzzleTableIndex(uint8_t gunType) {
    switch (gunType) {
        case kGunShouQiang:    return 0;   // 手枪
        case kGunJiQiang:      return 1;   // 机枪
        case kGunSanDan:       return 2;   // 散弹
        case kGunJuJi:         return 3;   // 狙击
        case kGunJiaTeLin:     return 4;   // 加特林
        case kGunHuoJianTong:  return 5;   // 火箭筒
        default:               return 0;
    }
}

// 枪口偏移 (px, 相对英雄脚底锚点) 按「枪型 × 8 方向」逐帧实测。原实现只取右朝向的平行偏移
// + 常数手高做旋转近似, 但 8 朝向是手绘预制帧、镜像不一致: 如手枪枪口右 55 / 左 -26 /
// 上 12 / 下 9, 横向差近 30px, 导致除正右外所有朝向「子弹发射位置与枪口不对齐」。
// 数值从 guns0.dat 各枪 fire_<朝向> 帧 rectRed 枪口中心 + hero0.dat stand_<朝向> 帧身体
// hand 挂点顶边校准。dir 编码同 AngleToEightDirFx: 1=上 2=右上 3=右 4=右下 5=下 6=左下
// 7=左 8=左上。
static void GunMuzzleOffset(uint8_t gunType, uint8_t dir, int32_t& mx, int32_t& my) {
    static const int32_t kMx[6][8] = {
        //   上    右上   右    右下   下    左下   左    左上
        {  12,   48,   55,   25,    9,  -19,  -26,   -5 },  // 手枪
        {   6,   49,   50,   23,    8,  -29,  -38,    6 },  // 机枪
        {   8,   48,   57,   27,   10,  -32,  -36,  -10 },  // 散弹
        {  11,   62,   68,   34,   14,  -40,  -48,   -6 },  // 狙击
        {  10,   76,   80,   40,   18,  -48,  -62,  -20 },  // 加特林
        {  28,   74,   64,   15,   -8,  -29,  -45,   11 },  // 火箭筒
    };
    static const int32_t kMy[6][8] = {
        {  56,   31,   18,   -9,  -12,    2,   19,   43 },  // 手枪
        {  68,   31,   23,  -12,  -15,    5,   22,   47 },  // 机枪
        {  51,   35,   14,  -16,  -17,    2,   17,   68 },  // 散弹
        {  70,   33,   15,  -24,  -28,    2,   20,   66 },  // 狙击
        {  73,   25,    3,  -44,  -47,  -22,    2,   68 },  // 加特林
        {  77,   39,   25,   -6,  -11,   15,   34,   70 },  // 火箭筒
    };
    const int32_t gi = GunToMuzzleTableIndex(gunType);
    const int32_t di = (dir >= 1 && dir <= 8) ? (dir - 1) : 2;   // 非法方向回退到右(3)
    mx = kMx[gi][di];
    my = kMy[gi][di];
}

// 英雄开火 → 产子弹实体 (原 StageScene::fireNetHeroes → WeaponManager::fireNetBullet →
// createABullet 的联网路径)。枪口位置按 8 方向查表 (与枪渲染 animDir 同量化), 保证子弹
// 出生点与枪口逐帧对齐; 飞行方向仍用连续瞄准角 aimRadians。反怪弹伤害在 UpdateBullet 里
// 走 kGunHurtValue, 故 b.hurt 留 0 (仅炮台弹/火箭筒命中用)。
static void SpawnHeroBullet(const HeroState& h, uint8_t gunType, fx_t aimRadians,
                            std::vector<BulletState>& outBullets, uint32_t& nextId) {
    const uint8_t dir = AngleToEightDirFx(aimRadians);   // 与 h.animDir 同量化, 匹配枪渲染帧
    int32_t mx, my;
    GunMuzzleOffset(gunType, dir, mx, my);
    BulletState b;
    b.id      = nextId++;
    b.classId = GunToBulletClass(gunType);
    b.action  = kBulletMove;
    b.x       = h.x + FxFromInt(mx);
    b.y       = h.y + FxFromInt(my);
    b.rotate  = aimRadians;
    b.speed   = FxFromInt(kHeroBulletSpeed);
    b.hurt    = 0;   // 反怪弹伤害走 kGunHurtValue; b.hurt 仅炮台弹命中英雄用
    b.ownerPlayerId = h.player_id;   // 击杀归属 (反怪弹 / 火箭筒 AOE / 全屏炸弹按此归属)
    outBullets.push_back(b);
}

// animationDone: 炮台动作完成 (原 Turret::animationDone)
static void TurretAnimationDone(TurretState& t) {
    int32_t dur = TurretActionFrames(t.action);
    if (dur <= 0) return;
    if (++t.actionElapsedFrames < dur) return;
    switch (t.action) {
    case kTurretShow:      t.action = kTurretStand; break;
    case kTurretAttack:    t.action = kTurretStand; break;
    case kTurretDie:       t.action = kTurretDisappear; break;
    case kTurretHurt:      t.action = kTurretStand; break;
    case kTurretDisappear: t.remove = true; break;
    default: break;
    }
    t.actionElapsedFrames = 0;
}

// update: 炮台每帧推进 (原 Turret::update)
static void UpdateTurret(TurretState& t, std::vector<HeroState>& heroes,
                         const MapCollision& col, std::vector<BulletState>& outBullets, uint32_t& nextId) {
    if (t.remove) return;

    // appear: 激活延迟 → show
    if (t.action == kTurretAppear) {
        if (t.delay > 0) --t.delay;
        if (t.delay <= 0) { t.action = kTurretShow; t.actionElapsedFrames = 0; }
        return;
    }

    // stand: 索敌 → attack (开火)
    if (t.action == kTurretStand) {
        HeroState* h = NearestHeroToMutable(heroes, t.x, t.y);
        if (h && h->alive && FxDistPx(t.x, t.y, h->x, h->y) < t.hurtHeroDis) {
            t.action = kTurretAttack;
            t.actionElapsedFrames = 0;
            TurretFire(t, h, outBullets, nextId);
        }
        return;
    }

    // 其它动作走 animationDone (show/attack/die/hurt/disappear)
    TurretAnimationDone(t);
}

// ---------------------------------------------------------------------------
// Init — 清空全部状态 + 按玩家列表出生英雄 (定点坐标)
// ---------------------------------------------------------------------------
bool BattleGame::Init(const std::vector<int64_t>& playerIds) {
    m_heroes.clear();
    m_sp01s.clear();
    m_bullets.clear();
    m_turrets.clear();
    m_props.clear();
    m_wave = WaveState();
    m_game = GameState();
    m_pendingSpawns.clear();
    m_pendingProps.clear();
    m_turretsSpawned = false;

    for (size_t i = 0; i < playerIds.size(); ++i) {
        HeroState h;
        h.id        = (uint32_t)(i + 1);   // 确定性 id (1 起)
        h.player_id = playerIds[i];
        // 出生点: 按索引横向分布 (确定性), 首人锚定 (160,240) 与单机/旧口径一致。
        // 多人间距由 160 收紧到 90: 原 x=160*(i+1) 在 5 人时首尾相距 640px,
        // 相机聚焦本端英雄 (800x480 视口) 时另一端落在视口外 → 表现为部分角色不可见。
        h.x = FxFromInt(160 + (int32_t)i * 90);
        h.y = FxFromInt(240);
        h.hp = 100 * kHpScale;
        h.maxHp = 100 * kHpScale;
        h.walkSpeed = FxFromFloat(6.875f);   // 英雄默认移速 137.5 px/s ÷ 20Hz = 6.875 px/帧
        h.speedMul  = kFxOne;                // 无移速 buff (1.0)
        h.facing = 0;
        h.action = kHeroStand;
        h.animDir = 3;                 // 右
        h.alive = true;
        m_heroes.push_back(h);
    }
    return !m_heroes.empty();
}

// ---------------------------------------------------------------------------
// SetHeroGun — 初始化英雄枪械 (局外系统传入的玩家主枪)
// ---------------------------------------------------------------------------
bool BattleGame::SetHeroGun(int64_t playerId, uint8_t gunType) {
    for (auto& h : m_heroes) {
        if (h.player_id == playerId) {
            h.gunType = gunType;   // 只改初始枪械类别; reportedGun 保持默认(手枪) 供首帧变化检测
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Step — 推进一个逻辑帧 (消费本帧输入 → 更新实体状态)
// ---------------------------------------------------------------------------
void BattleGame::Step(uint32_t frame, CBattleRng& rng, uint32_t& nextId, std::vector<PlayerInput>& inputs) {
    // 本帧新产子弹 (英雄开火 / 炮台开火) 先挂此向量, 帧末统一挂载 → 下一帧才开始动
    std::vector<BulletState> pendingBullets;

    // 按英雄数组顺序 (m_heroes, 两端 Init 用同一有序花名册 → 顺序一致) 处理输入,
    // 而非按 SubmitInput 的到达顺序。否则「本端在前」的客户端输入顺序与服务器
    // rosterOrder 不同时, 两人同帧开火的子弹 id (nextId) 分配顺序错位 → 哈希漂移。
    for (auto& h : m_heroes) {
        if (!h.alive) continue;
        const PlayerInput* in = nullptr;
        for (const auto& p : inputs) {
            if (p.player_id == h.player_id) { in = &p; break; }
        }
        if (!in) continue;

        // 换枪同步: 仅当客户端上报的枪械类别「变化」时更新 (显式切枪事件)。
        // 用 reportedGun 记住上次上报值 (随快照序列化/哈希, 回滚安全), 避免拾取
        // 火箭筒/加特林后 (HeroHitProps 确定性改 h.gunType) 被陈旧输入覆盖回手枪。
        if (h.reportedGun != (uint8_t)in->gun_type) {
            h.reportedGun = (uint8_t)in->gun_type;
            h.gunType    = (uint8_t)in->gun_type;
        }

        // 移动 + 碰撞 (定点, 复刻 Hero::changeHeroPos/checkCollidesToAddPos)
        MoveHero(h, in->move_dir, m_collision);

        // 朝向/开火: 有射击向量 (fire 且 shoot_dx/shoot_dy 任一非零) → 按连续角度;
        // 否则回退 move_dir (8 方向) 朝向。facing/aimRadians 记连续弧度, animDir 记量化
        // 8 方向 (渲染选动画用)。角度约定: (dx=1,dy=0)=右(0), (dx=0,dy=1)=上(+π/2),
        // 与 FxAtan2/FxSinCos 的数学坐标 (Y 向上) 一致。
        const bool hasShootDir = in->fire && (in->shoot_dx != 0 || in->shoot_dy != 0);
        if (hasShootDir) {
            const fx_t aim = FxAtan2(in->shoot_dy, in->shoot_dx);
            h.facing     = aim;
            h.aimRadians = aim;
            h.animDir    = AngleToEightDirFx(aim);
            h.moveDir    = in->move_dir;
        } else if (in->move_dir != 0) {
            h.facing  = DirToRadiansFx(in->move_dir);
            h.animDir = (uint8_t)(in->move_dir);   // 占位: 直接复用 1-8
            h.moveDir = in->move_dir;
        }

        // 开火: 产子弹实体 (复刻联网 fireNetBullet → createABullet; 命中/伤害交给
        // UpdateBullet, 不再 hitscan)。射击向量决定弹道角度, 冷却用 lastShootFrame。
        const bool fired = (hasShootDir &&
                            (h.lastShootFrame == 0 || frame - h.lastShootFrame >= kHeroFireIntervalFrames));
        if (fired) {
            h.lastShootFrame = frame;
            SpawnHeroBullet(h, h.gunType, h.aimRadians, pendingBullets, nextId);
        }

        // 动作状态: 移动/开火 → kHeroWalk/kHeroStandFire/kHeroWalkFire (渲染壳据此选动画;
        // kHero* 与客户端 ActionState_Hero 值一一对应)。朝向 animDir 已在上方按 facing 写。
        h.action = (in->move_dir != 0) ? (fired ? kHeroWalkFire : kHeroWalk)
                                      : (fired ? kHeroStandFire : kHeroStand);

        // 技能: 全屏炸弹 (原 StageScene::skillButtonPressed → doFullScreenBomb)
        // 引信倒计时入 HeroState::bombFuse, 每帧在敌人更新后结算 (见下 UpdateFullScreenBomb)。
        if (in->skill_id == kSkillFullScreenBomb && h.bombFuse <= 0) {
            h.bombFuse = kFullScreenBombFuseFrames;
        }
    }
    inputs.clear();

    // 炮台首帧出生 (配置注入后首次 Step 一次性生成; 幂等, 本帧即可被波次激活)
    SpawnInitialTurrets(nextId);

    // 波次刷怪 (darklayerba::update 定点复刻; 本帧新生的小怪下面 AI 阶段即生效)
    UpdateWave(rng, nextId);

    // 敌人 AI/移动 (normal 小怪 + boss 大怪状态机) — 按 index 遍历, 顺序确定
    for (size_t i = 0; i < m_sp01s.size(); ++i) {
        UpdateSp01(m_sp01s[i], m_heroes, m_sp01s, m_collision, rng,
                   m_pendingSpawns, nextId, m_pendingProps, &m_sp01Config);
    }

    // 炮台更新 (appear→show→stand→attack→fire) — 按 index 遍历, 顺序确定
    for (size_t i = 0; i < m_turrets.size(); ++i) {
        UpdateTurret(m_turrets[i], m_heroes, m_collision, pendingBullets, nextId);
    }

    // 子弹移动 / 命中 (反怪弹 / 火箭筒 / 炮台弹) — 按 index 遍历, 顺序确定
    for (size_t i = 0; i < m_bullets.size(); ++i) {
        UpdateBullet(m_bullets[i], m_sp01s, m_turrets, m_heroes, m_collision, rng,
                     m_pendingSpawns, nextId, m_pendingProps);
    }

    // 道具更新 (金币/阳光/武器拾取/超时/炸弹下落) — 已存在道具先动, 新掉落下一帧才动
    for (size_t i = 0; i < m_props.size(); ++i) {
        UpdateProp(m_props[i], m_heroes, m_game);
    }

    // 全屏炸弹引信 (原 doFullScreenBomb 的 0.5s 延迟 → 10 帧倒计时; 到 0 以英雄当前坐标结算)
    for (auto& h : m_heroes) {
        if (!h.alive || h.bombFuse <= 0) continue;
        if (--h.bombFuse == 0) {
            HurtSp01FullScreenBomb(m_sp01s, h.x, h.y, kFullScreenBombRadius, m_collision, rng,
                                   m_pendingSpawns, nextId, m_pendingProps, m_heroes, h.player_id);
        }
    }

    // 本帧生成的子实体/掉落/炮台弹挂载 (teacher 出怪 / spTeacher 分裂 / 掉落 / 炮台开火)
    if (!m_pendingSpawns.empty()) {
        m_sp01s.insert(m_sp01s.end(), m_pendingSpawns.begin(), m_pendingSpawns.end());
        m_pendingSpawns.clear();
    }
    if (!m_pendingProps.empty()) {
        m_props.insert(m_props.end(), m_pendingProps.begin(), m_pendingProps.end());
        m_pendingProps.clear();
    }
    if (!pendingBullets.empty()) {
        m_bullets.insert(m_bullets.end(), pendingBullets.begin(), pendingBullets.end());
    }

    // 清理 remove=true 的敌人 / 炮台 / 子弹 / 道具 (die → disappear 后 / 拾取 / 超时)
    m_sp01s.erase(std::remove_if(m_sp01s.begin(), m_sp01s.end(),
                  [](const Sp01State& s) { return s.remove; }), m_sp01s.end());
    m_turrets.erase(std::remove_if(m_turrets.begin(), m_turrets.end(),
                    [](const TurretState& t) { return t.remove; }), m_turrets.end());
    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(),
                    [](const BulletState& b) { return b.remove; }), m_bullets.end());
    m_props.erase(std::remove_if(m_props.begin(), m_props.end(),
                  [](const PropState& p) { return p.remove; }), m_props.end());

    // 墓碑复活结算 (合作玩法): 每个死亡英雄, 统计站在其墓碑范围 (kRespawnRangePx) 内的
    // 存活队友数 standing; standing>0 时 respawnTimer += standing (多人站立加速复活),
    // 达到 kRespawnFrames 复活 (原地, 满血, 站姿)。纯位置判定, 不消耗 RNG, 不影响 PRNG 流;
    // 复用本帧最终坐标 (英雄更新/受击已在上方完成)。
    // 单人战斗 (仅 1 个英雄): 无队友可救, 死亡后 respawnTimer 每帧自增 1 (免队友站立),
    // 同样 kRespawnFrames 帧后原地满血直接复活; 死亡次数照常累计, 供 Lua 侧
    // 「总死亡 > kMaxTotalDeaths」判负 (与多人一致)。
    const bool solo = (m_heroes.size() == 1);
    for (auto& h : m_heroes) {
        if (h.alive) continue;
        int32_t standing = 0;
        if (solo) {
            standing = 1;   // 单人: 固定每帧 +1, 无需队友站立即可自动复活
        } else {
            for (const auto& t : m_heroes) {
                if (!t.alive || t.player_id == h.player_id) continue;
                if (FxDistPx(t.x, t.y, h.x, h.y) < kRespawnRangePx) ++standing;
            }
        }
        if (standing > 0) {
            h.respawnTimer += standing;
            if (h.respawnTimer >= kRespawnFrames) {
                h.alive = true;
                h.hp = h.maxHp;
                h.action = kHeroStand;
                h.respawnTimer = 0;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 炮台出生/激活 (S4; 原 Sp01Boss::initPTSp01s + activeThisTurret 的定点复刻)
//
// SpawnInitialTurrets: 首帧幂等, 按 m_turretSpawns 文件序生成 kTurretInit 状态炮台 (不激活),
//   属性按 ptinfoId 查 m_turretConfig, 无配置行回退默认 (与 SpawnSp01 一致)。
// ActivateTurret: 波次 pt_N 事件触发, 使同名 (nameIndex) 炮台 init→appear(带 delay)。
//   客户端 m_pPTSp01s 用 CCDictionary 按 name 存, 同名后写覆盖前写; 内核按「最后一个同名」
//   对齐该语义 (level62 重复 pt_1 数据瑕疵亦逐位一致)。
// ---------------------------------------------------------------------------
void BattleGame::SpawnInitialTurrets(uint32_t& nextId) {
    if (m_turretsSpawned) return;
    m_turretsSpawned = true;
    for (const auto& sp : m_turretSpawns) {
        TurretState t;
        t.id = nextId++;
        t.nameIndex = sp.nameIndex;
        t.x = sp.x;
        t.y = sp.y;
        t.action = kTurretInit;
        t.active = false;
        t.remove = false;

        // 属性按 ptinfoId 查表; 无配置行回退默认 (确定性, 与 SpawnSp01 回退口径一致)
        int32_t hp = 100;
        fx_t hurt = FxFromInt(0);
        fx_t speed = FxFromInt(0);
        int32_t dis = 0;
        std::map<int32_t, TurretConfig>::const_iterator it = m_turretConfig.find(sp.ptinfoId);
        if (it != m_turretConfig.end()) {
            hp    = it->second.maxHp;
            hurt  = it->second.hurtToHero;
            speed = it->second.bulletSpeed;
            dis   = it->second.hurtHeroDis;
        }
        t.hp = hp * kHpScale;
        t.maxHp = hp * kHpScale;
        t.hurtToHero = hurt;
        t.bulletSpeed = speed;
        t.hurtHeroDis = dis;
        m_turrets.push_back(t);
    }
}

void BattleGame::ActivateTurret(int32_t nameIndex, int32_t delay) {
    // 客户端 m_pPTSp01s (CCDictionary) 按 name 覆盖 → 只激活最后一个同名炮台
    // (level62 重复 pt_1 数据瑕疵亦逐位一致)。未 spawn / 已激活过则忽略 (幂等)。
    TurretState* target = nullptr;
    for (auto& t : m_turrets) {
        if (t.nameIndex == nameIndex) target = &t;   // 取最后一个同名
    }
    if (!target) return;
    if (target->action != kTurretInit) return;   // 已激活过则忽略
    target->action = kTurretAppear;
    target->delay  = delay;
    target->active = true;
}

// 波次刷怪 (Sp01Boss::update + darklayerba::update 的定点复刻)
//
// 出生进度由 waveTimer 唯一推导 (WaveGroupSpawnedAt 纯函数), 快照只存 waveNum/waveTimer,
// 天然回滚安全。清场只按小怪 (m_sp01s) 判定, 炮台不挡波, 对齐 Sp01Boss::update。
// ---------------------------------------------------------------------------
void BattleGame::StartWave(int32_t waveNum) {
    m_wave.waveNum = waveNum;
    m_wave.waveTimer = 0;
    const WaveDef& wd = m_wavePlan[(size_t)waveNum - 1];
    if (wd.isKey) ++m_wave.xianShiWaveNum;
}

void BattleGame::UpdateWave(CBattleRng& rng, uint32_t& nextId) {
    if (m_wavePlan.empty() || m_wave.allWavesCleared) return;

    // 波间暂停 (gamePauseTime > 0 时不刷怪、不计时)
    if (m_wave.gamePauseTime > 0) {
        m_wave.gamePauseTime -= kFxOne;
        return;
    }

    if (m_wave.waveNum == 0) StartWave(1);

    const WaveDef& wd = m_wavePlan[(size_t)m_wave.waveNum - 1];
    const int32_t T = m_wave.waveTimer;

    // 本帧应出生数量 = SpawnedAt(T) - SpawnedAt(T-1) (startTime 刷第 1 个, 之后每 interval 一个)
    for (const auto& g : wd.groups) {
        int32_t due = WaveGroupSpawnedAt(g, T) - WaveGroupSpawnedAt(g, T - 1);
        if (g.classId == kClassTurret) {
            // 炮台激活 (pt_N): count=1 → 恰在 startTime 触发一次; interval 复用承载 appear 延迟
            for (int32_t k = 0; k < due; ++k) ActivateTurret(g.sp01Id, g.interval);
            continue;
        }
        fx_t x = 0, y = 0;
        WaveSpawnPos(m_collision, g.entryPoint, x, y);
        for (int32_t k = 0; k < due; ++k) {
            // C5: 按波次组的 sp01Id 查 Sp01Config 注入属性 (hp/走速/碰撞伤害)
            SpawnSp01(m_sp01s, nextId, g.classId, g.sp01Id, x, y, 0, kSp01Appear,
                      rng, &m_sp01Config);
        }
    }

    // 本波全部出生完成?
    bool allSpawned = true;
    for (const auto& g : wd.groups) {
        if (WaveGroupSpawnedAt(g, T) != g.count) { allSpawned = false; break; }
    }

    m_wave.waveTimer = T + 1;

    // 清场 → 下一波 / 胜利
    if (allSpawned && m_sp01s.empty()) {
        if (m_wave.waveNum >= (int32_t)m_wavePlan.size()) {
            m_wave.allWavesCleared = true;
        } else {
            StartWave(m_wave.waveNum + 1);
        }
    }
}

// ---------------------------------------------------------------------------
// SerializeState / DeserializeState — 全量快照 (游戏状态部分)
// ---------------------------------------------------------------------------
void BattleGame::SerializeState(BinWriter& w) const {
    // 地图碰撞 (MapCollision)
    w.i32(m_collision.mapWidth);
    w.i32(m_collision.mapHeight);
    w.u32((uint32_t)m_collision.people.size());
    for (const auto& r : m_collision.people) { w.fx(r.x); w.fx(r.y); w.fx(r.w); w.fx(r.h); }
    w.u32((uint32_t)m_collision.bullet.size());
    for (const auto& r : m_collision.bullet) { w.fx(r.x); w.fx(r.y); w.fx(r.w); w.fx(r.h); }
    w.u32((uint32_t)m_collision.entries.size());
    for (const auto& e : m_collision.entries) { w.fx(e.x); w.fx(e.y); }

    // 波次状态 (WaveState)
    w.i32(m_wave.waveNum);
    w.i32(m_wave.xianShiWaveNum);
    w.fx(m_wave.gamePauseTime);
    w.i32(m_wave.waveTimer);
    w.bl(m_wave.allWavesCleared);

    // 本局经济 (金币/钻石)
    w.i32(m_game.coins);
    w.i32(m_game.diamond);

    w.u32((uint32_t)m_heroes.size());
    for (const auto& h : m_heroes) WriteHero(w, h);
    w.u32((uint32_t)m_sp01s.size());
    for (const auto& s : m_sp01s) WriteSp01(w, s);
    w.u32((uint32_t)m_bullets.size());
    for (const auto& b : m_bullets) WriteBullet(w, b);
    w.u32((uint32_t)m_turrets.size());
    for (const auto& t : m_turrets) WriteTurret(w, t);
    w.u32((uint32_t)m_props.size());
    for (const auto& p : m_props) WriteProp(w, p);
}

bool BattleGame::DeserializeState(BinReader& r) {
    MapCollision mc;
    WaveState wv;
    uint32_t pc = 0, bc = 0;
    if (!r.i32(mc.mapWidth) || !r.i32(mc.mapHeight)) return false;
    if (!r.u32(pc)) return false;
    mc.people.resize(pc);
    for (auto& rect : mc.people) if (!r.fx(rect.x) || !r.fx(rect.y) || !r.fx(rect.w) || !r.fx(rect.h)) return false;
    if (!r.u32(bc)) return false;
    mc.bullet.resize(bc);
    for (auto& rect : mc.bullet) if (!r.fx(rect.x) || !r.fx(rect.y) || !r.fx(rect.w) || !r.fx(rect.h)) return false;
    uint32_t ec = 0;
    if (!r.u32(ec)) return false;
    mc.entries.resize(ec);
    for (auto& e : mc.entries) if (!r.fx(e.x) || !r.fx(e.y)) return false;
    if (!r.i32(wv.waveNum) || !r.i32(wv.xianShiWaveNum) || !r.fx(wv.gamePauseTime)) return false;
    if (!r.i32(wv.waveTimer) || !r.bl(wv.allWavesCleared)) return false;

    GameState gs;
    if (!r.i32(gs.coins) || !r.i32(gs.diamond)) return false;

    std::vector<HeroState> heroes;
    std::vector<Sp01State> sp01s;
    std::vector<BulletState> bullets;
    std::vector<TurretState> turrets;
    std::vector<PropState> props;

    uint32_t cnt = 0;
    if (!r.u32(cnt)) return false; heroes.resize(cnt);
    for (auto& h : heroes) if (!ReadHero(r, h)) return false;
    if (!r.u32(cnt)) return false; sp01s.resize(cnt);
    for (auto& s : sp01s) if (!ReadSp01(r, s)) return false;
    if (!r.u32(cnt)) return false; bullets.resize(cnt);
    for (auto& b : bullets) if (!ReadBullet(r, b)) return false;
    if (!r.u32(cnt)) return false; turrets.resize(cnt);
    for (auto& t : turrets) if (!ReadTurret(r, t)) return false;
    if (!r.u32(cnt)) return false; props.resize(cnt);
    for (auto& p : props) if (!ReadProp(r, p)) return false;

    m_collision = mc;
    m_wave = wv;
    m_game = gs;
    m_heroes = heroes;
    m_sp01s = sp01s;
    m_bullets = bullets;
    m_turrets = turrets;
    m_props = props;
    return true;
}

// ---------------------------------------------------------------------------
// HashInto — 状态一致性哈希 (FNV-1a 64 位, 游戏状态部分)
// 字段顺序与 SerializeState 对齐 (等价于对序列化流做 FNV-1a)。
// ---------------------------------------------------------------------------
void BattleGame::HashInto(Hasher& hasher) const {
    hasher.mix32((uint32_t)m_collision.mapWidth);
    hasher.mix32((uint32_t)m_collision.mapHeight);
    hasher.mix32((uint32_t)m_collision.people.size());
    for (const auto& r : m_collision.people) { hasher.mixFx(r.x); hasher.mixFx(r.y); hasher.mixFx(r.w); hasher.mixFx(r.h); }
    hasher.mix32((uint32_t)m_collision.bullet.size());
    for (const auto& r : m_collision.bullet) { hasher.mixFx(r.x); hasher.mixFx(r.y); hasher.mixFx(r.w); hasher.mixFx(r.h); }
    hasher.mix32((uint32_t)m_collision.entries.size());
    for (const auto& e : m_collision.entries) { hasher.mixFx(e.x); hasher.mixFx(e.y); }

    hasher.mix32((uint32_t)m_wave.waveNum);
    hasher.mix32((uint32_t)m_wave.xianShiWaveNum);
    hasher.mixFx(m_wave.gamePauseTime);
    hasher.mix32((uint32_t)m_wave.waveTimer);
    hasher.mix(m_wave.allWavesCleared ? 1 : 0);

    hasher.mix32((uint32_t)m_heroes.size());
    for (const auto& h : m_heroes) {
        hasher.mix32(h.id); hasher.mix64((uint64_t)h.player_id);
        hasher.mixFx(h.x); hasher.mixFx(h.y); hasher.mixFx(h.facing); hasher.mixFx(h.aimRadians);
        hasher.mix32((uint32_t)h.hp); hasher.mix32((uint32_t)h.maxHp);
        hasher.mixFx(h.walkSpeed); hasher.mixFx(h.speedMul);
        hasher.mix(h.action); hasher.mix(h.animDir); hasher.mix(h.moveDir); hasher.mix(h.alive ? 1 : 0);
        hasher.mix(h.invincible); hasher.mix32((uint32_t)h.hasGetHurtNum);
        hasher.mix(h.gunType); hasher.mix(h.preGun); hasher.mix(h.reportedGun); hasher.mix32((uint32_t)h.curHasShootNum);
        hasher.mix32((uint32_t)h.numBulletShouldShoot); hasher.mix32((uint32_t)h.numBulletHasShoot);
        hasher.mixFx(h.timeBulletInterval); hasher.mix32(h.lastShootFrame); hasher.mix(h.waitingShuttleInterval ? 1 : 0);
        hasher.mix32((uint32_t)h.bombFuse);
        hasher.mix32((uint32_t)h.killCount);
        hasher.mix32((uint32_t)h.deathCount); hasher.mix32((uint32_t)h.respawnTimer);
    }
    hasher.mix32((uint32_t)m_sp01s.size());
    for (const auto& s : m_sp01s) {
        hasher.mix32(s.id); hasher.mix(s.classId); hasher.mix32((uint32_t)s.idInSp01Info);
        hasher.mixFx(s.x); hasher.mixFx(s.y); hasher.mixFx(s.radians);
        hasher.mix32((uint32_t)s.hp); hasher.mix32((uint32_t)s.maxHp); hasher.mix32((uint32_t)s.hpPre);
        hasher.mix(s.action); hasher.mix(s.animDir); hasher.mix(s.remove ? 1 : 0);
        hasher.mixFx(s.walkSpeed); hasher.mixFx(s.moveBackSpeed); hasher.mixFx(s.moveBackRadians);
        hasher.mix32((uint32_t)s.moveBackAllCount); hasher.mix(s.moveBackFromGun ? 1 : 0); hasher.mix(s.stopMoveTemp ? 1 : 0);
        hasher.mix32((uint32_t)s.sleepWalkingCount); hasher.mix32((uint32_t)s.variationCount); hasher.mix32((uint32_t)s.residueTimeOrCount);
        hasher.mix32((uint32_t)s.troopsTiming); hasher.mix32((uint32_t)s.actionElapsedFrames); hasher.mix32((uint32_t)s.sp01PauseCount); hasher.mix32((uint32_t)s.stopTimeCount);
        hasher.mix(s.stopFindHeroUntilGetNextPoint ? 1 : 0); hasher.mix(s.isFindHeroInThisFPS ? 1 : 0); hasher.mix(s.andRadiansOrMinus ? 1 : 0);
        hasher.mixFx(s.hurtToHero);
        hasher.mix32((uint32_t)s.dropCoinsProbability); hasher.mix32((uint32_t)s.dropCoinsMax); hasher.mix32((uint32_t)s.dropCoinsMin);
        hasher.mix32((uint32_t)s.dropHuoJianTongPro); hasher.mix32((uint32_t)s.dropJiaTeLinPro); hasher.mix32((uint32_t)s.dropSunShinePro);
        hasher.mix32((uint32_t)s.dropSunShineMax); hasher.mix32((uint32_t)s.dropSunShineMin); hasher.mix32((uint32_t)s.andLight);
        hasher.mixFx(s.nextX); hasher.mixFx(s.nextY); hasher.mix(s.hasNextPoint ? 1 : 0);
    }
    hasher.mix32((uint32_t)m_bullets.size());
    for (const auto& b : m_bullets) {
        hasher.mix32(b.id); hasher.mix(b.classId); hasher.mix(b.action);
        hasher.mixFx(b.x); hasher.mixFx(b.y); hasher.mixFx(b.rotate); hasher.mixFx(b.speed); hasher.mixFx(b.hurt);
        hasher.mix32((uint32_t)b.lifeLength); hasher.mix32((uint32_t)b.sleepTime); hasher.mix(b.remove ? 1 : 0);
        hasher.mix32((uint32_t)b.hasHurtIds.size());
        for (uint32_t v : b.hasHurtIds) hasher.mix32(v);
        hasher.mixFx(b.targetX); hasher.mixFx(b.targetY); hasher.mix(b.hasTarget ? 1 : 0);
        hasher.mix64((uint64_t)b.ownerPlayerId);
    }
    hasher.mix32((uint32_t)m_turrets.size());
    for (const auto& t : m_turrets) {
        hasher.mix32(t.id); hasher.mix32((uint32_t)t.nameIndex); hasher.mixFx(t.x); hasher.mixFx(t.y); hasher.mixFx(t.aim);
        hasher.mix32((uint32_t)t.hp); hasher.mix32((uint32_t)t.maxHp); hasher.mix(t.action);
        hasher.mix32((uint32_t)t.hurtHeroDis); hasher.mix32((uint32_t)t.delay); hasher.mix(t.active ? 1 : 0); hasher.mix(t.remove ? 1 : 0);
        hasher.mixFx(t.hurtToHero); hasher.mixFx(t.bulletSpeed); hasher.mix32((uint32_t)t.actionElapsedFrames);
        hasher.mix32((uint32_t)t.dropCoinsProbability); hasher.mix32((uint32_t)t.dropCoinsMax); hasher.mix32((uint32_t)t.dropCoinsMin);
        hasher.mix32((uint32_t)t.dropSunShinePro); hasher.mix32((uint32_t)t.dropSunShineMax); hasher.mix32((uint32_t)t.dropSunShineMin);
    }
    hasher.mix32((uint32_t)m_props.size());
    for (const auto& p : m_props) {
        hasher.mix32(p.id); hasher.mix(p.classId); hasher.mix(p.action);
        hasher.mixFx(p.x); hasher.mixFx(p.y); hasher.mixFx(p.endX); hasher.mixFx(p.endY);
        hasher.mix32((uint32_t)p.value); hasher.mix32((uint32_t)p.velocity); hasher.mix32((uint32_t)p.acceleration);
        hasher.mix32((uint32_t)p.delay); hasher.mix32((uint32_t)p.life); hasher.mix32((uint32_t)p.fade); hasher.mix(p.remove ? 1 : 0);
    }
    hasher.mix32((uint32_t)m_game.coins);
    hasher.mix32((uint32_t)m_game.diamond);
}

// ---------------------------------------------------------------------------
// CheckWin — 权威胜负判定
// ---------------------------------------------------------------------------
int32_t BattleGame::CheckWin() const {
    int32_t aliveCount = 0;
    int32_t aliveIndex = -1;
    for (size_t i = 0; i < m_heroes.size(); ++i) {
        if (m_heroes[i].alive) {
            ++aliveCount;
            aliveIndex = (int32_t)i + 1;
        }
    }
    if (aliveCount == 0) return -1;            // 平局 (全员阵亡)
    if (aliveCount == 1) return aliveIndex;    // 唯一存活者胜
    return 0;                                   // 战斗继续
}

// ---------------------------------------------------------------------------
// HurtSp01ById — 子弹命中后调用 (P2 伤害入口)
// ---------------------------------------------------------------------------
void BattleGame::HurtSp01ById(uint32_t id, uint8_t bulletClassId, CBattleRng& rng, uint32_t& nextId) {
    for (auto& s : m_sp01s) {
        if (s.id != id) continue;
        Sp01ChangeToHurt(s, m_collision, rng, bulletClassId, kGunHurtValue,
                         m_pendingSpawns, nextId, m_pendingProps, m_heroes, 0);
        break;
    }
}

} // namespace battlecore
