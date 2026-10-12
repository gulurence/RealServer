// =============================================================================
// BattlePacing.h — 抖动缓冲 + 时间膨胀 + 超前量控制 (F4)
//
// 目标 (docs/frame-sync-client-framework.md §4.3 F4):
//   网络抖动时, 客户端既不「缓冲空了硬卡」(现状 A3 空转), 也不「积压无限追不上」,
//   而是用「时间膨胀 (time dilation)」平滑追帧 —— 缓冲深就快放(多推进逻辑帧),
//   缓冲浅就慢放(少推进逻辑帧)。这是守望 §2.6「时间膨胀」的降级落地。
//
// 定位: 这是「节奏控制」层, 不是「模拟」层。每个逻辑帧 Tick(frame) 仍是确定性的一帧,
//   本模块只决定「本渲染帧推进多少个逻辑帧」(0=慢放 / 1=正常 / 2=快放)。
//
// 关键量 (与 BattlePrediction.h 的 CPredictiveSim 对齐):
//   lead  = 预测帧 - 最后确认帧  (客户端超前服务器的逻辑帧数 = run-ahead)
//   depth = 抖动缓冲深度         (已收到但尚未消费的服务器帧数)
//   targetLead = 0.5*RTT + 安全边际 (守望 §2.3, 折算成帧, 见 ComputeTargetLead)
//
// 纯 C++ 头文件, 无网络/无引擎依赖; 时间由调用方注入 nowMs (墙钟毫秒), 保证单测确定性。
// 与 StageScene 渲染循环的接线属 P3 (客户端集成里程碑), 这里只交付可单测的节奏内核。
// =============================================================================
#pragma once

#include <cstdint>
#include <deque>

namespace battlecore {

// ---------------------------------------------------------------------------
// 指数加权滑动平均 (整数毫秒; 新样本权重 1/8, 旧值 7/8)
// ---------------------------------------------------------------------------
class CEwma {
public:
    void Reset() { m_has = false; m_value = 0; }
    void Add(uint64_t v) {
        m_value = m_has ? (m_value * 7 + v) / 8 : v;
        m_has = true;
    }
    bool     Has()   const { return m_has; }
    uint64_t Value() const { return m_value; }

private:
    bool     m_has   = false;
    uint64_t m_value = 0;
};

// ---------------------------------------------------------------------------
// 由 RTT 折算超前目标帧数 (守望 §2.3: 超前约半个 RTT + 安全边际)。
// rttMs: 往返时延; safetyFrames: 安全边际(帧); frameMs: 每逻辑帧毫秒(kFixedDeltaMs=50)。
// 半个 RTT 向上取整到整帧。
// ---------------------------------------------------------------------------
inline uint32_t ComputeTargetLead(uint64_t rttMs, uint32_t safetyFrames, uint32_t frameMs) {
    if (frameMs == 0) return safetyFrames;
    const uint32_t halfRttFrames = (uint32_t)((rttMs / 2 + frameMs - 1) / frameMs);
    return halfRttFrames + safetyFrames;
}

// ---------------------------------------------------------------------------
// 抖动缓冲 + 时间膨胀控制器 (F4)
//
// 使用契约 (由渲染层驱动):
//   1. 每收到一个服务器帧 → OnServerFrame(frame, nowMs) 入缓冲。
//   2. 每渲染帧 → n = FramesToTick() 得到本渲染帧应推进的逻辑帧数 (0/1/2);
//      随后调用 n 次 ConsumeFrame(), 每次对应「真正推进了一个逻辑帧」。
//   3. 可选: AddRttSample(rttMs) 喂 RTT, 供 ComputeTargetLead 折算超前目标。
// ---------------------------------------------------------------------------
class CBattlePacer {
public:
    struct Config {
        uint32_t targetDepth = 3;   // 理想缓冲深度 (帧)
        uint32_t lowWater   = 1;    // depth <= lowWater  → 慢放(0 帧/渲染帧)
        uint32_t highWater  = 5;    // depth >= highWater → 快放(2 帧/渲染帧)
        uint32_t capacity   = 32;   // 缓冲上限; 满则丢弃最新帧并计数溢出
    };

    CBattlePacer() { Reset(Config()); }

    void Reset(const Config& cfg) {
        m_cfg = cfg;
        m_frames.clear();
        m_maxDepth = 0;
        m_overflowCount = 0;
        m_stallCount = 0;
        m_lastArrivalMs = 0;
        m_arrivalGap.Reset();
        m_rtt.Reset();
    }

    // 服务器帧到达 → 入抖动缓冲 (frame 单调递增; 乱序/重复由上层先过滤)。
    // nowMs 为到达墙钟时间(注入), 用于到达间隔抖动观测。
    void OnServerFrame(uint32_t frame, uint64_t nowMs) {
        if (!m_frames.empty() && nowMs >= m_lastArrivalMs)
            m_arrivalGap.Add(nowMs - m_lastArrivalMs);   // 到达间隔抖动
        m_lastArrivalMs = nowMs;

        if (m_frames.size() >= m_cfg.capacity) {
            ++m_overflowCount;   // 缓冲满 → 丢弃(实际应由上层触发 F3 快照纠偏)
            return;
        }
        m_frames.push_back(frame);
        if (m_frames.size() > m_maxDepth) m_maxDepth = (uint32_t)m_frames.size();
    }

    // 每渲染帧: 返回本渲染帧应推进的逻辑帧数 (时间膨胀 0/1/2)。深度越浅越慢放。
    // 返回 0 计一次「慢放」(stall), 供观测网络欠载。
    uint32_t FramesToTick() {
        const uint32_t depth = Depth();
        if (depth >= m_cfg.highWater) return 2;            // 泄洪: 快放
        if (depth <= m_cfg.lowWater) { ++m_stallCount; return 0; }  // 积攒: 慢放
        return 1;                                          // 正常
    }

    // 消费一帧 (调用方真正推进了一个逻辑帧后调用, 取走缓冲头最老帧)。
    void ConsumeFrame() {
        if (!m_frames.empty()) m_frames.pop_front();
    }

    // 采样一次 RTT (上层在「输入发出→确认返回」时测得, 毫秒)。
    void AddRttSample(uint64_t rttMs) { m_rtt.Add(rttMs); }

    // ---- 观测 ----
    uint32_t Depth()            const { return (uint32_t)m_frames.size(); }
    uint32_t TargetDepth()      const { return m_cfg.targetDepth; }
    uint32_t MaxObservedDepth() const { return m_maxDepth; }
    uint32_t OverflowCount()    const { return m_overflowCount; }
    uint32_t StallCount()       const { return m_stallCount; }
    uint64_t RttMs()            const { return m_rtt.Value(); }
    bool     HasRtt()           const { return m_rtt.Has(); }
    uint64_t ArrivalGapMs()     const { return m_arrivalGap.Value(); }

private:
    Config               m_cfg;
    std::deque<uint32_t> m_frames;        // 缓冲中的服务器帧号 (FIFO)
    uint32_t             m_maxDepth = 0;
    uint32_t             m_overflowCount = 0;
    uint32_t             m_stallCount = 0;
    uint64_t             m_lastArrivalMs = 0;
    CEwma                m_arrivalGap;
    CEwma                m_rtt;
};

} // namespace battlecore
