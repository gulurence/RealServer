/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: BattleEntity.h
      Purpose:   战斗实体纯数据模型

      定义 HeroState/Sp01State/BulletState/TurretState/PropState/MapCollision/WaveState/GameState 等纯 struct
      坐标/速度/角度用 Q16.16 定点 fx_t, 实体 id 按确定性顺序分配, 作为锁步对齐与序列化/哈希主键
  *****************************************************************/

// =============================================================================
// BattleEntity.h — 战斗实体纯数据模型 (客户端 / 服务器共享, 零引擎依赖)
//
// 目标 (§2.1): 把 Hero/Sp01/Sp01Boss/Bullet/WeaponManager/Prop/Turret 的逻辑状态
// 从 Cocos 实体里抽出来, 用 fx_t(定点 Q16.16)/int32_t/uint8_t 定义成纯 struct。
// 服务器 BattleActor 与客户端渲染层各自持有一份, tick 只改这里, 渲染只读这里。
//
// 类型约定 (与 FixedPoint.h 一致):
//   - 坐标/速度/角度/伤害系数 → fx_t (Q16.16, 见 FixedPoint.h)。
//   - 角度统一 fx_t 弧度 (Q16.16), 范围 [-π, π]。确定性 sin/cos/atan2 见 FixedTrig.h。
//     注: 设计文档 §1.2 原方案是 8192 份 BAM 角度 + 正弦表, 落地改为整数 CORDIC +
//     fx_t 弧度 (自包含、无大表、逐位确定), 故此处角度类型用 fx_t 而非 angle_t。
//   - HP/计数/帧计时 → int32_t (HP 统一 int, 见 §1.3)。
//   - 枚举 (动作/朝向/枪械/classId) → uint8_t 字段 + 本文件内 enum。
//
// 身份: 每个实体一个 uint32_t id, 由核心按「确定性顺序」分配 (同种子同 spawn 顺序),
// 两端 id 一致, 是锁步对齐与序列化/哈希的主键。
// =============================================================================
#pragma once

#include <cstdint>
#include <vector>

#include "FixedPoint.h"

namespace battlecore {

// ---------------------------------------------------------------------------
// 基础几何 / 碰撞盒
// ---------------------------------------------------------------------------

// 定点矩形 (碰撞盒 / 攻击盒 / 受击盒 / 地图碰撞), 坐标与尺寸均为 Q16.16。
// 语义对齐 Cocos CCRect (闭区间 containsPoint, 相切算相交的 intersectsRect), 保证
// 单机/联网与旧浮点路径逐帧一致 (R1 差分对拍)。
struct FxRect {
    fx_t x = 0;
    fx_t y = 0;
    fx_t w = 0;
    fx_t h = 0;

    fx_t MinX() const { return x; }
    fx_t MinY() const { return y; }
    fx_t MaxX() const { return (fx_t)((int64_t)x + w); }   // int64 防溢出
    fx_t MaxY() const { return (fx_t)((int64_t)y + h); }

    // CCRect::containsPoint (闭区间)
    bool Contains(fx_t px, fx_t py) const {
        return px >= x && px <= MaxX() && py >= y && py <= MaxY();
    }

    // CCRect::intersectsRect (相切算相交)
    bool Intersects(const FxRect& o) const {
        return !(MaxX() < o.x || o.MaxX() < x || MaxY() < o.y || o.MaxY() < y);
    }
};

// 定点坐标点 (出生点 / 入口点等)
struct FxPoint {
    fx_t x = 0;
    fx_t y = 0;
};

// ---------------------------------------------------------------------------
// 实体 class id (镜像客户端 ActionId.h 的 CLASS_ID_*, 核心内独立定义避免依赖 Cocos)
// ---------------------------------------------------------------------------
enum : uint8_t {
    kClassSp01Chef         = 0,   // 小怪 (厨师)
    kClassSp01Dama         = 1,   // 大妈
    kClassSp01Boy          = 2,   // 男孩
    kClassSp01Girl         = 3,   // 女孩
    kClassSp01Dog          = 4,   // 狗
    kClassSp01BoyMax       = 5,
    kClassSp01Police       = 6,
    kClassSp01PoliceMax    = 7,
    kClassSp01Worker       = 8,
    kClassSp01Zombie       = 9,
    kClassSp01ZombieMax    = 10,
    kClassSp01WorkerMax    = 11,
    kClassSp01BigGuard     = 12,  // 大怪·守卫
    kClassSp01BigTeacher   = 13,  // 大怪·老师
    kClassSp01BigTeacherChild = 14,
    kClassSp01BigSPTeacher = 15,  // 大怪·体育老师

    kClassTurret           = 16,  // 炮台

    kClassBulletNormal     = 20,
    kClassBulletHuopao     = 21,
    kClassBulletJuji       = 22,
    kClassBulletSandan     = 23,
    kClassBulletHuojiantong = 24,
    kClassBulletJiQiang    = 25,
    kClassBulletFullScreenBomb = 26,
    kClassBulletPaota      = 27,

    kClassPropCoins        = 40,
    kClassPropBomb         = 41,
    kClassPropSunShine     = 42,
    kClassPropJiaTeLin     = 43,
    kClassPropHuoJianTong  = 44,
    kClassPropBomb2        = 45,
    kClassPropGunGift14    = 46,
    kClassPropGunGift24    = 47,
    kClassPropGunGift34    = 48,
    kClassPropGunGift44    = 49,
    kClassPropGunGift54    = 50,
    kClassPropGunGift64    = 51,
};

// 掉落物品类型 (镜像 ActionId.h DROPGOODSTYPE_*, 值域与上面 class id 独立;
// 仅用于 PropState.classId 的第二取值域, 与 CLASS_ID_prop_* 在 Prop::init 里按值区分)
enum : uint8_t {
    kDropGoodsDamage      = 0,
    kDropGoodsDefense     = 1,
    kDropGoodsMoveSpeed   = 2,
    kDropGoodsHp          = 3,
    kDropGoodsHuoJianTong = 4,
    kDropGoodsJiaTeLin    = 5,
    kDropGoodsCoins       = 6,
};

// ---------------------------------------------------------------------------
// 动作状态枚举 (镜像 ActionId.h 的 ActionState_*, 值必须一一对应)
// ---------------------------------------------------------------------------
enum : uint8_t {
    // 英雄
    kHeroStand     = 0,
    kHeroWalk      = 1,
    kHeroStandFire = 2,
    kHeroWalkFire  = 3,
    kHeroHurt      = 4,
    kHeroDie       = 5,

    // 小怪 / 大怪 (Sp01)
    kSp01Appear        = 0,
    kSp01Walk          = 1,
    kSp01Attack        = 2,
    kSp01Hurt          = 3,
    kSp01Die           = 4,
    kSp01Disappear     = 5,
    kSp01BGuardAttack1 = 6,
    kSp01BGuardAttack2 = 7,
    kSp01BGuardEquip   = 8,
    kSp01BGuardEquipWalk = 9,
    kSp01BGuardEquipAttack = 10,
    kSp01BGuardUnload  = 11,
    kSp01BTeacherChange = 12,
    kSp01BTeacherTroops = 13,
    kSp01BTeacherWaitResume = 14,
    kSp01BTeacherResume = 15,
    kSp01BSpTeacherToEgg = 16,
    kSp01BSpTeacherEgg   = 17,
    kSp01BSpTeacherExitEgg = 18,
    kSp01BSpTeacherFenLie  = 19,
    kSp01Null           = 20,

    // 子弹
    kBulletMove        = 0,
    kBulletFire        = 1,
    kBulletDieHitWall  = 2,
    kBulletDieHitSp01  = 3,
    kBulletDieBaozha   = 4,
    kBulletHurtSp01    = 5,

    // 道具
    kPropInit       = 0,
    kPropWait       = 1,
    kPropMoveToHero = 2,
    kPropDisappear  = 3,
    kPropMove       = 4,
    kPropBaozha     = 5,

    // 炮台 (Turret 独立动作表, 见 Turret.h)
    kTurretInit       = 0,
    kTurretAppear     = 1,
    kTurretAttack     = 2,
    kTurretDie        = 3,
    kTurretStand      = 4,
    kTurretShow       = 5,
    kTurretHurt       = 6,
    kTurretDisappear  = 7,
};

// ---------------------------------------------------------------------------
// 移动方向编码 (与 BattleInput.h 的 8 方向一致):
//   0=停, 1=上, 2=右上, 3=右, 4=右下, 5=下, 6=左下, 7=左, 8=左上
// (坐标系: Y 向上, 0 弧度 = 右, 逆时针增加)
// ---------------------------------------------------------------------------
typedef uint8_t move_dir_t;

// 枪械类型 (镜像 ActionId.h GunType, 只取逻辑用到的几个主类别)
enum : uint8_t {
    kGunShouQiang  = 0,   // 手枪
    kGunJiQiang    = 1,   // 机枪
    kGunSanDan     = 2,   // 散弹
    kGunJuJi       = 3,   // 狙击
    kGunJiaTeLin   = 16,  // 加特林
    kGunHuoJianTong = 17, // 火箭筒
    kGunNone       = 255,
};

// ---------------------------------------------------------------------------
// 英雄状态 (合并 Hero + WeaponManager 的逻辑状态)
// ---------------------------------------------------------------------------
struct HeroState {
    uint32_t id        = 0;      // 核心内实体 id (确定性分配)
    int64_t  player_id = 0;      // 玩家 id (跨端对齐)

    fx_t     x = 0;              // 世界坐标 (Q16.16)
    fx_t     y = 0;
    fx_t     facing = 0;         // 朝向 (弧度, 0=右, 逆时针)
    fx_t     aimRadians = 0;     // 瞄准角 (开火方向)

    int32_t  hp = 100;           // 生命值 (统一 int, §1.3)
    int32_t  maxHp = 100;

    fx_t     walkSpeed = 0;      // 移动速度 px/frame (固定 20Hz 步长)
    fx_t     speedMul = kFxOne;  // 移速 buff 系数 (1.0 无, 1.3 = AND_MOVESPEED)
    uint8_t  action = kHeroStand;   // 动作状态 ActionState_Hero
    uint8_t  animDir = 3;        // 朝向的 8 方向 (0-7, 供渲染选动画, 渲染可再平滑)
    move_dir_t moveDir = 0;      // 本帧量化移动方向 0-8
    bool     alive = true;

    // 无敌 / 受击计数
    uint8_t  invincible = 0;     // 无敌剩余帧数 (0=无)
    int32_t  hasGetHurtNum = 0;
    int32_t  killCount = 0;      // 击杀数 (战斗结算按击杀排名用; 随快照/哈希序列化)

    // 死亡/复活 (合作玩法): deathCount 累计死亡次数 (req3 总死亡>10 用); respawnTimer
    // 复活倒计时累计 (帧, 队友站墓碑每帧 += 站立人数, 达 kRespawnFrames 复活)。墓碑位置
    // 即死亡瞬间 h.x/h.y (死亡后英雄不再移动)。均随快照/哈希序列化, 两端确定性一致。
    int32_t  deathCount   = 0;
    int32_t  respawnTimer = 0;

    // 全屏炸弹引信 (帧; >0 倒计时, 到 0 时以英雄当前坐标为中心结算半径 500 伤害;
    //  原 StageScene::doFullScreenBomb 的 0.5s 延迟 → 10 帧, 见 BattleCore::kFullScreenBombFuseFrames)
    int32_t  bombFuse = 0;

    // 枪械 (WeaponManager 迁入)
    uint8_t  gunType = kGunShouQiang;
    uint8_t  preGun = kGunShouQiang;
    uint8_t  reportedGun = kGunShouQiang;  // 客户端最近一次上报的枪械类别 (换枪「变化检测」, 入快照/哈希保证回滚确定性)
    int32_t  curHasShootNum = 0; // 当前弹匣已射弹数
    int32_t  numBulletShouldShoot = 0;   // 连发应射弹数 (加特林/机枪连射)
    int32_t  numBulletHasShoot = 0;      // 连发已射弹数
    fx_t     timeBulletInterval = 0;     // 射击间隔 (秒, 定点)
    uint32_t lastShootFrame = 0;         // 上次射击逻辑帧号
    bool     waitingShuttleInterval = false;
};

// ---------------------------------------------------------------------------
// 小怪 / 大怪状态 (Sp01)
// ---------------------------------------------------------------------------
struct Sp01State {
    uint32_t id = 0;             // 实体 id (确定性分配)
    uint8_t  classId = 0;        // kClassSp01* / kClassTurret
    int32_t  idInSp01Info = 0;   // 配置表 (Sp01Info) 中的行号

    fx_t     x = 0;
    fx_t     y = 0;
    fx_t     radians = 0;        // 朝向 (弧度); sin/cos 用 FixedTrig 现算, 不缓存

    int32_t  hp = 100;
    int32_t  maxHp = 100;
    int32_t  hpPre = 100;        // 上次血量 (炮台/掉落判定用)

    uint8_t  action = kSp01Appear;
    uint8_t  animDir = 0;        // 8 方向 (0-7, 供渲染)
    bool     remove = false;     // 是否待移除

    // 移动
    fx_t     walkSpeed = 0;      // 速度 px/frame
    fx_t     moveBackSpeed = 0;  // 被散弹击退速度
    fx_t     moveBackRadians = 0;
    int32_t  moveBackAllCount = 0;   // 击退剩余帧数
    bool     moveBackFromGun = false;
    bool     stopMoveTemp = false;   // 临时停止移动

    // AI / 计时 (帧计)
    int32_t  sleepWalkingCount = 0;   // 睡眠/游走计时
    int32_t  variationCount = 0;      // 体育老师分裂等级
    int32_t  residueTimeOrCount = 0;  // 通用计时器 (老师/守卫; 既当秒数也当个数, 见移植注释)
    int32_t  troopsTiming = 0;        // 出怪间隔 (帧, 原 1.0s → 20 帧)
    int32_t  actionElapsedFrames = 0; // 当前动作已持续帧数 (动画完成 animationDone 触发条件)
    int32_t  sp01PauseCount = 0;      // 小怪停顿计时
    int32_t  stopTimeCount = 0;       // 停止计时
    bool     stopFindHeroUntilGetNextPoint = false;
    bool     isFindHeroInThisFPS = false;
    bool     andRadiansOrMinus = false;   // 游走角度加/减方向 (spawn 时 random(2) 决定)

    // 属性
    fx_t     hurtToHero = 0;     // 对英雄的单次伤害 (定点)
    int32_t  dropCoinsProbability = 0;
    int32_t  dropCoinsMax = 0;
    int32_t  dropCoinsMin = 0;
    int32_t  dropHuoJianTongPro = 0;
    int32_t  dropJiaTeLinPro = 0;
    int32_t  dropSunShinePro = 0;
    int32_t  dropSunShineMax = 0;
    int32_t  dropSunShineMin = 0;
    int32_t  andLight = 0;

    // 寻路 (走固定 nextPoint 路径)
    fx_t     nextX = 0;
    fx_t     nextY = 0;
    bool     hasNextPoint = false;
};

// ---------------------------------------------------------------------------
// 小怪配置属性 (C5: 由配置层 Sp01Info 解析注入; 内核 spawn 时按 idInSp01Info 查表)
//
// 只携带内核实例化需要的数值 (classId 已由波次编排解析携带, 不再重复); 游戏框架在
// Init 后调用 CBattleCore::SetSp01Config 一次性注入, 与 m_wavePlan 同为「本局常量」,
// 不入快照 (解析后的 hp/walkSpeed/hurtToHero 已随每个 Sp01State 序列化)。
// ---------------------------------------------------------------------------
struct Sp01Config {
    int32_t maxHp = 0;        // 最大生命 (HP 整数; 内核乘 kHpScale 进内部定点)
    fx_t    walkSpeed = 0;    // 走速 (px/帧, Q16.16)
    fx_t    hurtToHero = 0;   // 对英雄单次碰撞伤害 (Q16.16; 配置层 FxFromFloat(hp) 转来)
};

// Sp01Config 表键 = (classId, sp01Id) 组合。enemyInfo.plist 的 N (sp01Id) 只在同 name 内唯一
// (boy_511 与 girl_511 并存), 单用 sp01Id 会互相覆盖, 故拼 classId 高 16 位、sp01Id 低 16 位
// (classId<16、sp01Id<65536, 无歧义)。配置层与内核 spawn 共用此键。
inline uint32_t Sp01ConfigKey(uint8_t classId, int32_t sp01Id) {
    return ((uint32_t)classId << 16) | (uint32_t)sp01Id;
}

// ---------------------------------------------------------------------------
// 炮台配置 (Turret, 由 enemyInfo.plist 的 ptinfo_N 解析注入; 本局常量, 不入快照)
//
// 只带内核实例化炮台需要的数值; 游戏框架在 Init 后调用 CBattleCore::SetTurretConfig 一次性注入,
// 与 m_sp01Config 同为「本局常量」, 解析后的 hp/hurtToHero/bulletSpeed/hurtHeroDis 随各
// TurretState 序列化。炮台无难度加成 (客户端 setTurretInfo 只读 raw 值)。
// ---------------------------------------------------------------------------
struct TurretConfig {
    int32_t maxHp = 0;         // 最大生命 (HP 整数; 内核乘 kHpScale 进内部定点)
    fx_t    hurtToHero = 0;    // 炮弹对英雄伤害 (Q16.16)
    fx_t    bulletSpeed = 0;   // 炮弹速度 px/帧 @20Hz (配置层 FxFromFloat(moveSpeed×3))
    int32_t hurtHeroDis = 0;   // 攻击英雄判定距离 (像素, 原 attackdis / m_nHurtHeroDis)
};

// 炮台出生点 (TMX paota_point 对象层解析注入; 本局常量, 不入快照)。
// nameIndex = 对象名 pt_N 的 N (波次 pt_N 激活同名炮台); x/y 为客户端翻转后坐标 (fx_t);
// ptinfoId = 属性 ptinfo_N 的 N (查 TurretConfig 表)。
// ---------------------------------------------------------------------------
struct TurretSpawn {
    int32_t nameIndex = 0;
    fx_t    x = 0;
    fx_t    y = 0;
    int32_t ptinfoId = 0;
};

// ---------------------------------------------------------------------------
// 子弹状态 (Bullet)
// ---------------------------------------------------------------------------
struct BulletState {
    uint32_t id = 0;
    uint8_t  classId = kClassBulletNormal;
    uint8_t  action = kBulletMove;

    fx_t     x = 0;
    fx_t     y = 0;
    fx_t     rotate = 0;         // 飞行方向 (弧度); sin/cos 现算
    fx_t     speed = 0;          // 速度 px/frame
    fx_t     hurt = 0;           // 对怪伤害

    int32_t  lifeLength = 0;     // 存活帧数
    int32_t  sleepTime = 0;      // 延迟起飞 (散弹 / 狙击)
    bool     remove = false;

    // 狙击穿透: 已命中的目标 id 列表 (避免重复伤害)
    std::vector<uint32_t> hasHurtIds;

    // 火箭筒追踪目标 (homing)
    fx_t     targetX = 0;
    fx_t     targetY = 0;
    bool     hasTarget = false;

    int64_t  ownerPlayerId = 0;  // 归属玩家 (击杀归属/结算用; 反怪弹/火箭筒/炮台弹由开火者填入)
};

// ---------------------------------------------------------------------------
// 炮台状态 (Turret, 继承 Sp01 属性但独立动作表)
// ---------------------------------------------------------------------------
struct TurretState {
    uint32_t id = 0;
    int32_t  nameIndex = 0;      // 出生点名序号 (pt_N 的 N; 波次激活匹配用; 手动注入的炮台为 -1)
    fx_t     x = 0;
    fx_t     y = 0;
    fx_t     aim = 0;            // 瞄准角 (弧度)
    int32_t  hp = 100;
    int32_t  maxHp = 100;
    uint8_t  action = kTurretInit;
    int32_t  hurtHeroDis = 0;    // 攻击英雄的判定距离 (像素, 原 m_nHurtHeroDis 与 ccpDistance 比较)
    int32_t  delay = 0;          // 激活延迟帧数
    bool     active = false;     // 是否已激活
    bool     remove = false;

    // 炮弹 (原 m_fHurtToHero / m_fWalkSpeed → createAPaoTaB)
    fx_t     hurtToHero = 0;         // 炮弹对英雄伤害 (定点)
    fx_t     bulletSpeed = 0;        // 炮弹速度 px/帧
    int32_t  actionElapsedFrames = 0; // 当前动作已持续帧数 (animationDone)

    // 掉落配置 (同 Sp01, 死亡 randomCreateProp 用)
    int32_t  dropCoinsProbability = 0;
    int32_t  dropCoinsMax = 0;
    int32_t  dropCoinsMin = 0;
    int32_t  dropSunShinePro = 0;
    int32_t  dropSunShineMax = 0;
    int32_t  dropSunShineMin = 0;
};

// ---------------------------------------------------------------------------
// 道具状态 (Prop)
// ---------------------------------------------------------------------------
struct PropState {
    uint32_t id = 0;
    uint8_t  classId = kClassPropCoins;
    uint8_t  action = kPropInit;

    fx_t     x = 0;
    fx_t     y = 0;
    fx_t     endX = 0;           // 终点 (吸附/弹跳目标)
    fx_t     endY = 0;

    int32_t  value = 0;          // 数值 (金币/时长等)
    int32_t  velocity = 0;       // 速度 (炸弹类, px/帧)
    int32_t  acceleration = 0;   // 加速度 (炸弹类, px/帧²)
    int32_t  delay = 0;          // 延迟帧数 (掉落物品 posui 表演)
    int32_t  life = 0;           // wait 存活帧数 (0 表示无限/由类型决定)
    int32_t  fade = 0;           // disappear 淡出帧数 (淡出期间仍可被拾取)
    bool     remove = false;
};

// ---------------------------------------------------------------------------
// 地图碰撞 (TMX 对象层 "collide_people"/"collide_bullet" 的矩形列表, 加载时导出)
//
// 实际碰撞数据是对象层的轴对齐矩形 (CCRect), 不是 tile 网格 —— isPointInCollides_People/
// isPointInCollides_Bullet 就是「点是否落在任一矩形内」的遍历。核心据此存矩形列表,
// tick 内做点/矩形查询, 语义与旧路径逐帧一致。
// ---------------------------------------------------------------------------
struct MapCollision {
    int32_t mapWidth = 0;    // 地图像素宽 (TMX contentSize.width)
    int32_t mapHeight = 0;   // 地图像素高 (TMX contentSize.height)

    std::vector<FxRect> people;   // collide_people 对象层矩形 (人/怪挡墙)
    std::vector<FxRect> bullet;   // collide_bullet 对象层矩形 (子弹挡墙)
    std::vector<FxPoint> entries; // 出生点 (TMX 入口对象, 1-based, 波次刷怪用)

    bool empty() const { return mapWidth <= 0 || mapHeight <= 0; }

    // 定点坐标是否落在任一碰撞矩形内
    bool ContainsPeople(fx_t px, fx_t py) const {
        for (const auto& r : people) if (r.Contains(px, py)) return true;
        return false;
    }
    bool ContainsBullet(fx_t px, fx_t py) const {
        for (const auto& r : bullet) if (r.Contains(px, py)) return true;
        return false;
    }

    // 定点矩形是否与任一碰撞矩形相交 (攻击盒/受击盒命中判定)
    bool IntersectsPeople(const FxRect& q) const {
        for (const auto& r : people) if (r.Intersects(q)) return true;
        return false;
    }
    bool IntersectsBullet(const FxRect& q) const {
        for (const auto& r : bullet) if (r.Intersects(q)) return true;
        return false;
    }

    // 地图边界 (0 < x < mapW, 0 < y < mapH, 严格开区间; 对齐 IOBuiledLay::isPointInMap)
    bool InMap(fx_t px, fx_t py) const {
        return px > 0 && px < FxFromInt(mapWidth) && py > 0 && py < FxFromInt(mapHeight);
    }
};

// ---------------------------------------------------------------------------
// 碰撞盒静态表 (加载时从动画数据抽取, tick 内查表; 见 §2.3「碰撞盒逻辑化」)
//
// 键 = (实体类型, 动作状态)。每个动作对应一个逻辑盒 (FxRect, 实体局部坐标)。
// 攻击盒 (red) / 受击盒 (green) 分别成表。枪口/命中点由坐标 + 方向单位向量推算。
// 表项在加载时填好 (渲染层从动画 clip 抽取), 运行期只读。
// ---------------------------------------------------------------------------
struct BodyBoxTable {
    // 局部碰撞盒, 按 [classId][action] 索引 (缺省项尺寸 0 = 不可命中/不可被命中)
    std::vector<FxRect> boxes;
    int32_t             classCount = 0;   // 第一维 (classId 类别数)
    int32_t             actionCount = 0;  // 第二维 (动作状态数)

    void resize(int32_t classes, int32_t actions) {
        classCount = classes;
        actionCount = actions;
        boxes.assign((size_t)classes * (size_t)actions, FxRect());
    }
    FxRect& at(int32_t cls, int32_t action) {
        return boxes[(size_t)cls * (size_t)actionCount + (size_t)action];
    }
    const FxRect& at(int32_t cls, int32_t action) const {
        return boxes[(size_t)cls * (size_t)actionCount + (size_t)action];
    }
};

// ---------------------------------------------------------------------------
// 本局经济/积分状态 (原 UISetLayerb::alterCoinsNum/alterZuanShiNum, 单局全局)
// ---------------------------------------------------------------------------
struct GameState {
    int32_t coins = 0;     // 本局金币
    int32_t diamond = 0;   // 本局钻石
};

// ---------------------------------------------------------------------------
// 波次出生编排 (配置注入, tick 内零配置读)
//
// 由配置层 (WaveInfo/WaveSpawn 表) 在加载期解析而来, 通过 CBattleCore::SetWavePlan
// 一次性注入。内核 tick 内只消费这里已解析好的 classId/数量/间隔/出生点, 不再碰配置。
// 后续 C5 打通时, 配置表用 sp01Id 引用 Sp01Info, 游戏框架解析成 classId 再注入内核。
// ---------------------------------------------------------------------------
struct WaveSpawnGroup {
    uint8_t  classId = 0;    // 敌人类型 kClassSp01*; 哨兵 kClassTurret(16)=炮台激活 (见下)
    int32_t  sp01Id = 0;     // Sp01Info 配置行号 (C5: spawn 时按此查 Sp01Config; 0=回退默认)
                             //   classId==kClassTurret 时为炮台名序号 N (pt_N)
    int32_t  count = 0;      // 数量 (炮台激活恒为 1, 使 spawn 事件恰在 startTime 触发一次)
    int32_t  interval = 0;   // 出生间隔 (帧; 0 = 一次性全刷); 炮台激活时复用为 appear 延迟 (帧)
    int32_t  entryPoint = 1; // 出生点序号 (1-based, 对应 MapCollision::entries); 炮台激活不用
    int32_t  startTime = 0;  // 本波开始后第几帧开始刷 (炮台激活 = 激活事件帧)
};

struct WaveDef {
    bool isKey = true;                  // 是否计入显示波次 (原 plist isKey)
    std::vector<WaveSpawnGroup> groups; // 本波出生组
};

// ---------------------------------------------------------------------------
// 波次 / 出生管理状态 (Sp01Boss 迁入; 运行时可变部分, 入快照/哈希)
//
// 出生进度由 waveTimer 唯一推导 (每个出生组「已出生数量」是 (waveTimer, startTime,
// interval, count) 的纯函数), 因此快照只需存 waveNum/waveTimer 就能完整恢复刷怪进度,
// 天然回滚安全 (无需逐组存剩余数)。
// ---------------------------------------------------------------------------
struct WaveState {
    int32_t waveNum = 0;            // 当前波次 (1-based; 0 = 未开始)
    int32_t xianShiWaveNum = 0;     // 显示波次 (isKey 波次累计)
    fx_t    gamePauseTime = 0;      // 波间暂停计时 (帧)
    int32_t waveTimer = 0;          // 本波已进行帧数 (spawn 进度唯一来源)
    bool    allWavesCleared = false; // 全部波次已清空 (胜利标志)
};

} // namespace battlecore
