// test_probe_stale.cpp — 复刻真实冒烟的回滚节奏: 远端输入每 20 帧变一次方向,
// 本地对远端的预测 = 上一帧已知输入 → 只在 20 的倍数帧(== checkpoint 边界)发生回滚。
// 逐帧 VerifyFrame 比对中间已确认帧哈希, 探测 checkpoint 边界陈旧快照问题。
#include "../BattlePrediction.h"
#include <cstdio>
using namespace battlecore;

static PlayerInput In(int64_t pid, uint32_t dir, bool fire) {
    PlayerInput p; p.player_id = pid; p.move_dir = dir; p.fire = fire;
    DirToUnitVectorFx(dir, p.shoot_dx, p.shoot_dy);
    return p;
}
static void SetupCore(CBattleCore& c, uint32_t seed, const std::vector<int64_t>& ids) {
    c.Init(seed, ids); c.MutableCollision().mapWidth = 1024; c.MutableCollision().mapHeight = 640;
}

int main() {
    const uint32_t N = 120, DELAY = 2, seed = 0xABC;
    std::vector<int64_t> pid = { 101, 202 };
    std::vector<PlayerInput> p1(N + 1), p2(N + 1), g2(N + 1);
    for (uint32_t f = 1; f <= N; ++f) {
        p1[f] = In(101, 3, (f % 10) == 0);
        // p2 在 20 的倍数帧切换方向 (7/3 交替) — 复刻冒烟 kDirCycle[(frame/20)%N] 的边界
        uint32_t d = (f / 20) % 2 == 0 ? 7 : 3;
        p2[f] = In(202, d, false);
        // 本地预测远端 = 上一帧已知输入 → 只在 20 的倍数帧 (== checkpoint 边界) 猜错
        uint32_t gd = ((f - 1) / 20) % 2 == 0 ? 7 : 3;   // f-1 的方向
        if (f == 1) gd = 0;
        g2[f] = In(202, gd, false);
    }

    CPredictiveSim cli; cli.Init(seed, pid);
    cli.MutableCore().MutableCollision().mapWidth = 1024;
    cli.MutableCore().MutableCollision().mapHeight = 640;

    auto refHash = [&](uint32_t F) -> uint64_t {
        CBattleCore c; SetupCore(c, seed, pid);
        for (uint32_t f = 1; f <= F; ++f) { c.SubmitInput(p1[f]); c.SubmitInput(p2[f]); c.Tick(f); }
        return c.HashState();
    };

    for (uint32_t f = 1; f <= N; ++f) {
        std::vector<PlayerInput> pred; pred.push_back(p1[f]); pred.push_back(g2[f]);
        cli.PredictFrame(f, pred);
        if (f >= DELAY) {
            ServerFrameCommand cmd; cmd.frame = f - DELAY;
            cmd.inputs.push_back(p1[f - DELAY]); cmd.inputs.push_back(p2[f - DELAY]);
            cli.OnServerFrame(cmd);
        }
    }
    for (uint32_t f = N - DELAY + 1; f <= N; ++f) {
        ServerFrameCommand cmd; cmd.frame = f;
        cmd.inputs.push_back(p1[f]); cmd.inputs.push_back(p2[f]);
        cli.OnServerFrame(cmd);
    }

    printf("final hash match=%d rollback=%u lastConfirmed=%u\n",
           cli.HashState() == refHash(N), cli.RollbackCount(), cli.LastConfirmedFrame());

    int bad = 0;
    for (uint32_t F = 1; F <= N; ++F) {
        bool ok = cli.VerifyFrame(F, refHash(F));
        if (!ok) { if (bad < 25) printf("MISMATCH at confirmed frame=%u\n", F); ++bad; }
    }
    printf("total mismatches=%d\n", bad);
    return bad == 0 ? 0 : 1;
}
