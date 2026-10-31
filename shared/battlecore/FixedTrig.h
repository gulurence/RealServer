/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: FixedTrig.h
      Purpose:   确定性三角函数(整数 CORDIC)

      FxSin/FxCos/FxAtan2 用整数 CORDIC 实现, 无 libm 无大表, 16 次迭代收敛约 1e-4 且逐位确定
      轴角度返回精确值避免沿轴漂移, 角度统一 fx_t 弧度范围 [-π,π]
  *****************************************************************/

// =============================================================================
// FixedTrig.h — 确定性三角函数 (整数 CORDIC, 无浮点 / 无 libm / 无大表)
//
// 为什么不用 libm: sin/cos/atan2 在跨平台(MSVC vs gcc)最后一 ULP 可能不同, 会经
// 位移放大成帧同步分叉。CORDIC 只用移位+加减, 16 次迭代收敛到 ~1e-4 精度, 且
// 逐位确定。
//
// 角度统一用 fx_t 弧度 (Q16.16), 范围 [-π, π]。方向量化(8 方向)在输入边界由
// BattleInput.h 完成, 此处只提供连续角度的确定性 sin/cos/atan2。
//
// CORDIC 常量: atan(2^-i) (Q16.16), i=0..15; 增益 1/K = ∏cos(atan(2^-i)) ≈ 0.6072529350。
// =============================================================================
#pragma once

#include "FixedPoint.h"

namespace battlecore {

// CORDIC 增益 1/K → Q16.16 = 0.6072529350 * 65536 ≈ 39799
constexpr fx_t kCordicGain = (fx_t)39799;

// atan(2^-i) (Q16.16), i=0..15 (确定性常量表)。
inline const int32_t* CordicAtanTable() {
    static const int32_t kAtan[16] = {
        51472, 30386, 16055, 8150, 4091, 2047, 1024, 512,
        256, 128, 64, 32, 16, 8, 4, 2
    };
    return kAtan;
}

// ---------------------------------------------------------------------------
// CORDIC 核心 (旋转模式: 输入 rad 已归约到 [0, π/2], 输出 sin/cos)
// ---------------------------------------------------------------------------
inline void CordicSinCosQuadrant(fx_t rad, fx_t& s, fx_t& c) {
    fx_t x = kCordicGain;   // 预乘 1/K, 使输出即为 sin/cos 而非 K*sin/K*cos
    fx_t y = 0;
    fx_t z = rad;
    const int32_t* at = CordicAtanTable();
    for (int i = 0; i < 16; ++i) {
        fx_t dx = y >> i;
        fx_t dy = x >> i;
        if (z >= 0) { x -= dx; y += dy; z -= at[i]; }
        else        { x += dx; y -= dy; z += at[i]; }
    }
    c = x;
    s = y;
}

// 同时算 sin/cos (rad 任意, 自动象限归约)。
inline void FxSinCos(fx_t rad, fx_t& s, fx_t& c) {
    fx_t a = rad;
    while (a >= kFxTwoPi) a -= kFxTwoPi;
    while (a < 0)         a += kFxTwoPi;

    // 主轴精确值 (浮点 cos(0)/sin(π/2) 本就精确等于 ±1/0, 定点亦应精确,
    // 避免沿轴射击的子弹每帧累积 CORDIC 舍入漂移)。
    if (a == 0)              { s = 0;               c = kFxOne;        return; }
    if (a == kFxPiOver2)     { s = kFxOne;          c = 0;             return; }
    if (a == kFxPi)          { s = 0;               c = FxNeg(kFxOne); return; }
    if (a == kFx3PiOver2)    { s = FxNeg(kFxOne);   c = 0;             return; }

    if (a < kFxPiOver2) {
        // Q1: sin(a)=sin(r), cos(a)=cos(r)
        CordicSinCosQuadrant(a, s, c);
    } else if (a < kFxPi) {
        // Q2: r=a-π/2, sin(a)=cos(r), cos(a)=-sin(r)
        CordicSinCosQuadrant(a - kFxPiOver2, s, c);
        fx_t t = s; s = c; c = FxNeg(t);
    } else if (a < kFx3PiOver2) {
        // Q3: r=a-π, sin(a)=-sin(r), cos(a)=-cos(r)
        CordicSinCosQuadrant(a - kFxPi, s, c);
        s = FxNeg(s); c = FxNeg(c);
    } else {
        // Q4: r=a-3π/2, sin(a)=-cos(r), cos(a)=sin(r)
        CordicSinCosQuadrant(a - kFx3PiOver2, s, c);
        fx_t t = s; s = FxNeg(c); c = t;
    }
}

inline fx_t FxSin(fx_t rad) { fx_t s, c; FxSinCos(rad, s, c); return s; }
inline fx_t FxCos(fx_t rad) { fx_t s, c; FxSinCos(rad, s, c); return c; }

// ---------------------------------------------------------------------------
// CORDIC 核心 (向量模式: 输入 0 <= a <= b, 输出 atan(a/b) ∈ [0, π/4])
// ---------------------------------------------------------------------------
inline fx_t CordicAtan2Quadrant(fx_t a, fx_t b) {
    fx_t x = b;
    fx_t y = a;
    fx_t z = 0;
    const int32_t* at = CordicAtanTable();
    for (int i = 0; i < 16; ++i) {
        if (y > 0) {
            // 顺时针旋转把 y 压向 0
            fx_t nx = x + (y >> i);
            fx_t ny = y - (x >> i);
            x = nx; y = ny;
            z -= at[i];
        } else {
            fx_t nx = x - (y >> i);
            fx_t ny = y + (x >> i);
            x = nx; y = ny;
            z += at[i];
        }
    }
    // z 累计的是「把向量转回 x 轴」的角度(负), 取反得 atan(a/b)
    return FxNeg(z);
}

// atan2(y, x) → fx_t 弧度, 范围 [-π, π]。
inline fx_t FxAtan2(fx_t y, fx_t x) {
    if (x == 0 && y == 0) return 0;
    // 主轴精确值: 与 FxSinCos 的轴精确分支对齐 (atan2(0,+)=0 / atan2(0,-)=π /
    // atan2(+,0)=π/2 / atan2(-,0)=-π/2)。否则沿轴向量经 CORDIC 会带 1 ULP 误差,
    // 经 FxSin/FxCos 放大成「沿轴射击枪口偏移 1px」与「怪物沿轴追踪的 y 漂移」
    // (两者都会让子弹/怪物偏离轴线 → 帧同步哈希分叉)。
    if (y == 0) return (x > 0) ? 0 : kFxPi;
    if (x == 0) return (y > 0) ? kFxPiOver2 : FxNeg(kFxPiOver2);
    fx_t ax = FxAbs(x);
    fx_t ay = FxAbs(y);
    // base = atan2(|y|, |x|) ∈ [0, π/2]
    fx_t base;
    if (ax >= ay)      base = CordicAtan2Quadrant(ay, ax);          // ∈ [0, π/4]
    else               base = kFxPiOver2 - CordicAtan2Quadrant(ax, ay);  // ∈ [π/4, π/2]

    // 由 x/y 符号还原到 [-π, π]
    if (x >= 0 && y >= 0)     return base;                    // Q1
    if (x <  0 && y >= 0)     return kFxPi - base;            // Q2
    if (x <  0 && y <  0)     return base - kFxPi;            // Q3
    return FxNeg(base);                                       // Q4
}

} // namespace battlecore
