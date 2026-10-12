// =============================================================================
// test_battleprediction.cpp — 客户端预测 + 回滚和解 (F1/F2) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_battleprediction.exe test_battleprediction.cpp ..\BattleCore.cpp ..\BattleGame.cpp
// 运行: test_battleprediction.exe  (退出码 0=通过)
//
// 验证点 (frame-sync-client-framework.md §4.3):
//   F1) 客户端预测: 本地核心立即消费输入推进, 超前服务器 DELAY 帧 (run-ahead)。
//   F2) 回滚和解: 服务器帧命令与本地预测 diff, 不一致 → 回滚 checkpoint + 按权威重放,
//       最终与服务器权威核心状态逐位一致; 无和解的本地预测必然漂移 (对照组)。
//   F2) 完美预测: 本地预测 == 服务器命令时不回滚。
//   m_nextId) 快照恢复后继续推进, 实体 id 分配一致 (回滚重放不漂 id)。
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

static Sp01State MakeDropper(uint32_t id, int32_t x, int32_t y, int32_t hp) {
    Sp01State s;
    s.id = id; s.classId = kClassSp01Zombie; s.idInSp01Info = 0;
    s.x = FxFromInt(x); s.y = FxFromInt(y); s.radians = 0;
    s.hp = hp; s.maxHp = 10000; s.hpPre = hp;
    s.action = kSp01Walk; s.animDir = 0; s.remove = false;
    s.walkSpeed = FxFromInt(5); s.hurtToHero = FxFromInt(1);
    s.dropCoinsProbability = 100; s.dropCoinsMax = 3; s.dropCoinsMin = 3;
    s.dropSunShinePro = 0; s.dropSunShineMax = 0; s.dropSunShineMin = 0;
    return s;
}

int main() {
    printf("== 客户端预测 + 回滚和解 (F1/F2) 单元测试 ==\n");

    const uint32_t N = 120;        // 总帧数
    const uint32_t DELAY = 3;      // 服务器帧命令落后本地预测的帧数
    const uint32_t seed = 0xABC;
    std::vector<int64_t> pid = { 101, 202 };

    // 输入脚本: p1 一直右移(3), 每 10 帧开火; p2 一直左移(7), 每 15 帧开火;
    // 客户端对 p2 的预测 = 一直停止(0) → 每帧都错 → 持续触发回滚。
    std::vector<PlayerInput> p1(N + 1), p2(N + 1), p2guess(N + 1);
    for (uint32_t f = 1; f <= N; ++f) {
        p1[f] = In(101, 3, (f % 10) == 0);
        p2[f] = In(202, 7, (f % 15) == 0);
        p2guess[f] = In(202, 0, false);
    }

    // ---- 权威服务器核心 ----
    CBattleCore srv;
    SetupCore(srv, seed, pid);
    for (uint32_t f = 1; f <= N; ++f) { srv.SubmitInput(p1[f]); srv.SubmitInput(p2[f]); srv.Tick(f); }
    uint64_t srvHash = srv.HashState();

    // ---- 对照组: 无和解的本地预测 (p2 用猜值) → 必然漂移 ----
    {
        CBattleCore ctrl;
        SetupCore(ctrl, seed, pid);
        for (uint32_t f = 1; f <= N; ++f) { ctrl.SubmitInput(p1[f]); ctrl.SubmitInput(p2guess[f]); ctrl.Tick(f); }
        CHECK(ctrl.HashState() != srvHash, "对照组: 无和解的本地预测必然漂移 (hash 不同)");
    }

    // ---- 测试 1: F1 预测 + F2 回滚和解 ----
    {
        CPredictiveSim cli;
        CHECK(cli.Init(seed, pid), "CPredictiveSim.Init");
        cli.MutableCore().MutableCollision().mapWidth = 1024;
        cli.MutableCore().MutableCollision().mapHeight = 640;

        for (uint32_t f = 1; f <= N; ++f) {
            std::vector<PlayerInput> pred;
            pred.push_back(p1[f]); pred.push_back(p2guess[f]);
            cli.PredictFrame(f, pred);
            if (f >= DELAY) {
                ServerFrameCommand cmd;
                cmd.frame = f - DELAY;
                cmd.inputs.push_back(p1[f - DELAY]);
                cmd.inputs.push_back(p2[f - DELAY]);
                cli.OnServerFrame(cmd);
            }
        }
        // F1: 本地已预测到 N, 但只确认到 N-DELAY → 超前 DELAY 帧 (run-ahead)
        CHECK(cli.PredictedFrame() == N && cli.LastConfirmedFrame() == N - DELAY,
              "F1 run-ahead: 预测到 %u, 确认到 %u (超前 %u 帧)",
              cli.PredictedFrame(), cli.LastConfirmedFrame(), DELAY);

        // 送完剩余服务器帧 → 全部确认
        for (uint32_t f = N - DELAY + 1; f <= N; ++f) {
            ServerFrameCommand cmd;
            cmd.frame = f;
            cmd.inputs.push_back(p1[f]); cmd.inputs.push_back(p2[f]);
            cli.OnServerFrame(cmd);
        }
        CHECK(cli.LastConfirmedFrame() == N, "F2 全部帧确认 (实际 %u)", cli.LastConfirmedFrame());
        CHECK(cli.RollbackCount() > 0, "F2 发生过回滚和解 (次数 %u)", cli.RollbackCount());
        CHECK(cli.HashState() == srvHash, "F2 和解后与服务器权威状态逐位一致");
    }

    // ---- 测试 2: 完美预测 (p2 猜对) → 不回滚 ----
    {
        CPredictiveSim cli2;
        CHECK(cli2.Init(seed, pid), "CPredictiveSim2.Init");
        cli2.MutableCore().MutableCollision().mapWidth = 1024;
        cli2.MutableCore().MutableCollision().mapHeight = 640;

        for (uint32_t f = 1; f <= N; ++f) {
            std::vector<PlayerInput> pred;
            pred.push_back(p1[f]); pred.push_back(p2[f]);   // 完美预测
            cli2.PredictFrame(f, pred);
            if (f >= DELAY) {
                ServerFrameCommand cmd;
                cmd.frame = f - DELAY;
                cmd.inputs.push_back(p1[f - DELAY]);
                cmd.inputs.push_back(p2[f - DELAY]);
                cli2.OnServerFrame(cmd);
            }
        }
        for (uint32_t f = N - DELAY + 1; f <= N; ++f) {
            ServerFrameCommand cmd;
            cmd.frame = f;
            cmd.inputs.push_back(p1[f]); cmd.inputs.push_back(p2[f]);
            cli2.OnServerFrame(cmd);
        }
        CHECK(cli2.RollbackCount() == 0, "完美预测零回滚 (实际 %u)", cli2.RollbackCount());
        CHECK(cli2.HashState() == srvHash, "完美预测状态一致");
    }

    // ---- 测试 3: m_nextId 快照恢复后继续推进仍一致 (回滚重放不漂 id) ----
    {
        CBattleCore A;
        SetupCore(A, seed, { 101 });
        A.MutableSp01s().push_back(MakeDropper(1, 400, 240, 1));
        A.HurtSp01ById(1, kClassBulletNormal);   // 打死 → 掉落 3 金币 → m_nextId 前进
        for (uint32_t f = 1; f <= 20; ++f) A.Tick(f);
        CHECK(!A.GetProps().empty(), "A 产生掉落道具 (m_nextId 已前进)");

        std::string snap = A.SerializeState();
        uint64_t hashA = A.HashState();

        CBattleCore B;
        SetupCore(B, seed, { 101 });
        CHECK(B.DeserializeState(snap), "DeserializeState 恢复快照");
        CHECK(B.HashState() == hashA, "恢复后哈希一致");

        // 两边各注入第二个怪(同 id), 打死 → 再掉落 → m_nextId 必须一致
        A.MutableSp01s().push_back(MakeDropper(2, 500, 240, 1));
        B.MutableSp01s().push_back(MakeDropper(2, 500, 240, 1));
        A.HurtSp01ById(2, kClassBulletNormal);
        B.HurtSp01ById(2, kClassBulletNormal);
        for (uint32_t f = 21; f <= 40; ++f) { A.Tick(f); B.Tick(f); }
        CHECK(A.HashState() == B.HashState(), "m_nextId 恢复后继续推进逐位一致");
    }

    // ---- 测试 4: 换枪 (gun_type) 分歧 → 必须触发回滚 (InputsEqual 逐字段含 gun_type) ----
    {
        // 纯函数校验: 仅 gun_type 不同 → 不得判等。否则本地用陈旧的枪械类别, 子弹类别/伤害
        // 与服务器漂移, 进而小怪死亡时机/波次推进不一致 (即「怪物移动和攻击不同步」)。
        PlayerInput ga = In(202, 3, true); ga.gun_type = 0;
        PlayerInput gb = In(202, 3, true); gb.gun_type = 1;
        std::vector<PlayerInput> va; va.push_back(ga);
        std::vector<PlayerInput> vb; vb.push_back(gb);
        CHECK(!InputsEqual(va, vb), "InputsEqual: 仅 gun_type 不同应判不等");

        // 端到端: p2 每 10 帧开火且枪械类别=1(机枪); 客户端对 p2 预测 gun_type=0(手枪),
        // 其余字段一致 → 修复前 InputsEqual 误判相等不触发回滚, 修复后逐帧回滚并对齐。
        std::vector<PlayerInput> g1(N + 1), g2(N + 1), g2guess(N + 1);
        for (uint32_t f = 1; f <= N; ++f) {
            g1[f] = In(101, 3, false);
            g2[f] = In(202, 3, (f % 10) == 0); g2[f].gun_type = 1;
            g2guess[f] = In(202, 3, (f % 10) == 0); g2guess[f].gun_type = 0;   // 猜错枪械类别
        }

        CBattleCore gsrv;
        SetupCore(gsrv, seed, pid);
        for (uint32_t f = 1; f <= N; ++f) { gsrv.SubmitInput(g1[f]); gsrv.SubmitInput(g2[f]); gsrv.Tick(f); }
        uint64_t gsrvHash = gsrv.HashState();

        CPredictiveSim gcli;
        CHECK(gcli.Init(seed, pid), "gun_type: CPredictiveSim.Init");
        gcli.MutableCore().MutableCollision().mapWidth = 1024;
        gcli.MutableCore().MutableCollision().mapHeight = 640;
        for (uint32_t f = 1; f <= N; ++f) {
            std::vector<PlayerInput> pred;
            pred.push_back(g1[f]); pred.push_back(g2guess[f]);
            gcli.PredictFrame(f, pred);
            if (f >= DELAY) {
                ServerFrameCommand cmd;
                cmd.frame = f - DELAY;
                cmd.inputs.push_back(g1[f - DELAY]); cmd.inputs.push_back(g2[f - DELAY]);
                gcli.OnServerFrame(cmd);
            }
        }
        for (uint32_t f = N - DELAY + 1; f <= N; ++f) {
            ServerFrameCommand cmd;
            cmd.frame = f;
            cmd.inputs.push_back(g1[f]); cmd.inputs.push_back(g2[f]);
            gcli.OnServerFrame(cmd);
        }
        CHECK(gcli.RollbackCount() > 0, "gun_type 分歧触发回滚 (次数 %u)", gcli.RollbackCount());
        CHECK(gcli.HashState() == gsrvHash, "gun_type 回滚和解后状态一致");
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
