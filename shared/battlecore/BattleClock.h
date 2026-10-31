/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: BattleClock.h
      Purpose:   逻辑帧时钟与联网对战模式开关

      定义 20Hz/50ms 固定步长逻辑帧时钟(CBattleClock/NetClock)与 IsNetBattle 全局开关
      联网对战用帧号×50ms 替代墙钟, 单机回退真实墙钟, 经函数局部静态跨编译单元共享
  *****************************************************************/

// =============================================================================
// BattleClock.h — 逻辑帧时钟 + 全局联网对战模式开关
//
// 设计目标 (§7.6 六件套「固定步长」「逻辑帧」):
//   - 逻辑帧率 20Hz, 固定步长 50ms。
//   - 联网对战模式下, 战斗逻辑里的墙钟 (gettimeofday/millisecondNow/CURTIME)
//     全部替换为逻辑帧时间 (帧号 * 固定步长), 保证两端时序一致。
//   - 单机模式回退真实墙钟, 行为与改造前一致。
//
// 线程模型: 客户端单线程 (Cocos 主循环), 服务器在 Actor 调度线程调用;
//           全局状态经内联函数局部静态实现跨编译单元共享 (C++03/11 兼容)。
// =============================================================================
#pragma once

#include <cstdint>

namespace battlecore {

// 逻辑帧率 / 固定步长 (与服务器权威核心一致)
constexpr uint32_t kLogicFps     = 20;
constexpr uint32_t kFixedDeltaMs = 1000u / kLogicFps;   // 50 ms

// ---------------------------------------------------------------------------
// 全局联网对战模式开关
// ---------------------------------------------------------------------------
namespace detail {
inline bool& NetBattleFlag() {
    static bool s_flag = false;   // 函数局部静态: 全程序唯一
    return s_flag;
}
} // namespace detail

// 是否处于联网帧同步对战 (true 时确定性随机/逻辑帧时钟生效)
inline bool IsNetBattle() { return detail::NetBattleFlag(); }

// 进入/退出联网对战模式
inline void SetNetBattle(bool on) { detail::NetBattleFlag() = on; }

// ---------------------------------------------------------------------------
// 全局逻辑帧时钟 (联网模式下为权威逻辑时间源)
// ---------------------------------------------------------------------------
namespace detail {
class CBattleClock {
public:
    void Reset(uint32_t startFrame = 0) {
        m_frame  = startFrame;
        m_timeMs = (uint64_t)startFrame * kFixedDeltaMs;
    }
    void Advance() {
        ++m_frame;
        m_timeMs = (uint64_t)m_frame * kFixedDeltaMs;
    }
    uint32_t Frame()  const { return m_frame; }
    uint64_t TimeMs() const { return m_timeMs; }

private:
    uint32_t m_frame  = 0;
    uint64_t m_timeMs = 0;
};

inline CBattleClock& NetClock() {
    static CBattleClock s_clock;
    return s_clock;
}
} // namespace detail

// 重置逻辑帧时钟
inline void ResetBattleClock(uint32_t startFrame = 0) {
    detail::NetClock().Reset(startFrame);
}

// 推进一个逻辑帧
inline void AdvanceBattleClock() { detail::NetClock().Advance(); }

// 当前逻辑帧号
inline uint32_t CurrentFrameNo() { return detail::NetClock().Frame(); }

// 当前逻辑时间 (毫秒) — 替换墙钟毫秒。单机模式下调用方应回退真实墙钟。
inline uint64_t CurrentFrameTimeMs() { return detail::NetClock().TimeMs(); }

} // namespace battlecore
