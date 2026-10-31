/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: BattleRng.h
      Purpose:   确定性 PRNG(splitmix64)

      CBattleRng splitmix64 确定性伪随机数发生器, 同种子同序列, 0 种子视为 1
      提供 NextU32/NextFloat/NextRange 与全局确定性源 SeedBattleRng/DetRandom/DetRandomRange/DetRandom01
  *****************************************************************/

// =============================================================================
// BattleRng.h — 确定性 PRNG (splitmix64)
//
// 设计目标 (§7.6 六件套「确定性随机」):
//   - 纯 C++、无引擎/平台依赖, 客户端与服务器各自编译同一份头文件。
//   - 替换游戏里的 rand() / random(x) / random_range / frandom / CCRANDOM_0_1。
//   - 同一种子 → 同一序列, 保证两端模拟一致。
//
// 说明: 拆出自 CBattleCore::NextRandom (splitmix64), 供 BattleCore 与
//       客户端游戏逻辑 (Hero/WeaponManager/Sp01 等) 共用。
// =============================================================================
#pragma once

#include <cstdint>

namespace battlecore {

// splitmix64 确定性伪随机数发生器 (自包含、跨平台一致)
class CBattleRng {
public:
    CBattleRng() = default;
    explicit CBattleRng(uint64_t seed) { Seed(seed); }

    // 种子 (0 视为 1, splitmix64 需非零初态)
    void Seed(uint64_t seed) { m_state = seed ? seed : 1u; }

    // 64 位随机整数
    uint64_t NextU64() {
        uint64_t z = (m_state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // 32 位随机整数
    uint32_t NextU32() { return (uint32_t)(NextU64() >> 32); }

    // [0, 1) 双精度 (53-bit 尾数, 双端一致, 避免 float 精度漂移)
    double NextDouble() {
        return (double)(NextU64() >> 11) * (1.0 / 9007199254740992.0);
    }

    // [0, 1) 单精度
    float NextFloat() { return (float)NextDouble(); }

    // [lo, hi] 闭区间整数 (lo <= hi)
    int32_t NextRange(int32_t lo, int32_t hi) {
        if (hi <= lo) return lo;
        uint32_t span = (uint32_t)(hi - lo + 1);
        return lo + (int32_t)(NextU32() % span);
    }

    // 内部状态 (供快照序列化 / 回放分支保存)
    uint64_t State() const { return m_state; }
    void SetState(uint64_t s) { m_state = s ? s : 1u; }

private:
    uint64_t m_state = 1u;
};

// ---------------------------------------------------------------------------
// 全局确定性随机源 (进程内单例, 经内联函数局部静态实现跨编译单元共享)
// 联网对战模式下战斗逻辑全部经此取随机, 保证两端一致。
// ---------------------------------------------------------------------------
namespace detail {
inline CBattleRng& NetRng() {
    static CBattleRng s_rng(1u);
    return s_rng;
}
} // namespace detail

// 初始化全局确定性随机源种子 (进入战斗时由服务器下发的 seed 调用)
inline void SeedBattleRng(uint64_t seed) { detail::NetRng().Seed(seed); }

// [lo, hi] 闭区间确定性随机整数
inline int32_t DetRandomRange(int32_t lo, int32_t hi) {
    return detail::NetRng().NextRange(lo, hi);
}

// [0, 1) 确定性浮点
inline float DetRandom01() { return detail::NetRng().NextFloat(); }

// [0, n) 确定性整数 (替换 random(n) = rand()%n)
inline int32_t DetRandom(int32_t n) {
    return n > 1 ? (int32_t)(detail::NetRng().NextU32() % (uint32_t)n) : 0;
}

} // namespace battlecore
