// =============================================================================
// BattleGame.h — 战斗数据模块 (实体数据 + 配置 + 全部玩法规则)
//
// 与帧同步引擎 (BattleCore.h 的 CBattleCore) 严格分层:
//   - CBattleCore (帧同步模块) : 确定性 PRNG / 实体 id 分配 / 输入队列 / 快照引擎标量,
//     游戏稳定后不再变动。
//   - BattleGame (本模块)      : 实体容器 (hero/sp01/bullet/turret/prop) + 地图碰撞 +
//     波次/小怪/炮台配置 + 移动/开火/波次/AI/子弹/道具/炮台/技能等全部玩法规则,
//     会随游戏扩展新属性而持续变动。
//
// 边界约定:
//   - 本模块不 #include "BattleCore.h", 只依赖 BattleEntity.h/BattleInput.h/BattleRng.h/
//     BattleSerialize.h (纯数据 + 原语 + 确定性 PRNG)。
//   - PRNG (CBattleRng&) 与实体 id 分配器 (uint32_t&) 由引擎持有, 经参数传入 Step /
//     HurtSp01ById, 保证「引擎状态」与「游戏状态」物理隔离 (见 BattleCore.cpp)。
//   - 序列化只覆盖「本局可变状态」(实体 + collision + wave + game), 引擎标量
//     (magic/seed/rng/frame/nextId) 由 CBattleCore 写在本模块流之前。
// =============================================================================
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "BattleEntity.h"    // 实体数据模型 (HeroState/Sp01State/...)
#include "BattleInput.h"     // PlayerInput (单帧输入结构 + 方向量化)
#include "BattleRng.h"       // CBattleRng (确定性 PRNG)
#include "BattleSerialize.h" // BinWriter/BinReader/Hasher

namespace battlecore {

class BattleGame {
public:
    BattleGame() = default;
    ~BattleGame() = default;

    // 初始化: 清空全部状态 + 按玩家列表出生英雄 (id 1..N, 确定性)
    bool Init(const std::vector<int64_t>& playerIds);

    // 推进一个逻辑帧: 消费本帧输入 → 更新实体状态。frame 为逻辑帧号;
    // rng/nextId 由引擎持有经引用传入; inputs 消费后清空 (与旧 Tick 语义一致)。
    void Step(uint32_t frame, CBattleRng& rng, uint32_t& nextId, std::vector<PlayerInput>& inputs);

    // 全量快照序列化 (仅游戏状态; 引擎标量由 CBattleCore 写在本流之前/之后)
    void SerializeState(BinWriter& w) const;
    bool DeserializeState(BinReader& r);

    // 状态一致性哈希 (FNV-1a 64 位; 只混入游戏状态, 引擎标量由 CBattleCore 混入)
    void HashInto(Hasher& hasher) const;

    // 权威胜负判定: 0=未结束, -1=平局, >0=胜方玩家索引 (heroes 下标 + 1)
    int32_t CheckWin() const;

    // 设置某英雄初始枪械类别 (局外系统传入; 只改 gunType 不改 reportedGun)
    bool SetHeroGun(int64_t playerId, uint8_t gunType);

    // 子弹命中后调用 (P2 伤害入口; 触发 changeToHurt 的定点复刻)
    void HurtSp01ById(uint32_t id, uint8_t bulletClassId, CBattleRng& rng, uint32_t& nextId);

    // 访问器
    const std::vector<HeroState>&   GetHeroes()  const { return m_heroes; }
    const std::vector<Sp01State>&   GetSp01s()   const { return m_sp01s; }
    const std::vector<BulletState>& GetBullets() const { return m_bullets; }
    const std::vector<TurretState>& GetTurrets() const { return m_turrets; }
    const std::vector<PropState>&   GetProps()   const { return m_props; }
    const MapCollision& GetCollision() const { return m_collision; }
    const WaveState& GetWave() const { return m_wave; }
    const GameState& GetGame() const { return m_game; }

    // 波次编排注入 (本局常量, 不入快照)
    void SetWavePlan(const std::vector<WaveDef>& plan) { m_wavePlan = plan; }
    const std::vector<WaveDef>& GetWavePlan() const { return m_wavePlan; }

    // 小怪属性注入 (C5; 键 = Sp01ConfigKey(classId, sp01Id))
    void SetSp01Config(const std::map<uint32_t, Sp01Config>& cfg) { m_sp01Config = cfg; }
    const std::map<uint32_t, Sp01Config>& GetSp01Config() const { return m_sp01Config; }

    // 炮台注入 (S4; 出生点 + 属性, 键 = ptinfoId)
    void SetTurretSpawns(const std::vector<TurretSpawn>& spawns) { m_turretSpawns = spawns; }
    void SetTurretConfig(const std::map<int32_t, TurretConfig>& cfg) { m_turretConfig = cfg; }
    const std::vector<TurretSpawn>& GetTurretSpawns() const { return m_turretSpawns; }
    const std::map<int32_t, TurretConfig>& GetTurretConfig() const { return m_turretConfig; }

    // 可变访问器 (内部生成实体 / 测试填充用; 运行期状态修改统一在 Step 内进行)
    std::vector<HeroState>&   MutableHeroes()  { return m_heroes; }
    std::vector<Sp01State>&   MutableSp01s()   { return m_sp01s; }
    std::vector<BulletState>& MutableBullets() { return m_bullets; }
    std::vector<TurretState>& MutableTurrets() { return m_turrets; }
    std::vector<PropState>&   MutableProps()   { return m_props; }
    MapCollision& MutableCollision() { return m_collision; }
    WaveState& MutableWave() { return m_wave; }
    GameState& MutableGame() { return m_game; }

private:
    // 波次刷怪 + 清场判定 (每帧在敌人 AI 前调用)
    void StartWave(int32_t waveNum);
    void UpdateWave(CBattleRng& rng, uint32_t& nextId);

    // 炮台: 首帧幂等出生 (按 m_turretSpawns 文件序) + 波次激活 (pt_N → appear)
    void SpawnInitialTurrets(uint32_t& nextId);
    void ActivateTurret(int32_t nameIndex, int32_t delay);

private:
    std::vector<HeroState>   m_heroes;
    std::vector<Sp01State>   m_sp01s;
    std::vector<BulletState> m_bullets;
    std::vector<TurretState> m_turrets;
    std::vector<PropState>   m_props;

    MapCollision m_collision;   // 地图碰撞 (对象层矩形, 加载时导出)
    WaveState m_wave;           // 波次/出生管理状态
    GameState m_game;           // 本局经济/积分 (金币/钻石)

    std::vector<WaveDef> m_wavePlan;   // 波次编排 (配置注入, 本局常量, 不入快照)
    std::map<uint32_t, Sp01Config> m_sp01Config;  // 小怪属性 (键=Sp01ConfigKey; 配置注入, 不入快照)

    std::vector<TurretSpawn> m_turretSpawns;          // 炮台出生点 (配置注入, 不入快照)
    std::map<int32_t, TurretConfig> m_turretConfig;   // 炮台属性 (键=ptinfoId; 配置注入, 不入快照)
    bool m_turretsSpawned = false;                    // 炮台是否已首帧出生 (幂等标志)

    std::vector<Sp01State> m_pendingSpawns;    // 本帧生成的子实体 (teacher 出怪 / spTeacher 分裂)
    std::vector<PropState> m_pendingProps;     // 本帧掉落的道具 (金币/阳光)
};

} // namespace battlecore
