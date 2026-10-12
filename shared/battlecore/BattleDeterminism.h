// =============================================================================
// BattleDeterminism.h — 确定性入口宏 (六件套「确定性随机」统一收口)
//
// 把游戏战斗逻辑里的非确定性调用 (rand/random/random_range/frandom/
// CCRANDOM_0_1 与墙钟毫秒) 统一收口到 battlecore 的确定性源。
//
// 语义:
//   - 联网帧同步对战 (IsNetBattle()==true): 走确定性 PRNG + 逻辑帧时钟,
//     两端序列一致。
//   - 单机模式 (IsNetBattle()==false): 回退 rand()/系统时钟, 行为与改造前一致,
//     不影响现有单机玩法。
//
// 用法: 客户端 Defines.h 把 random(x)/random_range/frandom/frandom_range/
//       CCRANDOM_0_1 宏重定义为下列 NetXxx 调用; WeaponManager 等直接读墙钟的
//       代码改调 NetCurTimeMs()。战斗逻辑因此零改动接入确定性源。
// =============================================================================
#pragma once

#include <chrono>
#include <cstdlib>

#include "BattleClock.h"
#include "BattleRng.h"

namespace battlecore {

// ---------------------------------------------------------------------------
// 确定性随机 (单机回退 rand())
// ---------------------------------------------------------------------------

// [0, n) 整数 — 替换 random(n) = rand()%n
inline int32_t NetRandom(int32_t n) {
    if (IsNetBattle()) return DetRandom(n);
    return n > 1 ? (int32_t)((uint32_t)rand() % (uint32_t)n) : 0;
}

// [lo, hi] 闭区间整数 — 替换 random_range(lo, hi)
inline int32_t NetRandomRange(int32_t lo, int32_t hi) {
    if (IsNetBattle()) return DetRandomRange(lo, hi);
    return (rand() % (hi - lo + 1)) + lo;
}

// [0, 1) 浮点 — 替换 CCRANDOM_0_1()
inline float NetFrandom01() {
    if (IsNetBattle()) return DetRandom01();
    return (float)rand() / (float)RAND_MAX;
}

// [0, 1) 浮点 (32 位整型分母, 与旧 frandom 一致) — 替换 frandom
inline float NetFrandom() {
    if (IsNetBattle()) return DetRandom01();
    return (float)rand() / (float)0x100000000u;
}

// [lo, hi] 区间浮点 — 替换 frandom_range(lo, hi)
inline float NetFrandomRange(float lo, float hi) {
    return (hi - lo) * NetFrandom() + lo;
}

// ---------------------------------------------------------------------------
// 逻辑时钟 (单机回退系统墙钟毫秒)
// ---------------------------------------------------------------------------

// 当前时间 (毫秒)。联网模式下为逻辑帧时间 (帧号 * 50ms), 单机回退真实墙钟。
inline uint64_t NetCurTimeMs() {
    if (IsNetBattle()) return CurrentFrameTimeMs();
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()).count();
}

} // namespace battlecore
