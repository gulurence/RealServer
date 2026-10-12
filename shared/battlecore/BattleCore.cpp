// =============================================================================
// BattleCore.cpp — 确定性帧同步引擎实现 (纯引擎: PRNG / id 分配 / 输入队列 / 快照引擎标量)
//
// 玩法 (移动/开火/波次/AI/子弹/道具/炮台/技能) 与实体状态已迁出到 BattleGame.cpp
// (战斗数据模块)。本文件只保留「引擎」职责: Init 复位引擎标量、SubmitInput 累积输入、
// Tick 委托 m_game.Step、快照/哈希负责引擎标量 (magic/seed/rng/frame/nextId) 并与
// m_game 的游戏状态流拼接。公开 API 与旧版完全一致。
// =============================================================================
#include "BattleCore.h"
#include "BattleSerialize.h"

#include <cstdint>
#include <string>
#include <vector>

namespace battlecore {

// 快照序列化魔数 (版本化: 每次改变序列化格式时递增, 防旧回放/快照误读)
static const uint32_t kMagic = 0x42435431;   // "BCT1" (v1: 定点全实体数据模型)

CBattleCore::CBattleCore() = default;
CBattleCore::~CBattleCore() = default;

// ---------------------------------------------------------------------------
// Init — 引擎标量复位 + 战斗数据模块初始化 (英雄出生点等)
// ---------------------------------------------------------------------------
bool CBattleCore::Init(uint32_t seed, const std::vector<int64_t>& playerIds) {
    m_seed  = seed;
    m_frame = 0;
    m_rng.Seed(seed);   // 复用 CBattleRng (splitmix64, 0 视为 1)
    m_nextId = 1000;
    m_pendingInputs.clear();
    return m_game.Init(playerIds);
}

// ---------------------------------------------------------------------------
// SetHeroGun — 初始化英雄枪械 (委托战斗数据模块)
// ---------------------------------------------------------------------------
bool CBattleCore::SetHeroGun(int64_t playerId, uint8_t gunType) {
    return m_game.SetHeroGun(playerId, gunType);
}

// ---------------------------------------------------------------------------
// SubmitInput — 累积本帧输入
// ---------------------------------------------------------------------------
void CBattleCore::SubmitInput(const PlayerInput& input) {
    m_pendingInputs.push_back(input);
}

// ---------------------------------------------------------------------------
// Tick — 推进一个逻辑帧 (引擎记帧号, 委托 m_game.Step 消费本帧输入)
// ---------------------------------------------------------------------------
void CBattleCore::Tick(uint32_t frame) {
    m_frame = frame;
    m_game.Step(frame, m_rng, m_nextId, m_pendingInputs);
}

// ---------------------------------------------------------------------------
// SerializeState / DeserializeState — 全量快照 (引擎标量 + 战斗数据模块游戏状态)
// ---------------------------------------------------------------------------
std::string CBattleCore::SerializeState() const {
    BinWriter w;
    w.u32(kMagic);
    w.u32(m_seed);
    w.u64(m_rng.State());   // PRNG 内部状态, 保证快照/回放完整忠实
    w.u32(m_frame);
    w.u32(m_nextId);        // 实体 id 分配器 (回滚后重放必须恢复, 否则 id 漂移)
    m_game.SerializeState(w);
    return w.out;
}

bool CBattleCore::DeserializeState(const std::string& data) {
    BinReader r(data);
    uint32_t magic = 0, seed = 0, frame = 0, nextId = 1000;
    uint64_t prngState = 0;
    if (!r.u32(magic) || magic != kMagic) return false;
    if (!r.u32(seed) || !r.u64(prngState) || !r.u32(frame) || !r.u32(nextId)) return false;

    m_seed = seed;
    m_rng.SetState(prngState);
    m_frame = frame;
    m_nextId = nextId;

    if (!m_game.DeserializeState(r)) return false;
    m_pendingInputs.clear();
    return true;
}

// ---------------------------------------------------------------------------
// HashState — 状态一致性哈希 (FNV-1a 64 位, 与 xProcessDefine::Fnv1a64 同源)
// 引擎标量 (seed/frame/nextId) 由本类混入, 游戏状态由 m_game.HashInto 混入, 字段
// 顺序与旧版 HashState 严格一致。
// ---------------------------------------------------------------------------
uint64_t CBattleCore::HashState() const {
    Hasher hasher;
    hasher.mix32(m_seed);
    hasher.mix32(m_frame);
    hasher.mix32(m_nextId);
    m_game.HashInto(hasher);
    return hasher.result();
}

// ---------------------------------------------------------------------------
// CheckWin — 权威胜负判定 (委托战斗数据模块)
// ---------------------------------------------------------------------------
int32_t CBattleCore::CheckWin() const {
    return m_game.CheckWin();
}

// ---------------------------------------------------------------------------
// HurtSp01ById — 子弹命中后调用 (P2 伤害入口; 委托战斗数据模块)
// ---------------------------------------------------------------------------
void CBattleCore::HurtSp01ById(uint32_t id, uint8_t bulletClassId) {
    m_game.HurtSp01ById(id, bulletClassId, m_rng, m_nextId);
}

} // namespace battlecore
