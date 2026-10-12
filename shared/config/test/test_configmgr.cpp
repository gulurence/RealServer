// =============================================================================
// test_configmgr.cpp — 配置体系 C3 (CConfigMgr 多 bin 管理 + 原子替换 + 逐表 diff) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_configmgr.exe test_configmgr.cpp
// 运行: test_configmgr.exe  (退出码 0=通过)
//
// 验证点 (C3 纯核心部分; Redis 发布/订阅走 engine 侧适配器):
//   1) 加载多 bin → version/contentHash 正确, FindReader/ForEachId 命中, sink 收到首载通知。
//   2) 幂等重载: 相同内容重复 Load 不递增版本、不通知 (version 由调用方显式传入)。
//   3) 逐表 diff: 只改一张表 → changedTables 只含该表; 删一张表 → 含被删表名。
//   4) 原子性: 任一路径加载失败 → 保持旧状态, 不通知。
//   5) 确定性: 两管理器同 bundle → 聚合 contentHash 一致 (纯内容, 与路径无关)。
// =============================================================================
#include "../CConfigMgr.h"
#include "../CBinWriter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace rsconfig;

static int g_fail = 0;

#define CHECK(cond, ...)                                          \
    do {                                                          \
        if (!(cond)) {                                            \
            ++g_fail;                                             \
            printf("FAIL: "); printf(__VA_ARGS__); printf("\n");   \
            if (g_fail > 20) { printf("... 已超过 20 处失败, 提前退出\n"); exit(1); } \
        }                                                         \
    } while (0)

// ---- 样例表构造 (足够 C3 验证; 值变化用于制造 diff) ----
static TableData MakeSp01(uint32_t maxHp) {
    TableData t; t.name = "sp01";
    t.fields = { FieldDesc("id", kTypeI32), FieldDesc("maxHp", kTypeI32) };
    Row r; r.id = 1; r.cells.resize(2);
    r.cells[0].i = 1; r.cells[1].i = (int64_t)maxHp;
    t.rows = { r };
    return t;
}
static TableData MakeWave() {
    TableData t; t.name = "wave";
    t.fields = { FieldDesc("id", kTypeI32), FieldDesc("count", kTypeI32) };
    Row r; r.id = 1; r.cells.resize(2);
    r.cells[0].i = 1; r.cells[1].i = 5;
    t.rows = { r };
    return t;
}

// 把多张表写成一个 bin 文件, 返回文件路径 (写到测试目录, 便于 CConfigMgr::LoadFile 读)
static std::string WriteBin(const std::string& name, const std::vector<TableData>& tables) {
    std::vector<uint8_t> b; std::string err;
    BuildBin(tables, b, err);
    std::string path = "tmp_cfgmgr_" + name + ".bin";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(b.data(), 1, b.size(), f); std::fclose(f); }
    return path;
}

// 内存 fake 变更接收端 (模拟 Redis 订阅端 / 客户端拉取端)
struct FakeSink : IConfigChangeSink {
    std::vector<ConfigChange> changes;
    void OnConfigChanged(const ConfigChange& ch) override { changes.push_back(ch); }
};

static bool Contains(const std::vector<std::string>& v, const char* s) {
    for (const auto& x : v) if (x == s) return true;
    return false;
}

int main() {
    printf("== 配置体系 C3 (CConfigMgr 多 bin 管理) 单元测试 ==\n");

    // 一份 bundle: sp01.bin (sp01 表) + wave.bin (wave 表)
    std::string sp01Path = WriteBin("sp01", { MakeSp01(100) });
    std::string wavePath = WriteBin("wave", { MakeWave() });
    std::vector<std::string> bundle = { sp01Path, wavePath };

    // ---- 测试 1: 首载 ----
    {
        CConfigMgr mgr;
        FakeSink sink;
        mgr.SetSink(&sink);
        std::string err;
        CHECK(mgr.Load(bundle, 1, err), "首载成功 (%s)", err.c_str());
        CHECK(mgr.Version() == 1, "首载 version==1 (实际 %u)", mgr.Version());
        CHECK(mgr.ContentHash() != 0, "contentHash != 0");
        CHECK(!mgr.Empty(), "非空");

        CHECK(mgr.FindReader("sp01") != nullptr, "FindReader(sp01) 命中");
        CHECK(mgr.FindReader("wave") != nullptr, "FindReader(wave) 命中");
        CHECK(mgr.FindReader("nope") == nullptr, "FindReader(nope) 不存在");

        std::vector<uint32_t> ids;
        mgr.ForEachId("sp01", [&](uint32_t id) { ids.push_back(id); });
        CHECK(ids.size() == 1 && ids[0] == 1, "sp01 ForEachId == [1]");

        CHECK(sink.changes.size() == 1, "首载通知 1 次 (实际 %zu)", sink.changes.size());
        if (sink.changes.size() == 1) {
            CHECK(sink.changes[0].version == 1, "通知 version==1");
            CHECK(sink.changes[0].contentHash == mgr.ContentHash(), "通知 contentHash 一致");
            CHECK(Contains(sink.changes[0].changedTables, "sp01") &&
                  Contains(sink.changes[0].changedTables, "wave"),
                  "首载 changedTables 含 sp01+wave");
        }
    }

    // ---- 测试 2: 幂等重载 (相同内容) ----
    {
        CConfigMgr mgr;
        FakeSink sink;
        mgr.SetSink(&sink);
        std::string err;
        mgr.Load(bundle, 1, err);
        mgr.Load(bundle, 1, err);
        CHECK(mgr.Version() == 1, "幂等重载 version 仍 ==1 (实际 %u)", mgr.Version());
        CHECK(sink.changes.size() == 1, "幂等重载不重复通知 (实际 %zu)", sink.changes.size());
    }

    // ---- 测试 3: 逐表 diff (只改 sp01) ----
    {
        CConfigMgr mgr;
        FakeSink sink;
        mgr.SetSink(&sink);
        std::string err;
        mgr.Load(bundle, 1, err);

        std::string sp01v2 = WriteBin("sp01_v2", { MakeSp01(200) });  // maxHp 100→200
        std::vector<std::string> bundle2 = { sp01v2, wavePath };
        CHECK(mgr.Load(bundle2, 2, err), "改 sp01 重载成功 (%s)", err.c_str());
        CHECK(mgr.Version() == 2, "重载 version==2 (实际 %u)", mgr.Version());
        CHECK(sink.changes.size() == 2, "重载通知 1 次 (实际 %zu)", sink.changes.size());
        if (sink.changes.size() == 2) {
            CHECK(sink.changes[1].changedTables.size() == 1, "changedTables 只有 1 项 (实际 %zu)",
                  sink.changes[1].changedTables.size());
            CHECK(sink.changes[1].changedTables.size() == 1 &&
                  sink.changes[1].changedTables[0] == "sp01", "changedTables == [sp01]");
        }

        // 删表: 只加载 wave → sp01 应出现在 changedTables (删除也算变更)
        std::vector<std::string> bundle3 = { wavePath };
        CHECK(mgr.Load(bundle3, 3, err), "删 sp01 重载成功 (%s)", err.c_str());
        CHECK(mgr.Version() == 3, "删表 version==3 (实际 %u)", mgr.Version());
        CHECK(sink.changes.size() == 3, "删表通知 1 次");
        if (sink.changes.size() == 3)
            CHECK(Contains(sink.changes[2].changedTables, "sp01"), "删表 changedTables 含 sp01");
        CHECK(mgr.FindReader("sp01") == nullptr, "删表后 FindReader(sp01)==nullptr");
    }

    // ---- 测试 4: 原子性 (任一路径失败 → 保持旧状态) ----
    {
        CConfigMgr mgr;
        FakeSink sink;
        mgr.SetSink(&sink);
        std::string err;
        mgr.Load(bundle, 1, err);
        uint64_t hashBefore = mgr.ContentHash();
        uint32_t verBefore = mgr.Version();
        size_t notifyBefore = sink.changes.size();

        std::vector<std::string> badBundle = { sp01Path, "tmp_cfgmgr_no_such.bin" };
        CHECK(!mgr.Load(badBundle, 2, err), "含坏路径的 Load 返回 false");
        CHECK(mgr.Version() == verBefore, "失败后 version 不变");
        CHECK(mgr.ContentHash() == hashBefore, "失败后 contentHash 不变");
        CHECK(mgr.FindReader("sp01") != nullptr && mgr.FindReader("wave") != nullptr,
              "失败后旧表仍可读");
        CHECK(sink.changes.size() == notifyBefore, "失败后不通知");
    }

    // ---- 测试 5: 确定性 (两管理器同 bundle → 同聚合 hash) ----
    {
        CConfigMgr a, b;
        std::string err;
        CHECK(a.Load(bundle, 1, err) && b.Load(bundle, 1, err), "两管理器同 bundle 载入成功");
        CHECK(a.ContentHash() == b.ContentHash(), "聚合 contentHash 一致");
        CHECK(a.Version() == b.Version(), "version 一致");
    }

    // 清理临时 bin
    std::remove(sp01Path.c_str());
    std::remove(wavePath.c_str());
    std::remove("tmp_cfgmgr_sp01_v2.bin");

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
