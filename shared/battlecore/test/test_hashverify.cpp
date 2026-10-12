// =============================================================================
// test_hashverify.cpp — 哈希校验 + 快照纠偏 (F3) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_hashverify.exe test_hashverify.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_hashverify.exe  (退出码 0=通过)
//
// 验证点 (frame-sync-client-framework.md §4.3 F3):
//   1) 确定性一致性: 客户端与服务器同输入 → VerifyFrame 各确认帧哈希一致, 零失配。
//   2) 失配检测: 客户端状态被破坏(模拟确定性漂移, 输入不变) → VerifyFrame 检出失配。
//      (F2 的输入 diff 抓不到这种「同输入却状态漂移」, 正是 F3 补的缺口。)
//   3) 快照纠偏: ResyncFromSnapshot 全量恢复 → 之后继续推进与服务器逐位一致。
// =============================================================================
#include "../BattlePrediction.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

static PlayerInput In(int64_t pid, uint32_t dir, bool fire) {
    PlayerInput p;
    p.player_id = pid; p.move_dir = dir; p.fire = fire;
    DirToUnitVectorFx(dir, p.shoot_dx, p.shoot_dy);
    return p;
}

static void SetupCore(CBattleCore& c, uint32_t seed, const std::vector<int64_t>& ids) {
    c.Init(seed, ids);
    c.MutableCollision().mapWidth = 1024;
    c.MutableCollision().mapHeight = 640;
}

int main() {
    printf("== 哈希校验 + 快照纠偏 (F3) 单元测试 ==\n");

    const uint32_t N = 60;
    const uint32_t DELAY = 3;
    const uint32_t seed = 0xABC;
    std::vector<int64_t> pid = { 101, 202 };

    // 输入脚本: p1 一直右移(3), 每 10 帧开火; p2 一直左移(7), 每 15 帧开火。
    // 客户端对 p2 用「完美预测」(猜对) → 不触发 F2 回滚, 便于独立测 F3 的哈希失配路径。
    std::vector<PlayerInput> p1(N + 1), p2(N + 1);
    for (uint32_t f = 1; f <= N; ++f) {
        p1[f] = In(101, 3, (f % 10) == 0);
        p2[f] = In(202, 7, (f % 15) == 0);
    }

    // ---- 权威服务器核心: 逐帧记录 hash + snapshot ----
    CBattleCore srv;
    SetupCore(srv, seed, pid);
    std::vector<uint64_t> srvHash(N + 1);
    std::vector<std::string> srvSnap(N + 1);
    for (uint32_t f = 1; f <= N; ++f) {
        srv.SubmitInput(p1[f]); srv.SubmitInput(p2[f]); srv.Tick(f);
        srvHash[f] = srv.HashState();
        srvSnap[f] = srv.SerializeState();
    }

    // 跑一个「完美预测 + 全部确认」的客户端到 N 帧 (无状态破坏)
    auto RunCleanClient = [&](CPredictiveSim& cli) {
        for (uint32_t f = 1; f <= N; ++f) {
            std::vector<PlayerInput> pred; pred.push_back(p1[f]); pred.push_back(p2[f]);
            cli.PredictFrame(f, pred);
            if (f >= DELAY) {
                ServerFrameCommand cmd;
                cmd.frame = f - DELAY;
                cmd.inputs.push_back(p1[f - DELAY]); cmd.inputs.push_back(p2[f - DELAY]);
                cli.OnServerFrame(cmd);
            }
        }
        for (uint32_t f = N - DELAY + 1; f <= N; ++f) {
            ServerFrameCommand cmd;
            cmd.frame = f;
            cmd.inputs.push_back(p1[f]); cmd.inputs.push_back(p2[f]);
            cli.OnServerFrame(cmd);
        }
    };

    // ---- 测试 1: 确定性一致性 ----
    {
        CPredictiveSim cli;
        CHECK(cli.Init(seed, pid), "Init");
        cli.MutableCore().MutableCollision().mapWidth = 1024;
        cli.MutableCore().MutableCollision().mapHeight = 640;
        RunCleanClient(cli);
        CHECK(cli.LastConfirmedFrame() == N, "全部确认 (实际 %u)", cli.LastConfirmedFrame());
        CHECK(cli.RollbackCount() == 0, "完美预测零回滚");

        bool allOk = true;
        for (uint32_t f = 10; f <= N; f += 10) allOk = allOk && cli.VerifyFrame(f, srvHash[f]);
        CHECK(allOk, "确定性: 各确认帧哈希校验一致");
        CHECK(cli.HashMismatchCount() == 0, "零失配 (实际 %u)", cli.HashMismatchCount());
        CHECK(cli.HashState() == srvHash[N], "校验后预测状态未受影响 (与服务器一致)");
    }

    // ---- 测试 2: 失配检测 (状态破坏, 输入不变 → F2 抓不到, F3 抓到) ----
    {
        CPredictiveSim cli;
        CHECK(cli.Init(seed, pid), "Init");
        cli.MutableCore().MutableCollision().mapWidth = 1024;
        cli.MutableCore().MutableCollision().mapHeight = 640;

        // 预测到 24 帧 (checkpoint: 0/10/20), 然后在 checkpoint 20~30 之间破坏状态
        for (uint32_t f = 1; f <= 24; ++f) {
            std::vector<PlayerInput> pred; pred.push_back(p1[f]); pred.push_back(p2[f]);
            cli.PredictFrame(f, pred);
        }
        // 破坏: 直接把英雄 0 的血量改错 (模拟「同输入却漂移」, 输入流不变)
        CHECK(!cli.MutableCore().MutableHeroes().empty(), "存在英雄");
        cli.MutableCore().MutableHeroes()[0].hp = 999999;

        // 继续推进到 40 帧 (checkpoint 30/40 会把破坏后的状态存进去)
        for (uint32_t f = 25; f <= 40; ++f) {
            std::vector<PlayerInput> pred; pred.push_back(p1[f]); pred.push_back(p2[f]);
            cli.PredictFrame(f, pred);
        }
        // 确认帧 (完美预测 → F2 不回滚, 破坏得以保留)
        for (uint32_t f = 1; f <= 40; ++f) {
            ServerFrameCommand cmd;
            cmd.frame = f;
            cmd.inputs.push_back(p1[f]); cmd.inputs.push_back(p2[f]);
            cli.OnServerFrame(cmd);
        }
        CHECK(cli.RollbackCount() == 0, "F2 输入 diff 未触发回滚 (破坏未被 F2 发现)");

        // F3 哈希校验: 从 checkpoint 30 (已含破坏) 重放 → 与服务器哈希不一致
        CHECK(!cli.VerifyFrame(40, srvHash[40]), "F3 检出失配 (状态漂移)");
        CHECK(cli.HashMismatchCount() == 1, "失配计数 ==1 (实际 %u)", cli.HashMismatchCount());
        CHECK(cli.HashMismatched(), "HashMismatched 置位");
    }

    // ---- 测试 3: 快照纠偏恢复 ----
    {
        CPredictiveSim cli;
        CHECK(cli.Init(seed, pid), "Init");
        cli.MutableCore().MutableCollision().mapWidth = 1024;
        cli.MutableCore().MutableCollision().mapHeight = 640;

        // 破坏放在 checkpoint 20~30 之间 (同测试 2), 让 checkpoint 30 记下破坏,
        // VerifyFrame(40) 从 checkpoint 30 重放才会检出失配。
        for (uint32_t f = 1; f <= 24; ++f) {
            std::vector<PlayerInput> pred; pred.push_back(p1[f]); pred.push_back(p2[f]);
            cli.PredictFrame(f, pred);
        }
        cli.MutableCore().MutableHeroes()[0].hp = 999999;   // 破坏
        for (uint32_t f = 25; f <= 40; ++f) {
            std::vector<PlayerInput> pred; pred.push_back(p1[f]); pred.push_back(p2[f]);
            cli.PredictFrame(f, pred);
        }
        for (uint32_t f = 1; f <= 40; ++f) {
            ServerFrameCommand cmd;
            cmd.frame = f; cmd.inputs.push_back(p1[f]); cmd.inputs.push_back(p2[f]);
            cli.OnServerFrame(cmd);
        }
        CHECK(!cli.VerifyFrame(40, srvHash[40]), "检出失配");

        // 快照纠偏: 恢复到服务器 40 帧的权威状态
        CHECK(cli.ResyncFromSnapshot(40, srvSnap[40]), "快照恢复成功");
        CHECK(cli.ResyncCount() == 1, "纠偏计数 ==1 (实际 %u)", cli.ResyncCount());
        CHECK(cli.LastConfirmedFrame() == 40 && cli.PredictedFrame() == 40,
              "纠偏后确认/预测帧回退到 40");
        CHECK(cli.HashState() == srvHash[40], "纠偏后与服务器 40 帧状态一致");

        // 继续推进 41..N → 与服务器逐位一致 (漂移已被纠正)
        for (uint32_t f = 41; f <= N; ++f) {
            std::vector<PlayerInput> pred; pred.push_back(p1[f]); pred.push_back(p2[f]);
            cli.PredictFrame(f, pred);
            ServerFrameCommand cmd;
            cmd.frame = f; cmd.inputs.push_back(p1[f]); cmd.inputs.push_back(p2[f]);
            cli.OnServerFrame(cmd);
        }
        CHECK(cli.HashState() == srvHash[N], "纠偏后继续推进逐位一致");
        CHECK(cli.VerifyFrame(N, srvHash[N]), "纠偏后哈希校验一致");
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
