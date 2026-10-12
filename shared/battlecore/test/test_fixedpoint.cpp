// =============================================================================
// test_fixedpoint.cpp — FixedPoint.h / FixedTrig.h 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /Fe:test_fixedpoint.exe test_fixedpoint.cpp
// 运行: test_fixedpoint.exe  (退出码 0=通过, 非 0=失败并打印首个失败点)
//
// 验证点: 四则/开方/三角 与 double 参考值逐点对拍 (精度阈值内), 以及确定性
// (同输入两次结果位一致, 平凡成立但显式断言)。
// =============================================================================
#include "../FixedPoint.h"
#include "../FixedTrig.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace battlecore;

static int g_fail = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            ++g_fail;                                       \
            printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); \
            if (g_fail > 20) { printf("... 已超过 20 处失败, 提前退出\n"); exit(1); } \
        }                                                   \
    } while (0)

static void TestConversion() {
    // 整数 ↔ 定点
    CHECK(FxToIntFloor(FxFromInt(137)) == 137, "FxFromInt/FxToIntFloor 137");
    CHECK(FxToIntRound(FxFromInt(137)) == 137, "FxFromInt/FxToIntRound 137");
    CHECK(FxFromInt(3) == 3 * 65536, "FxFromInt(3)");
    CHECK(FxToIntFloor(FxFromInt(-5)) == -5, "负整数 floor -5");
    CHECK(FxToIntRound(FxFromInt(-5)) == -5, "负整数 round -5");

    // 半值四舍五入: 0.5 → 1, -0.5 → -1 (半值远离零)
    CHECK(FxToIntRound(kFxHalf) == 1, "0.5 round -> 1");
    CHECK(FxToIntRound(-kFxHalf) == -1, "-0.5 round -> -1");
    CHECK(FxToIntFloor(kFxHalf) == 0, "0.5 floor -> 0");

    // float ↔ 定点 (FxFromFloat 截断; 2.5 恰可精确表示)
    CHECK(FxFromFloat(2.5f) == FxFromInt(2) + kFxHalf, "FxFromFloat(2.5) == 2.5");
    CHECK(FxToIntFloor(FxFromFloat(2.5f)) == 2, "FxFromFloat(2.5) floor -> 2");
    CHECK(FxToIntRound(FxFromFloat(2.5f)) == 3, "FxFromFloat(2.5) round -> 3");
    CHECK(FxToIntFloor(FxFromFloat(-2.5f)) == -3, "FxFromFloat(-2.5) floor -> -3");
    CHECK(FxToIntRound(FxFromFloat(-2.5f)) == -3, "FxFromFloat(-2.5) round -> -3");
    // 非精确值: 0.05f 量化(截断)后 floor 仍为 0
    CHECK(FxToIntFloor(FxFromFloat(0.05f)) == 0, "FxFromFloat(0.05) floor -> 0");
}

static void TestArithmetic() {
    // 乘法对拍 double (输入均 Q16.16 可精确表示, 排除输入量化噪声)
    struct { float a, b; } cases[] = {
        {2.0f, 3.5f}, {1.5f, 2.25f}, {-1.25f, 4.0f}, {0.5f, 0.25f},
        {137.5f, 0.5f}, {-6.875f, -3.0f}, {137.5f, 0.25f}, {6.875f, 8.0f},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        fx_t fa = FxFromFloat(cases[i].a);
        fx_t fb = FxFromFloat(cases[i].b);
        double expect = (double)cases[i].a * cases[i].b;
        double got = (double)FxToFloat(FxMul(fa, fb));
        CHECK(fabs(got - expect) < 1e-3, "FxMul %.3f*%.3f = %.6f (期望 %.6f)",
              cases[i].a, cases[i].b, got, expect);
    }

    // 除法对拍 double (输入均 Q16.16 可精确表示; 非整除结果仅末尾量化 ~1.5e-5, 远小于阈值)
    struct { float a, b; } dcases[] = {
        {10.0f, 4.0f}, {1.0f, 4.0f}, {-5.0f, 2.0f}, {6.875f, 0.5f},
        {137.5f, 5.0f}, {10.0f, 3.0f}, {1.0f, 3.0f}, {-6.875f, 4.0f},
    };
    for (size_t i = 0; i < sizeof(dcases)/sizeof(dcases[0]); ++i) {
        fx_t fa = FxFromFloat(dcases[i].a);
        fx_t fb = FxFromFloat(dcases[i].b);
        double expect = (double)dcases[i].a / dcases[i].b;
        double got = (double)FxToFloat(FxDiv(fa, fb));
        CHECK(fabs(got - expect) < 1e-3, "FxDiv %.3f/%.3f = %.6f (期望 %.6f)",
              dcases[i].a, dcases[i].b, got, expect);
    }

    // 开方对拍
    for (int v = 1; v <= 1000; ++v) {
        fx_t a = FxFromInt(v);
        double expect = sqrt((double)v);
        double got = (double)FxToFloat(FxSqrt(a));
        CHECK(fabs(got - expect) < 2e-3, "FxSqrt(%d) = %.5f (期望 %.5f)", v, got, expect);
        if (g_fail > 20) return;
    }
    // 非整数开方
    {
        fx_t a = FxFromFloat(2.25f);
        CHECK(fabs((double)FxToFloat(FxSqrt(a)) - 1.5) < 2e-3, "FxSqrt(2.25) = 1.5");
    }
}

static void TestTrig() {
    // sin/cos 对拍 (全圆密集采样)
    const int N = 2000;
    double maxSinErr = 0, maxCosErr = 0;
    for (int i = 0; i <= N; ++i) {
        double rad = -3.14159265358979 + (2 * 3.14159265358979) * i / N;
        fx_t fr = FxFromFloat((float)rad);
        double s = (double)FxToFloat(FxSin(fr));
        double c = (double)FxToFloat(FxCos(fr));
        double es = sin(rad), ec = cos(rad);
        double se = fabs(s - es), ce = fabs(c - ec);
        if (se > maxSinErr) maxSinErr = se;
        if (ce > maxCosErr) maxCosErr = ce;
        if (se > 2e-3 || ce > 2e-3) {
            CHECK(false, "FxSin/FxCos rad=%.4f sin=%.6f(期望%.6f) cos=%.6f(期望%.6f)",
                  rad, s, es, c, ec);
            break;
        }
    }
    printf("[sin 最大误差 %.2e, cos 最大误差 %.2e]\n", maxSinErr, maxCosErr);
    CHECK(maxSinErr < 2e-3 && maxCosErr < 2e-3, "sin/cos 误差超阈值");

    // atan2 对拍 (全象限网格 + 随机)
    double maxAtanErr = 0;
    for (int gy = -200; gy <= 200; ++gy) {
        for (int gx = -200; gx <= 200; ++gx) {
            if (gx == 0 && gy == 0) continue;
            double x = gx * 5.0, y = gy * 5.0;
            fx_t fx = FxFromFloat((float)x);
            fx_t fy = FxFromFloat((float)y);
            double got = (double)FxToFloat(FxAtan2(fy, fx));
            double expect = atan2(y, x);
            double e = fabs(got - expect);
            if (e > maxAtanErr) maxAtanErr = e;
            if (e > 3e-3) {
                CHECK(false, "FxAtan2(%g,%g)=%.6f 期望 %.6f", y, x, got, expect);
                return;
            }
        }
    }
    // 随机点 (确定性种子)
    srand(12345);
    for (int i = 0; i < 5000; ++i) {
        double x = ((double)rand() / RAND_MAX * 2 - 1) * 2000;
        double y = ((double)rand() / RAND_MAX * 2 - 1) * 2000;
        if (x == 0 && y == 0) continue;
        fx_t fx = FxFromFloat((float)x);
        fx_t fy = FxFromFloat((float)y);
        double got = (double)FxToFloat(FxAtan2(fy, fx));
        double expect = atan2(y, x);
        double e = fabs(got - expect);
        if (e > maxAtanErr) maxAtanErr = e;
        if (e > 3e-3) {
            CHECK(false, "FxAtan2(rand %g,%g)=%.6f 期望 %.6f", y, x, got, expect);
            return;
        }
    }
    printf("[atan2 最大误差 %.2e]\n", maxAtanErr);
    CHECK(maxAtanErr < 3e-3, "atan2 误差超阈值");

    // 8 方向精确值 (对角用 1/√2 常量)
    CHECK(fabs((double)FxToFloat(FxSin(kFxPiOver2)) - 1.0) < 1e-3, "sin(π/2)=1");
    CHECK(fabs((double)FxToFloat(FxSin(0)) - 0.0) < 1e-3, "sin(0)=0");
    CHECK(fabs((double)FxToFloat(FxCos(0)) - 1.0) < 1e-3, "cos(0)=1");
    CHECK(fabs((double)FxToFloat(FxSin(kFxPiOver4)) - 0.70710678) < 1e-3, "sin(π/4)=√2/2");
    CHECK(fabs((double)FxToFloat(FxCos(kFxPiOver4)) - 0.70710678) < 1e-3, "cos(π/4)=√2/2");
}

static void TestBoundary() {
    // 加/减/取负/取绝对值 (TestArithmetic 未覆盖的基础四则)
    CHECK(FxAdd(FxFromInt(3), FxFromInt(4)) == FxFromInt(7), "FxAdd");
    CHECK(FxSub(FxFromInt(3), FxFromInt(4)) == FxFromInt(-1), "FxSub");
    CHECK(FxNeg(FxFromInt(5)) == FxFromInt(-5), "FxNeg");
    CHECK(FxAbs(FxFromInt(-5)) == FxFromInt(5), "FxAbs");
    CHECK(FxAbs(kFxHalf) == kFxHalf, "FxAbs 正值不变");

    // 范围边界: 最大可表示整数 ±32767 (32768 会溢出, 明确不在支持范围)
    CHECK(FxFromInt(32767) == 32767 * 65536, "FxFromInt(32767)");
    CHECK(FxToIntFloor(FxFromInt(-32767)) == -32767, "FxFromInt(-32767) 往返");

    // 乘法 int64 中间量: 两个较大定点相乘, 中间积超 int32 但落 int64, 结果仍正确
    // 20000 * 1.5 = 30000 (中间积 ~1.3e14, 若用 int32 中间量会溢出)
    CHECK(FxMul(FxFromInt(20000), FxFromInt(1) + kFxHalf) == FxFromInt(30000),
          "FxMul 大数 int64 中间量");

    // 单位元 / 零元
    CHECK(FxMul(kFxOne, kFxOne) == kFxOne, "1*1==1");
    CHECK(FxDiv(kFxOne, kFxOne) == kFxOne, "1/1==1");
    CHECK(FxSqrt(kFxOne) == kFxOne, "FxSqrt(1)==1");
    CHECK(FxSqrt(0) == 0, "FxSqrt(0)==0");
}

static void TestDeterminism() {
    // 确定性: 同输入两次结果位一致
    fx_t a = FxFromFloat(3.14159f);
    fx_t s1, c1, s2, c2;
    FxSinCos(a, s1, c1);
    FxSinCos(a, s2, c2);
    CHECK(s1 == s2 && c1 == c2, "FxSinCos 确定性");
    CHECK(FxAtan2(FxFromInt(3), FxFromInt(4)) == FxAtan2(FxFromInt(3), FxFromInt(4)),
          "FxAtan2 确定性");
}

int main() {
    printf("== FixedPoint / FixedTrig 单元测试 ==\n");
    TestConversion();
    TestArithmetic();
    TestTrig();
    TestBoundary();
    TestDeterminism();
    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
