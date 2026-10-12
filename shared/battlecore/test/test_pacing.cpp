// =============================================================================
// test_pacing.cpp — 抖动缓冲 + 时间膨胀 + 超前量 (F4) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_pacing.exe test_pacing.cpp
// 运行: test_pacing.exe  (退出码 0=通过)
//
// 验证点 (frame-sync-client-framework.md §4.3 F4):
//   1) 时间膨胀决策: 缓冲深度 → 0(慢放)/1(正常)/2(快放), 及慢放计数。
//   2) 抖动缓冲 FIFO + 深度统计 (峰值深度、到达间隔 EWMA)。
//   3) 缓冲溢出: 满则丢最新帧并计数, 深度不超容量。
//   4) 超前目标折算: 0.5*RTT + 安全边际, 向上取整到帧。
//   5) RTT EWMA 收敛。
//   6) 完整追帧场景: 突发积压 → 时间膨胀泄洪 → 收敛, 全程确定性。
// =============================================================================
#include "../BattlePacing.h"

#include <cstdio>
#include <cstdlib>

using namespace battlecore;

static int g_fail = 0;

#define CHECK(cond, ...)                                          \
    do {                                                          \
        if (!(cond)) {                                            \
            ++g_fail;                                             \
            printf("FAIL: "); printf(__VA_ARGS__); printf("\n");   \
            if (g_fail > 20) { printf("... 已超过 20 处失败, 提前退出\n"); exit(1); } \
        }                                                         \
    } while (0)

int main() {
    printf("== 抖动缓冲 + 时间膨胀 + 超前量 (F4) 单元测试 ==\n");

    // ---- 测试 1: 时间膨胀决策 (depth → 0/1/2) ----
    {
        CBattlePacer::Config cfg;   // targetDepth=3, lowWater=1, highWater=5, capacity=32
        CBattlePacer p; p.Reset(cfg);

        CHECK(p.FramesToTick() == 0, "空缓冲 → 慢放(0)");
        CHECK(p.StallCount() == 1, "慢放计数 +1");

        // depth = 3 (正常区: lowWater < depth < highWater)
        p.OnServerFrame(1, 0); p.OnServerFrame(2, 50); p.OnServerFrame(3, 100);
        CHECK(p.Depth() == 3, "缓冲深度 == 3");
        CHECK(p.FramesToTick() == 1, "depth=3 → 正常(1)");

        // depth = 5 (高水位 → 快放)
        p.OnServerFrame(4, 150); p.OnServerFrame(5, 200);
        CHECK(p.Depth() == 5, "缓冲深度 == 5");
        CHECK(p.FramesToTick() == 2, "depth=5 → 快放(2)");

        // 消费 2 帧后 depth=3, 回到正常
        p.ConsumeFrame(); p.ConsumeFrame();
        CHECK(p.Depth() == 3, "消费后深度 == 3");
        CHECK(p.FramesToTick() == 1, "回到正常(1)");
    }

    // ---- 测试 2: 抖动缓冲 FIFO + 峰值深度 + 到达间隔 EWMA ----
    {
        CBattlePacer p; p.Reset(CBattlePacer::Config());
        // 到达时刻: 0 / 50 / 60 / 65 → 间隔 50/10/5
        p.OnServerFrame(1, 0);
        p.OnServerFrame(2, 50);
        p.OnServerFrame(3, 60);
        p.OnServerFrame(4, 65);
        CHECK(p.Depth() == 4, "缓冲深度 == 4");
        CHECK(p.MaxObservedDepth() == 4, "峰值深度 == 4");

        // 到达间隔 EWMA: 50 → (50*7+10)/8=45 → (45*7+5)/8=40
        CHECK(p.ArrivalGapMs() == 40, "到达间隔 EWMA == 40 (实际 %llu)",
              (unsigned long long)p.ArrivalGapMs());

        // FIFO: 消费按到达序取走
        p.ConsumeFrame(); CHECK(p.Depth() == 3, "消费一帧 → 深度 3");
        p.ConsumeFrame(); CHECK(p.Depth() == 2, "消费两帧 → 深度 2");
        p.ConsumeFrame(); p.ConsumeFrame();
        CHECK(p.Depth() == 0, "消费完 → 深度 0");
    }

    // ---- 测试 3: 缓冲溢出 (满则丢最新帧并计数) ----
    {
        CBattlePacer::Config cfg; cfg.capacity = 4;
        CBattlePacer p; p.Reset(cfg);
        for (uint32_t f = 1; f <= 6; ++f) p.OnServerFrame(f, (uint64_t)f * 10);
        CHECK(p.Depth() == 4, "深度不超容量 4 (实际 %u)", p.Depth());
        CHECK(p.OverflowCount() == 2, "溢出计数 == 2 (实际 %u)", p.OverflowCount());
        CHECK(p.MaxObservedDepth() == 4, "峰值深度 == 4");
    }

    // ---- 测试 4: 超前目标折算 (0.5*RTT + 安全边际, 向上取整) ----
    {
        CHECK(ComputeTargetLead(100, 2, 50) == 3, "100ms→ceil(50/50)+2=3 (实际 %u)", ComputeTargetLead(100, 2, 50));
        CHECK(ComputeTargetLead(150, 1, 50) == 3, "150ms→ceil(75/50)+1=3 (实际 %u)", ComputeTargetLead(150, 1, 50));
        CHECK(ComputeTargetLead(40, 1, 50) == 2, "40ms→ceil(20/50)+1=2 (实际 %u)", ComputeTargetLead(40, 1, 50));
        CHECK(ComputeTargetLead(0, 2, 50) == 2, "0ms→0+2=2 (实际 %u)", ComputeTargetLead(0, 2, 50));
        CHECK(ComputeTargetLead(100, 1, 0) == 1, "frameMs=0 回退安全边际 (实际 %u)", ComputeTargetLead(100, 1, 0));
    }

    // ---- 测试 5: RTT EWMA 收敛 ----
    {
        CBattlePacer p; p.Reset(CBattlePacer::Config());
        CHECK(!p.HasRtt(), "初始无 RTT 样本");
        p.AddRttSample(80);
        CHECK(p.HasRtt() && p.RttMs() == 80, "首样本 == 80 (实际 %llu)", (unsigned long long)p.RttMs());
        p.AddRttSample(160);
        CHECK(p.RttMs() == 90, "次样本 (80*7+160)/8=90 (实际 %llu)", (unsigned long long)p.RttMs());
        p.AddRttSample(160);
        CHECK(p.RttMs() == 98, "三样本 (90*7+160)/8=98 (实际 %llu)", (unsigned long long)p.RttMs());
        p.AddRttSample(160);
        CHECK(p.RttMs() == 105, "四样本 (98*7+160)/8=105 (实际 %llu)", (unsigned long long)p.RttMs());
    }

    // ---- 测试 6: 完整追帧场景 (突发积压 → 泄洪 → 收敛), 确定性 ----
    {
        auto RunScenario = []() {
            CBattlePacer p; p.Reset(CBattlePacer::Config());
            uint32_t consumed = 0;
            // 模拟网络突发: 服务器一口气追回 12 帧 (此前积压)
            for (uint32_t f = 1; f <= 12; ++f) p.OnServerFrame(f, (uint64_t)f * 50);
            CHECK(p.Depth() == 12, "突发后深度 == 12");
            CHECK(p.FramesToTick() == 2, "深度高 → 快放(2)");

            // 泄洪: 渲染循环把积压排回目标深度 3 (不是清空 —— 缓冲要留 3 帧抖动余量)
            for (int r = 0; r < 32 && p.Depth() > p.TargetDepth(); ++r) {
                uint32_t n = p.FramesToTick();
                for (uint32_t i = 0; i < n && p.Depth() > p.TargetDepth(); ++i) { p.ConsumeFrame(); ++consumed; }
            }
            CHECK(p.Depth() == p.TargetDepth(), "泄洪到目标深度 3 (实际 %u)", p.Depth());
            CHECK(consumed == 9, "消费 12-3=9 帧 (实际 %u)", consumed);
            CHECK(p.OverflowCount() == 0, "无溢出");
            CHECK(p.MaxObservedDepth() == 12, "峰值深度 == 12");
            CHECK(p.StallCount() == 0, "泄洪期间无慢放 (实际 %u)", p.StallCount());
        };
        RunScenario();   // 跑两遍验证确定性 (纯整数运算, 结果应一致)
        RunScenario();
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
