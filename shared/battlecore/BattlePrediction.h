// =============================================================================
// BattlePrediction.h — 客户端预测 + 回滚和解 + 哈希校验/快照纠偏 (F1/F2/F3)
//
// 目标模型 (降级版守望先锋, 见 docs/frame-sync-client-framework.md §4):
//   - 服务器权威: 服务器跑 CBattleCore 权威模拟, 广播 FRAME_NTF{frame, inputs[]}。
//   - 客户端预测(F1): 本地 CBattleCore 立即消费本帧输入推进并渲染, 不等服务器帧。
//   - 回滚和解(F2): 本地按间隔存 checkpoint 快照; 收到服务器帧命令时 diff 本地预测,
//     不一致则回滚到最近一致快照 → 按服务器命令重放 → 再重放本地超前预测尾巴。
//   - 哈希校验(F3): 服务器定期广播 BATTLE_HASH_NTF{frame, hash}, 客户端 VerifyFrame
//     从最近 checkpoint 用权威输入重放到该帧算哈希比对, 检测 F2 抓不到的确定性漂移
//     (同样的输入却算出不同状态, 即跨平台定点漂移/内存破坏); 失配则回发快照请求,
//     服务器回 BATTLE_SNAPSHOT_NTF{frame, snapshot}, 客户端 ResyncFromSnapshot 全量纠偏。
//
// 关键正确性前提: CBattleCore 的 SerializeState/DeserializeState 必须完整恢复
//   m_rng 与 m_nextId (实体 id 分配器), 否则回滚重放会产生 id 漂移。m_nextId 已入
//   快照/哈希 (见 BattleCore.cpp)。
//
// 纯 C++ 头文件, 无网络/无引擎依赖; 帧命令/哈希/快照注入与网络解耦, 由上层(StageScene
// 或单测)把收到的 FRAME_NTF/HASH_NTF/SNAPSHOT_NTF 转成结构调对应方法。
// =============================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

#include "BattleCore.h"

namespace battlecore {

// 服务器帧命令 (BATTLE_FRAME_NTF 落地后的结构): 某逻辑帧的全员输入
struct ServerFrameCommand {
    uint32_t                  frame;
    std::vector<PlayerInput>  inputs;
};

// 输入列表是否相等 (按 player_id 排序后逐字段比较, 顺序无关)
inline bool PlayerInputLess(const PlayerInput& a, const PlayerInput& b) {
    return a.player_id < b.player_id;
}
inline bool InputsEqual(std::vector<PlayerInput> a, std::vector<PlayerInput> b) {
    if (a.size() != b.size()) return false;
    std::sort(a.begin(), a.end(), PlayerInputLess);
    std::sort(b.begin(), b.end(), PlayerInputLess);
    for (size_t i = 0; i < a.size(); ++i) {
        const PlayerInput& x = a[i];
        const PlayerInput& y = b[i];
        if (x.player_id != y.player_id || x.move_dir != y.move_dir ||
            x.shoot_dx != y.shoot_dx || x.shoot_dy != y.shoot_dy ||
            x.skill_id != y.skill_id || x.fire != y.fire ||
            x.gun_type != y.gun_type)
            return false;
    }
    return true;
}

class CPredictiveSim {
public:
    CPredictiveSim() {}

    bool Init(uint32_t seed, const std::vector<int64_t>& playerIds) {
        m_seed = seed;
        m_playerIds = playerIds;
        m_lastConfirmed = 0;
        m_predictedFrame = 0;
        m_rollbackCount = 0;
        m_rolledBack = false;
        m_hashMismatchCount = 0;
        m_hashMismatched = false;
        m_resyncCount = 0;
        m_checkpoints.clear();
        m_predInputs.clear();
        m_serverInputs.clear();
        return m_core.Init(seed, playerIds);
    }

    // F1: 本地立即消费一帧全员输入推进预测 (不等服务器)。inputs 含本地真实输入
    // 与对远端玩家输入的「预测/猜测」。
    void PredictFrame(uint32_t frame, const std::vector<PlayerInput>& inputs) {
        if (m_checkpoints.empty()) {
            // 初始 checkpoint: frame 0 (即 Tick(1) 之前的初始状态, 含外部注入的初始实体)
            m_checkpoints.push_back(Checkpoint{0, m_core.SerializeState()});
        }
        for (size_t i = 0; i < inputs.size(); ++i) m_core.SubmitInput(inputs[i]);
        m_core.Tick(frame);
        m_predictedFrame = frame;
        m_predInputs[frame] = inputs;
        if ((frame % m_checkpointEvery) == 0) {
            m_checkpoints.push_back(Checkpoint{frame, m_core.SerializeState()});
        }
    }

    // F2: 收到服务器帧命令 → 与本地预测 diff → 必要时回滚 + 重放。
    void OnServerFrame(const ServerFrameCommand& cmd) {
        m_rolledBack = false;
        m_serverInputs[cmd.frame] = cmd.inputs;
        if (cmd.frame <= m_lastConfirmed) return;              // 已确认, 忽略
        if (cmd.frame != m_lastConfirmed + 1) return;          // 有缺口, 等前面帧 (in-order 下不触发)

        std::map<uint32_t, std::vector<PlayerInput> >::const_iterator it = m_predInputs.find(cmd.frame);
        if (it != m_predInputs.end() && InputsEqual(it->second, cmd.inputs)) {
            m_lastConfirmed = cmd.frame;                       // 一致 → 确认
            return;
        }

        RollbackAndReplay(cmd.frame);
        m_rolledBack = true;
        ++m_rollbackCount;
        m_lastConfirmed = cmd.frame;
    }

    // F3: 校验「已确认帧 frame」的本地哈希 == 服务器权威哈希 (BATTLE_HASH_NTF 落地)。
    // 实现: 从最近一个 frame < 目标帧的 checkpoint 用权威输入重放到该帧算哈希比对, 再重放
    // 预测尾巴恢复本地预测状态 (对调用者透明)。frame 的权威输入须已在 m_serverInputs。
    // 返回: true=一致; false=失配 (已置 HashMismatched, 递增 HashMismatchCount)。
    bool VerifyFrame(uint32_t frame, uint64_t serverHash);

    // F3 辅助: 计算「已确认帧 frame」的本地权威哈希 (从最近 checkpoint 用权威输入重放到
    // frame, 再重放预测尾巴恢复, 对调用者透明)。用于客户端侧冒烟打点 / HASH_NTF 上报
    // (客户端↔服务器哈希比对在服务器 handle_hash 侧完成, 客户端只算自己这一侧)。
    // 返回: true=算出 (outHash 有效); false=权威输入有缺口 (outHash 未定义)。
    bool HashAtConfirmedFrame(uint32_t frame, uint64_t& outHash);

    // 观战/纯回放: 无预测, 直接按服务器权威帧推进核心 (不做 diff/回滚)。
    // 仅当 cmd.frame == m_lastConfirmed+1 时应用并返回 true (上层缓冲乱序帧)。
    // 用于观战者: 不产生本地预测, 核心状态完全由服务器帧流驱动 (快照对齐后逐帧推进)。
    bool ApplyAuthoritativeFrame(const ServerFrameCommand& cmd) {
        if (cmd.frame != m_lastConfirmed + 1) return false;
        m_serverInputs[cmd.frame] = cmd.inputs;
        for (size_t i = 0; i < cmd.inputs.size(); ++i) m_core.SubmitInput(cmd.inputs[i]);
        m_core.Tick(cmd.frame);
        m_lastConfirmed = cmd.frame;
        m_predictedFrame = cmd.frame;
        return true;
    }

    // F3: 收到服务器快照 (BATTLE_SNAPSHOT_NTF{frame, snapshot}) → 全量纠偏。
    // 本地核心恢复为 frame 结束后的权威状态, 丢弃过期 checkpoint/输入, 从 frame+1 重新预测。
    // 返回: true=恢复成功; false=快照反序列化失败 (保持原状态)。
    bool ResyncFromSnapshot(uint32_t frame, const std::string& snapshot);

    // 观测
    uint32_t LastConfirmedFrame() const { return m_lastConfirmed; }
    uint32_t PredictedFrame() const { return m_predictedFrame; }
    bool     RolledBack() const { return m_rolledBack; }       // 最近一次 OnServerFrame 是否回滚
    uint32_t RollbackCount() const { return m_rollbackCount; }
    bool     HashMismatched() const { return m_hashMismatched; }
    uint32_t HashMismatchCount() const { return m_hashMismatchCount; }
    uint32_t ResyncCount() const { return m_resyncCount; }
    uint64_t HashState() const { return m_core.HashState(); }
    const CBattleCore& Core() const { return m_core; }
    CBattleCore& MutableCore() { return m_core; }

private:
    struct Checkpoint { uint32_t frame; std::string snapshot; };

    // 找最近一个 frame < f 的 checkpoint 下标 (初始 checkpoint(0) 恒存在, 故必命中)
    int32_t FindCheckpointBefore(uint32_t f) const {
        for (int32_t i = (int32_t)m_checkpoints.size() - 1; i >= 0; --i)
            if (m_checkpoints[i].frame < f) return i;
        return -1;
    }

    // 用权威输入重放 from+1 .. to (须全部已确认; 遇缺口提前停)
    void ReplayServer(uint32_t from, uint32_t to) {
        for (uint32_t k = from + 1; k <= to; ++k) {
            std::map<uint32_t, std::vector<PlayerInput> >::const_iterator si = m_serverInputs.find(k);
            if (si == m_serverInputs.end()) return;
            for (size_t i = 0; i < si->second.size(); ++i) m_core.SubmitInput(si->second[i]);
            m_core.Tick(k);
        }
    }

    // 重放预测尾巴 from+1 .. to (已确认帧用权威输入, 未确认用本地预测输入)
    void ReplayTail(uint32_t from, uint32_t to) {
        for (uint32_t k = from + 1; k <= to; ++k) {
            std::vector<PlayerInput> inputs;
            std::map<uint32_t, std::vector<PlayerInput> >::const_iterator si = m_serverInputs.find(k);
            if (si != m_serverInputs.end()) inputs = si->second;
            else {
                std::map<uint32_t, std::vector<PlayerInput> >::const_iterator pi = m_predInputs.find(k);
                if (pi == m_predInputs.end()) break;
                inputs = pi->second;
            }
            for (size_t i = 0; i < inputs.size(); ++i) m_core.SubmitInput(inputs[i]);
            m_core.Tick(k);
        }
    }

    void RollbackAndReplay(uint32_t f) {
        // 找最近一个 frame < f 的 checkpoint (其状态 = 该帧结束后的状态, 严格 < f 以保证
        // 分歧帧 f 尚未应用)。
        int32_t ci = FindCheckpointBefore(f);
        if (ci < 0) return;   // 理论不可达 (初始 checkpoint 恒存在)

        m_core.DeserializeState(m_checkpoints[ci].snapshot);
        uint32_t from = m_checkpoints[ci].frame;

        // 丢弃已过期的 checkpoint (frame >= f 的都在分歧帧 f 及之后的旧预测路径上, 失效)。
        // 注意必须用 >= 而非 >: 分歧帧 f 恰为 checkpoint 边界 (f % checkpointEvery == 0) 时,
        // checkpoint{f} 是在错误预测路径上拍的快照, 回滚后即为陈旧; 若不弹掉, 后续
        // FindCheckpointBefore(f+1..f+checkpointEvery) 会命中陈旧快照 → 中间帧哈希/状态漂移。
        while (!m_checkpoints.empty() && m_checkpoints.back().frame >= f)
            m_checkpoints.pop_back();

        ReplayServer(from, f);              // 权威输入重放到 f
        ReplayTail(f, m_predictedFrame);    // 重放预测尾巴 f+1 .. P
    }

    CBattleCore m_core;
    uint32_t m_seed = 0;
    std::vector<int64_t> m_playerIds;
    uint32_t m_checkpointEvery = 10;   // 每 10 帧一个快照
    std::deque<Checkpoint> m_checkpoints;
    uint32_t m_lastConfirmed = 0;
    uint32_t m_predictedFrame = 0;
    uint32_t m_rollbackCount = 0;
    bool m_rolledBack = false;

    uint32_t m_hashMismatchCount = 0;   // F3: 哈希失配累计次数
    bool     m_hashMismatched = false;  // F3: 最近一次 VerifyFrame 是否失配
    uint32_t m_resyncCount = 0;         // F3: 快照纠偏累计次数

    std::map<uint32_t, std::vector<PlayerInput> > m_predInputs;    // frame → 本地预测全员输入
    std::map<uint32_t, std::vector<PlayerInput> > m_serverInputs;  // frame → 服务器权威全员输入
};

// ---------------------------------------------------------------------------

inline bool CPredictiveSim::VerifyFrame(uint32_t frame, uint64_t serverHash) {
    m_hashMismatched = false;
    int32_t ci = FindCheckpointBefore(frame);
    if (ci < 0) return false;   // 无可用 checkpoint (理论不可达: 初始 checkpoint(0) 恒存在)

    m_core.DeserializeState(m_checkpoints[ci].snapshot);
    uint32_t from = m_checkpoints[ci].frame;
    ReplayServer(from, frame);
    uint64_t local = m_core.HashState();
    m_hashMismatched = (local != serverHash);
    if (m_hashMismatched) ++m_hashMismatchCount;

    ReplayTail(frame, m_predictedFrame);   // 恢复预测状态 (对调用者透明)
    return !m_hashMismatched;
}

inline bool CPredictiveSim::HashAtConfirmedFrame(uint32_t frame, uint64_t& outHash) {
    int32_t ci = FindCheckpointBefore(frame);
    if (ci < 0) return false;

    m_core.DeserializeState(m_checkpoints[ci].snapshot);
    uint32_t from = m_checkpoints[ci].frame;
    ReplayServer(from, frame);             // 权威输入重放到 frame (遇缺口提前停)
    if (m_lastConfirmed < frame) {         // 权威输入有缺口 → 重放不足, 哈希不可信
        ReplayTail(frame, m_predictedFrame);
        return false;
    }
    outHash = m_core.HashState();
    ReplayTail(frame, m_predictedFrame);   // 恢复预测状态 (对调用者透明)
    return true;
}

inline bool CPredictiveSim::ResyncFromSnapshot(uint32_t frame, const std::string& snapshot) {
    if (!m_core.DeserializeState(snapshot)) return false;

    m_checkpoints.clear();
    m_checkpoints.push_back(Checkpoint{frame, snapshot});
    // 丢弃 frame 之后的过期输入 (会从 frame+1 重新预测/重新收帧)
    m_predInputs.erase(m_predInputs.upper_bound(frame), m_predInputs.end());
    m_serverInputs.erase(m_serverInputs.upper_bound(frame), m_serverInputs.end());
    m_lastConfirmed = frame;
    m_predictedFrame = frame;
    ++m_resyncCount;
    return true;
}

} // namespace battlecore
