// =============================================================================
// FixedPoint.h — Q16.16 定点数 (帧同步确定性数学基础, 六件套「整数数值」)
//
// 目标: 帧同步战斗逻辑里所有浮点计算统一到 int 定点, 跨平台(客户端 MSVC / 服务器
// gcc) 逐位一致, 消除 IEEE754 漂移。tick 热路径内零浮点调用。
//
// 表示: Q16.16 —— 底层 int32_t, 高 16 位整数 + 低 16 位小数。
//   精度 1/65536 ≈ 1.5e-5; 范围 ±32768 (足够 1024x640 地图 + 速度量级)。
//
// 约定:
//   - 中间乘除一律 int64_t 防溢出。
//   - 乘除统一「四舍五入, 半值远离零 (round-half-away-from-zero)」, 全库一个舍入规则。
//   - FxFromFloat/FxToFloat 只用于「加载/序列化边界」和「渲染边界」, 绝不进 tick 热路径。
//
// 公式参考 libfixmath (MIT, https://github.com/PetteriAimonen/libfixmath), 只取
// add/sub/mul/div/sqrt, 不整库 vendor。
// =============================================================================
#pragma once

#include <cstdint>

namespace battlecore {

// Q16.16 定点类型 (int32_t, 16 位小数)
typedef int32_t fx_t;

constexpr int32_t kFxShift = 16;
constexpr fx_t    kFxOne   = (fx_t)(1 << 16);   // 1.0 = 65536
constexpr fx_t    kFxHalf  = (fx_t)(1 << 15);   // 0.5 = 32768
constexpr fx_t    kFxEps   = (fx_t)1;           // 最小正精度 1/65536

// π 及常用分数 (Q16.16, 精确整数常量)
constexpr fx_t kFxPi        = (fx_t)205887;   // π
constexpr fx_t kFxTwoPi     = (fx_t)411775;   // 2π
constexpr fx_t kFxPiOver2   = (fx_t)102944;   // π/2
constexpr fx_t kFxPiOver4   = (fx_t)51472;    // π/4
constexpr fx_t kFx3PiOver4  = (fx_t)154416;   // 3π/4
constexpr fx_t kFx3PiOver2  = (fx_t)308831;   // 3π/2
constexpr fx_t kFxSqrtHalf  = (fx_t)46341;    // 1/√2 ≈ 0.70710678

// ---------------------------------------------------------------------------
// 构造 / 转换 (仅边界用)
// ---------------------------------------------------------------------------

// 整数 → 定点。要求 |v| <= 32767 (更大需自行用 int64 组合)。
inline fx_t FxFromInt(int32_t v) {
    return (fx_t)((int64_t)v << kFxShift);
}

// 定点 → 整数 (向下取整, 即向 -∞; 依赖算术右移, MSVC/gcc 一致)。
inline int32_t FxToIntFloor(fx_t v) {
    return v >> kFxShift;
}

// 定点 → 整数 (四舍五入, 半值远离零: 正数 floor(v+0.5), 负数 -floor(|v|+0.5))。
inline int32_t FxToIntRound(fx_t v) {
    if (v >= 0) return (int32_t)(((int64_t)v + kFxHalf) >> kFxShift);
    return (int32_t)-((((int64_t)(-v)) + kFxHalf) >> kFxShift);
}

// 定点 → float (仅渲染边界)。定点 → double 同理可用 (float) 换成 (double)。
inline float FxToFloat(fx_t v) {
    return (float)v / 65536.0f;
}

// float → 定点 (仅加载/序列化边界; 单次截断舍入, 确定性)。
inline fx_t FxFromFloat(float f) {
    return (fx_t)(int32_t)(f * 65536.0f);
}

// ---------------------------------------------------------------------------
// 算术 (全整数)
// ---------------------------------------------------------------------------

inline fx_t FxAdd(fx_t a, fx_t b) { return (fx_t)(a + b); }
inline fx_t FxSub(fx_t a, fx_t b) { return (fx_t)(a - b); }
inline fx_t FxNeg(fx_t a)         { return (fx_t)(-a); }
inline fx_t FxAbs(fx_t a)         { return (fx_t)(a < 0 ? -a : a); }

// 乘法: (int64)a*b 中间, 加 kFxHalf 后算术右移 → 四舍五入半值远离零。
inline fx_t FxMul(fx_t a, fx_t b) {
    int64_t p = (int64_t)a * (int64_t)b;
    p += (p >= 0) ? kFxHalf : -kFxHalf;
    return (fx_t)(p >> kFxShift);
}

// 除法: (a<<16)/b, 四舍五入半值远离零。b 不得为 0。
inline fx_t FxDiv(fx_t a, fx_t b) {
    int64_t num = (int64_t)a << kFxShift;
    int64_t q   = num / b;
    int64_t r   = num % b;
    int64_t bAbs = (b < 0) ? -(int64_t)b : (int64_t)b;
    int64_t rAbs = (r < 0) ? -r : r;
    if (rAbs * 2 >= bAbs)
        q += ((num < 0) == (b < 0)) ? 1 : -1;   // 半值远离零
    return (fx_t)q;
}

// 平方
inline fx_t FxSquare(fx_t a) { return FxMul(a, a); }

// 开方 (非负输入; 整数牛顿迭代, 结果向下取整, 误差 < 1 LSB)。
inline uint64_t FxU64SqrtFloor(uint64_t n) {
    if (n <= 1) return n;
    uint64_t x = n;
    uint64_t y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
}

inline fx_t FxSqrt(fx_t a) {
    if (a <= 0) return 0;
    return (fx_t)FxU64SqrtFloor((uint64_t)a << kFxShift);
}

} // namespace battlecore
