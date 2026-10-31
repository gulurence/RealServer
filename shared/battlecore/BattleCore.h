/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: BattleCore.h
      Purpose:   确定性帧同步引擎接口

      CBattleCore 组合持有 BattleGame, 定义确定性 PRNG、实体 id 分配器、输入队列、快照/哈希与胜负判定
      公开 API 与旧版一致, 保证 SWIG/Lua/客户端/预测/测试零接口漂移, 提供 FrameSyncCore 别名
  *****************************************************************/

// =============================================================================
// BattleCore.h — 确定性帧同步引擎 (客户端 / 服务器 共享同一份代码)
//
// 严格分层 (二模块拆分):
//   - CBattleCore (本模块, 帧同步引擎) : 确定性 PRNG / 实体 id 分配器 / 输入队列 /
//     快照引擎标量 (magic/seed/rng/frame/nextId) / 状态哈希 / 胜负判定 —— 游戏稳定后
//     不再变动。
//   - BattleGame (战斗数据模块)        : 实体数据 (hero/sp01/bullet/turret/prop) +
//     配置 (波次/小怪/炮台) + 全部玩法规则 (移动/开火/波次/AI/子弹/道具/炮台/技能),
//     会随游戏扩展新属性而持续变动 (见 BattleGame.h)。
//
// 本类只保留「引擎」职责, 全部玩法状态与规则委托给成员 m_game (BattleGame)。公开 API
// 与旧版 CBattleCore 完全一致, 保证 SWIG / Lua / 客户端 / 预测 / 测试零接口漂移。
//
// 设计目标 (§7.6):
//   - 纯 C++、无引擎依赖 (无 CCNode/无服务器头文件), 客户端与服务器各自编译同一批源文件。
//   - 固定步长 tick + 确定性 PRNG + 整数定点坐标 (Q16.16, 避免浮点跨平台漂移) 保证两端模拟一致。
//   - 全量状态快照序列化 / 恢复 (checkpoint、断线重连、战斗回放共用)。
//   - 状态哈希一致性校验 (比对客户端上报的 PBBattleHashNtf)。
//   - 权威胜负判定。
//
// 类型约定: 共享核心不依赖 engine 的 int32/int64 别名, 直接用 <cstdint> 固定宽度
// 类型 (int32_t/int64_t/uint32_t/uint64_t), 保证跨平台/跨工程一致。
// =============================================================================
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "BattleEntity.h"  // 实体数据模型 (HeroState/Sp01State/...)
#include "BattleInput.h"   // PlayerInput (单帧输入结构 + 方向量化)
#include "BattleRng.h"     // CBattleRng (确定性 PRNG)
#include "BattleGame.h"    // 战斗数据模块 (实体 + 玩法规则, 本类组合持有)

namespace battlecore {

// =============================================================================
// CBattleCore — 确定性帧同步引擎
// =============================================================================
class CBattleCore {
public:
    CBattleCore();
    ~CBattleCore();

    // 初始化: 注入种子 + 玩家列表, 初始化确定性 PRNG 与英雄出生点
    bool Init(uint32_t seed, const std::vector<int64_t>& playerIds);

    // 投递某玩家一帧输入 (按帧累积, Tick 时统一生效)
    void SubmitInput(const PlayerInput& input);

    // 推进一个逻辑帧: 消费本帧输入 → 更新实体状态 → 帧号 +1
    void Tick(uint32_t frame);

    // 设置某英雄的初始枪械类别 (局外系统传入的玩家主枪; 在 Init 后、首帧 Tick 前调用)。
    // 只改 gunType, 不改 reportedGun (仍为默认手枪) —— 首帧输入 gun_type 经变化检测
    // 在两端施加同一结果, 避免初始主枪被陈旧输入误复位, 保证哈希一致。
    bool SetHeroGun(int64_t playerId, uint8_t gunType);

    // 全量状态快照序列化 / 恢复 (checkpoint / 断线重连 / 回放共用)
    std::string SerializeState() const;
    bool DeserializeState(const std::string& data);

    // 状态一致性哈希 (FNV-1a 64 位)
    uint64_t HashState() const;

    // 权威胜负判定: 0=未结束, -1=平局, >0=胜方玩家索引 (heroes 下标 + 1)
    int32_t CheckWin() const;

    // P2 伤害入口: 子弹命中后调用 (伤害值/暴击率等为占位配置, C5 从 bin 读)
    // 内部触发 changeToHurt (HP 阈值 / boss 阶段转移 / 死亡掉落) 的定点复刻。
    void HurtSp01ById(uint32_t id, uint8_t bulletClassId);

    // 访问器
    uint32_t GetSeed() const { return m_seed; }
    uint32_t GetFrame() const { return m_frame; }
    const std::vector<HeroState>&   GetHeroes()  const { return m_game.GetHeroes(); }
    const std::vector<Sp01State>&   GetSp01s()   const { return m_game.GetSp01s(); }
    const std::vector<BulletState>& GetBullets() const { return m_game.GetBullets(); }
    const std::vector<TurretState>& GetTurrets() const { return m_game.GetTurrets(); }
    const std::vector<PropState>&   GetProps()   const { return m_game.GetProps(); }
    const MapCollision& GetCollision() const { return m_game.GetCollision(); }
    const WaveState& GetWave() const { return m_game.GetWave(); }
    const GameState& GetGame() const { return m_game.GetGame(); }

    // 波次编排注入: 配置层在 Init 后一次性设置 (本局常量, 不入快照; 两端需注入同一份)
    void SetWavePlan(const std::vector<WaveDef>& plan) { m_game.SetWavePlan(plan); }
    const std::vector<WaveDef>& GetWavePlan() const { return m_game.GetWavePlan(); }

    // 小怪属性注入 (C5): 配置层解析 Sp01Info 后一次性设置, spawn 按 Sp01ConfigKey(classId, sp01Id) 查表
    // (本局常量, 不入快照; 解析后的 hp/walkSpeed/hurtToHero 已随各 Sp01State 序列化)
    void SetSp01Config(const std::map<uint32_t, Sp01Config>& cfg) { m_game.SetSp01Config(cfg); }
    const std::map<uint32_t, Sp01Config>& GetSp01Config() const { return m_game.GetSp01Config(); }

    // 炮台注入 (S4): 出生点 + 属性一次性设置, Tick 首帧幂等 spawn 全部炮台 (kTurretInit 待波次激活)
    // (本局常量, 不入快照; 解析后的属性已随各 TurretState 序列化)。键 = ptinfoId (0..5)。
    void SetTurretSpawns(const std::vector<TurretSpawn>& spawns) { m_game.SetTurretSpawns(spawns); }
    void SetTurretConfig(const std::map<int32_t, TurretConfig>& cfg) { m_game.SetTurretConfig(cfg); }
    const std::vector<TurretSpawn>& GetTurretSpawns() const { return m_game.GetTurretSpawns(); }
    const std::map<int32_t, TurretConfig>& GetTurretConfig() const { return m_game.GetTurretConfig(); }

    // 可变访问器 (P2 移植内部生成实体 / 测试填充用; 运行期状态修改统一在 Tick 内进行)
    std::vector<HeroState>&   MutableHeroes()  { return m_game.MutableHeroes(); }
    std::vector<Sp01State>&   MutableSp01s()   { return m_game.MutableSp01s(); }
    std::vector<BulletState>& MutableBullets() { return m_game.MutableBullets(); }
    std::vector<TurretState>& MutableTurrets() { return m_game.MutableTurrets(); }
    std::vector<PropState>&   MutableProps()   { return m_game.MutableProps(); }
    MapCollision& MutableCollision() { return m_game.MutableCollision(); }
    WaveState& MutableWave() { return m_game.MutableWave(); }
    GameState& MutableGame() { return m_game.MutableGame(); }

private:
    uint32_t m_seed  = 0;
    uint32_t m_frame = 0;
    CBattleRng m_rng;   // 确定性 PRNG (splitmix64, 复用 BattleRng.h)

    uint32_t m_nextId = 1000;                  // 实体 id 分配器 (确定性递增; 子实体生成用)
    std::vector<PlayerInput> m_pendingInputs;   // 本帧待处理输入

    BattleGame m_game;   // 战斗数据模块 (实体容器 + 配置 + 全部玩法规则)
};

// 语义别名: 帧同步模块对外亦可用 FrameSyncCore 指代 (与战斗数据模块 BattleGame 对照)。
using FrameSyncCore = CBattleCore;

} // namespace battlecore
