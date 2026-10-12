// =============================================================================
// BattleInput.h — 真实输入指令结构 (七件套「输入指令」)
//
// 移动: 连续摇杆量化成离散 8 方向 (move_dir); 射击: 传连续方向向量
// (shoot_dx/shoot_dy, Q16.16 定点, 数学坐标 Y 向上), 核心用 FxAtan2 算角度。
// 与 battle.proto 的 PBBattlePlayerInput 一一对应。客户端每逻辑帧采集一次,
// 经 ProtoWriter 编码上报服务器权威核心; 两端用同一编码保证确定性。
//
// 移动方向编码 (move_dir):
//   0=停止, 1=上, 2=右上, 3=右, 4=右下, 5=下, 6=左下, 7=左, 8=左上
// =============================================================================
#pragma once

#include <cstdint>

#include "FixedPoint.h"

namespace battlecore {

// 单帧输入 (镜像 battle.proto PBBattlePlayerInput)
struct PlayerInput {
    int64_t  player_id = 0;
    uint32_t move_dir  = 0;   // 移动方向 0-8
    fx_t     shoot_dx  = 0;   // 射击方向向量 X (Q16.16 定点; 数学坐标 Y 向上)
    fx_t     shoot_dy  = 0;   // 射击方向向量 Y (Q16.16 定点)
    uint32_t skill_id  = 0;   // 技能 ID (0=无)
    bool     fire      = false;   // 是否开火
    uint32_t gun_type  = 0;   // 当前枪械类别 (内核 kGun*: 手枪0/机枪1/散弹2/狙击3/加特林16/火箭筒17)
};

// ---------------------------------------------------------------------------
// 技能 ID (PlayerInput::skill_id 取值; 客户端 SillButtonID 在采集输入时映射到这里)
// ---------------------------------------------------------------------------
enum SkillId : uint32_t {
    kSkillNone            = 0,   // 无技能
    kSkillFullScreenBomb  = 1,   // 全屏炸弹 (以英雄为中心, 半径 500, 0.5s 引信后结算)
    kSkillFullScreenFrozen = 2,  // 全屏冰冻 (占位, 未实现)
};

// ---------------------------------------------------------------------------
// 连续方向 → 离散 8 方向 (0=无方向)
// ---------------------------------------------------------------------------

// 由角度 (弧度) 量化到 8 方向。0 弧度 = 右, 逆时针增加。
// 约定: 方向 3=右(0), 1=上(π/2), 7=左(π), 5=下(-π/2)。
inline uint32_t DirFromRadians(float radians, float deadZone = 0.0f) {
    // 把角度归一化到 [-π, π)
    const float kTwoPi = 6.28318530718f;
    float a = radians;
    while (a >=  3.14159265359f) a -= kTwoPi;
    while (a <  -3.14159265359f) a += kTwoPi;

    // 8 方向, 每 45° 一个扇区; 0° (右) 为方向 3。
    // 用角度除以 π/4 得到扇区索引, 再映射到 BattleCore 编码。
    const float kStep = 3.14159265359f / 4.0f;   // 45°
    int sector = (int)((a + 3.14159265359f + kStep / 2.0f) / kStep);   // 0..8
    // sector 0 对应 -π (左), 8 对应 +π (左, 环绕)
    if (sector >= 8) sector = 0;

    // sector → 8 方向编码 (左=-π 是方向 7, 逆时针递增)
    //   sector 0 (左)         -> 7
    //   sector 1 (左下)        -> 6
    //   sector 2 (下)          -> 5
    //   sector 3 (右下)        -> 4
    //   sector 4 (右)          -> 3
    //   sector 5 (右上)        -> 2
    //   sector 6 (上)          -> 1
    //   sector 7 (左上)        -> 8
    static const uint32_t kMap[8] = { 7, 6, 5, 4, 3, 2, 1, 8 };
    return kMap[sector];
}

// 由方向向量 (dx, dy) 量化到 8 方向 (0=无方向)。
// 屏幕坐标 Y 向下, 这里按数学坐标 (Y 向上) 传入: dy>0 为上。
inline uint32_t DirFromVector(float dx, float dy, float deadZone = 0.1f) {
    float len2 = dx * dx + dy * dy;
    if (len2 < deadZone * deadZone) return 0;
    // 手写扇区判断避免 atan2 平台差异 (确定性): 用 |dx|/|dy| 相对关系量化 8 方向
    float ax = dx < 0 ? -dx : dx;   // |dx|
    float ay = dy < 0 ? -dy : dy;   // |dy|
    // 用 |dx|/|dy| 相对关系判断 8 方向
    const float kTan225 = 0.41421356f;   // tan(22.5°)
    if (ax <= ay * kTan225) {
        // 主要上下
        return (dy > 0) ? 1 : 5;   // 上 / 下
    }
    if (ay <= ax * kTan225) {
        // 主要左右
        return (dx > 0) ? 3 : 7;   // 右 / 左
    }
    // 对角线
    if (dx > 0) return (dy > 0) ? 2 : 4;   // 右上 / 右下
    return (dy > 0) ? 8 : 6;               // 左上 / 左下
}

// 8 方向编码 → 单位向量 (DirFromRadians/DirFromVector 的逆映射, 供表现层 ghost 驱动)。
// 0=停返回 0 向量; 地图坐标 Y 向上 (与 DirFromRadians 约定一致: 上=+Y)。
inline void DirToUnitVector(uint32_t dir, float& dx, float& dy) {
    static const float kD = 0.70710678f;   // 1/√2
    switch (dir) {
        case 1: dx = 0;   dy = 1;    break;   // 上
        case 2: dx = kD;  dy = kD;   break;   // 右上
        case 3: dx = 1;   dy = 0;    break;   // 右
        case 4: dx = kD;  dy = -kD;  break;   // 右下
        case 5: dx = 0;   dy = -1;   break;   // 下
        case 6: dx = -kD; dy = -kD;  break;   // 左下
        case 7: dx = -1;  dy = 0;    break;   // 左
        case 8: dx = -kD; dy = kD;   break;   // 左上
        default: dx = 0;  dy = 0;    break;   // 0=停
    }
}

// 8 方向编码 → 弧度 (确定性开火角度)。与 DirToUnitVector 同一坐标系:
//   3=右→0, 1=上→π/2, 7=左→π, 5=下→-π/2; 与 Bullet 的 (cos,sin) 位移约定一致。
inline float DirToRadians(uint32_t dir) {
    static const float kPI  = 3.14159265359f;
    static const float kPI2 = 1.57079632679f;
    static const float kPI4 = 0.78539816340f;
    switch (dir) {
        case 1: return  kPI2;          // 上
        case 2: return  kPI4;          // 右上
        case 3: return  0.0f;          // 右
        case 4: return -kPI4;          // 右下
        case 5: return -kPI2;          // 下
        case 6: return -kPI2 - kPI4;   // 左下 -3π/4
        case 7: return  kPI;           // 左
        case 8: return  kPI2 + kPI4;   // 左上 3π/4
        default: return 0.0f;          // 0=停
    }
}

// ---------------------------------------------------------------------------
// 8 方向 → 定点单位向量 (Q16.16; 对角用 kFxSqrtHalf 精确常量, 无 LUT 近似)。
// 供核心 tick 内位移/枪口位置使用, 热路径零浮点。坐标系与 DirToUnitVector 一致 (Y 向上)。
// ---------------------------------------------------------------------------
inline void DirToUnitVectorFx(uint32_t dir, fx_t& dx, fx_t& dy) {
    switch (dir) {
        case 1: dx = 0;            dy = kFxOne;            break;   // 上
        case 2: dx = kFxSqrtHalf;  dy = kFxSqrtHalf;       break;   // 右上
        case 3: dx = kFxOne;       dy = 0;                 break;   // 右
        case 4: dx = kFxSqrtHalf;  dy = FxNeg(kFxSqrtHalf); break;  // 右下
        case 5: dx = 0;            dy = FxNeg(kFxOne);     break;   // 下
        case 6: dx = FxNeg(kFxSqrtHalf); dy = FxNeg(kFxSqrtHalf); break; // 左下
        case 7: dx = FxNeg(kFxOne); dy = 0;                break;   // 左
        case 8: dx = FxNeg(kFxSqrtHalf); dy = kFxSqrtHalf; break;   // 左上
        default: dx = 0; dy = 0;   break;   // 0=停
    }
}

// 8 方向 → 定点弧度 (确定性开火角度)。与 DirToRadians 同一坐标系:
//   3=右→0, 1=上→π/2, 7=左→π, 5=下→-π/2。
inline fx_t DirToRadiansFx(uint32_t dir) {
    switch (dir) {
        case 1: return kFxPiOver2;            // 上
        case 2: return kFxPiOver4;            // 右上
        case 3: return 0;                     // 右
        case 4: return FxNeg(kFxPiOver4);     // 右下
        case 5: return FxNeg(kFxPiOver2);     // 下
        case 6: return FxNeg(kFx3PiOver4);    // 左下 -3π/4
        case 7: return kFxPi;                 // 左
        case 8: return kFx3PiOver4;           // 左上 3π/4
        default: return 0;                    // 0=停
    }
}

// ---------------------------------------------------------------------------
// 连续角度 (fx_t 弧度) → 8 方向编码 (DirFromRadians 的定点版, 供渲染选动画 animDir)。
// 与 DirToRadiansFx 互为逆映射: 3=右(0), 1=上(π/2), 7=左(π), 5=下(-π/2)。
// 扇区按 π/4 均分, 加 π/8 (半扇区) 实现「就近取整」, 避免 π/(π/4)=3.99998 截断错位。
// ---------------------------------------------------------------------------
inline uint8_t AngleToEightDirFx(fx_t rad) {
    while (rad > kFxPi)             rad -= kFxTwoPi;
    while (rad < FxNeg(kFxPi))      rad += kFxTwoPi;
    const fx_t a = rad + kFxPi;                 // [0, 2π]
    const fx_t kPiOver8 = kFxPiOver4 / 2;       // π/8 (半扇区, 四舍五入)
    int32_t sector = (int32_t)((a + kPiOver8) / kFxPiOver4);
    if (sector >= 8) sector = 0;
    static const uint8_t kMap[8] = { 7, 6, 5, 4, 3, 2, 1, 8 };
    return kMap[sector];
}

} // namespace battlecore
