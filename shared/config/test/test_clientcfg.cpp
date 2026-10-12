// =============================================================================
// test_clientcfg.cpp — 配置体系 C4 (CClientConfigMgr 客户端拉取热更) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_clientcfg.exe test_clientcfg.cpp
// 运行: test_clientcfg.exe  (退出码 0=通过)
//
// 验证点 (C4 纯核心部分; 真实连接层/HTTP 下载随客户端工作流接入):
//   1) 本地加载 v1 + 收到 v2 通知 → 判定需要拉取。
//   2) 收到相同/更旧版本通知 → 忽略 (防降级)。
//   3) 拉取成功: 校验 contentHash 后原子替换到 v2, 配置生效。
//   4) 拉取字节哈希不符 (下载损坏/版本错乱) → 拒载, 保持旧配置 (回滚)。
//   5) 传输失败 → 拒载, 保持旧配置 (回滚)。
//   6) 聚合哈希确定性 (同字节两次一致) + 内容敏感性 (maxHp 100→200 哈希必变)。
// =============================================================================
#include "../CClientConfigMgr.h"
#include "../CBinWriter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

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

// ---- 样例表构造 ----
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

// 把多张表写成一个 bin blob (内存)
static BinBlob MakeBlob(const std::string& name, const std::vector<TableData>& tables) {
    BinBlob b; b.name = name;
    std::string err;
    BuildBin(tables, b.bytes, err);
    return b;
}

// 把 blob 落盘 (供 LoadLocal 读)
static std::string WriteBlob(const BinBlob& b, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(b.bytes.data(), 1, b.bytes.size(), f); std::fclose(f); }
    return path;
}

// 内存 fake 传输层: version → blobs (模拟服务器 CDN / 连接层分块下载)
struct FakeTransport : IConfigTransport {
    std::map<uint32_t, std::vector<BinBlob>> versions;
    bool fail = false;   // 模拟传输失败
    bool Pull(uint32_t version, std::vector<BinBlob>& out, std::string& err) override {
        if (fail) { err = "network error"; return false; }
        auto it = versions.find(version);
        if (it == versions.end()) { err = "no such version"; return false; }
        out = it->second;
        return true;
    }
};

// 从 blob 里读 sp01.maxHp 值 (用 CBinReader + FindRow 校验配置确实换新了)
static int64_t ReadSp01MaxHp(const CClientConfigMgr& mgr) {
    const CBinReader* r = mgr.Mgr().FindReader("sp01");
    if (!r) return -1;
    uint32_t len = 0;
    const uint8_t* row = r->FindRow("sp01", 1, &len);
    if (!row) return -1;
    // schema: [fieldCount][nameOff|type][nameOff|type]; data: id(i32) + maxHp(i32)
    RowCursor cur(row, len);
    cur.U32();           // id
    return cur.I32();    // maxHp
}

int main() {
    printf("== 配置体系 C4 (CClientConfigMgr 客户端拉取热更) 单元测试 ==\n");

    // v1 (本地打包): sp01.maxHp=100 + wave; v2 (服务器): sp01.maxHp=200 + wave
    BinBlob sp01v1 = MakeBlob("sp01.bin", { MakeSp01(100) });
    BinBlob wavev1 = MakeBlob("wave.bin", { MakeWave() });
    BinBlob sp01v2 = MakeBlob("sp01.bin", { MakeSp01(200) });
    BinBlob wavev2 = MakeBlob("wave.bin", { MakeWave() });

    std::string sp01v1Path = WriteBlob(sp01v1, "tmp_client_sp01v1.bin");
    std::string wavev1Path = WriteBlob(wavev1, "tmp_client_wavev1.bin");
    std::vector<std::string> local = { sp01v1Path, wavev1Path };

    std::vector<BinBlob> v2 = { sp01v2, wavev2 };
    uint64_t v2Hash = CConfigMgr::AggregateHash(v2);   // 服务器侧 CConfigMgr 算出并广播的哈希

    // ---- 测试 1: 本地加载 v1 + 收到 v2 通知 ----
    {
        CClientConfigMgr c;
        std::string err;
        CHECK(c.LoadLocal(local, 1, err), "本地加载 v1 成功 (%s)", err.c_str());
        CHECK(c.Version() == 1, "本地 version==1");
        CHECK(ReadSp01MaxHp(c) == 100, "本地 sp01.maxHp==100");

        CHECK(c.OnNotify(2, v2Hash), "v2 通知 → 需要拉取");
        CHECK(c.PendingVersion() == 2, "待拉取版本==2");
    }

    // ---- 测试 2: 相同/更旧版本通知 → 忽略 ----
    {
        CClientConfigMgr c;
        std::string err;
        c.LoadLocal(local, 1, err);
        CHECK(!c.OnNotify(1, v2Hash), "相同版本通知 → 不拉");
        CHECK(!c.OnNotify(0, v2Hash), "更旧版本通知 → 不拉");
        CHECK(c.PendingVersion() == 0, "无待拉取版本");
    }

    // ---- 测试 3: 拉取成功 → 校验哈希 → 原子替换 ----
    {
        CClientConfigMgr c;
        FakeTransport t; t.versions[2] = v2;
        std::string err;
        c.LoadLocal(local, 1, err);
        c.OnNotify(2, v2Hash);

        CHECK(c.ApplyPull(t, err), "拉取成功 (%s)", err.c_str());
        CHECK(c.Version() == 2, "替换后 version==2 (实际 %u)", c.Version());
        CHECK(c.ContentHash() == v2Hash, "替换后 contentHash==通知哈希");
        CHECK(ReadSp01MaxHp(c) == 200, "sp01.maxHp 已换新 ==200");
        CHECK(c.PendingVersion() == 0, "消费后待拉取清空");
    }

    // ---- 测试 4: 拉取字节哈希不符 (下载损坏/版本错乱) → 回滚 ----
    {
        CClientConfigMgr c;
        FakeTransport t;
        std::string err;
        c.LoadLocal(local, 1, err);

        // 服务器通知的哈希与拉到的字节不一致 (模拟串版本/损坏)
        BinBlob corrupt = sp01v2; corrupt.bytes[0] ^= 0xFF;   // 破坏魔数 → 不可载
        std::vector<BinBlob> bad = { corrupt, wavev2 };
        t.versions[2] = bad;

        CHECK(c.OnNotify(2, v2Hash), "通知 v2");
        CHECK(!c.ApplyPull(t, err), "哈希不符 → 拒载 (%s)", err.c_str());
        CHECK(c.Version() == 1, "回滚后 version 仍 ==1 (实际 %u)", c.Version());
        CHECK(ReadSp01MaxHp(c) == 100, "回滚后 sp01.maxHp 仍 ==100");
    }

    // ---- 测试 5: 传输失败 → 回滚 ----
    {
        CClientConfigMgr c;
        FakeTransport t; t.fail = true; t.versions[2] = v2;
        std::string err;
        c.LoadLocal(local, 1, err);
        c.OnNotify(2, v2Hash);
        CHECK(!c.ApplyPull(t, err), "传输失败 → 拒载 (%s)", err.c_str());
        CHECK(c.Version() == 1, "传输失败后 version 仍 ==1");
        CHECK(ReadSp01MaxHp(c) == 100, "传输失败后配置不变");
    }

    // ---- 测试 6: 聚合哈希确定性 + 内容敏感性 ----
    {
        uint64_t h1 = CConfigMgr::AggregateHash(v2);
        uint64_t h2 = CConfigMgr::AggregateHash(v2);
        CHECK(h1 != 0, "聚合哈希非 0");
        CHECK(h1 == h2, "同 bundle 两次哈希一致 (确定性)");

        std::vector<BinBlob> v1blobs = { sp01v1, wavev1 };
        uint64_t hOld = CConfigMgr::AggregateHash(v1blobs);
        CHECK(hOld != h1, "内容不同 (maxHp 100→200) → 聚合哈希不同");
    }

    // ---- 测试 7: ReceiveBundle (服务器直接下发完整 bundle, 不走通知/拉取) ----
    {
        CClientConfigMgr c;
        std::string err;
        c.LoadLocal(local, 1, err);

        CHECK(c.ReceiveBundle(2, v2Hash, v2, err), "ReceiveBundle 成功 (%s)", err.c_str());
        CHECK(c.Version() == 2, "ReceiveBundle 后 version==2 (实际 %u)", c.Version());
        CHECK(c.ContentHash() == v2Hash, "ReceiveBundle 后 contentHash==下发哈希");
        CHECK(ReadSp01MaxHp(c) == 200, "ReceiveBundle 后 sp01.maxHp==200");

        // 哈希不符 (损坏/串版本) → 拒载回滚
        BinBlob corrupt = sp01v2; corrupt.bytes[0] ^= 0xFF;
        std::vector<BinBlob> bad = { corrupt, wavev2 };
        CClientConfigMgr c2;
        c2.LoadLocal(local, 1, err);
        CHECK(!c2.ReceiveBundle(2, v2Hash, bad, err), "ReceiveBundle 哈希不符拒载 (%s)", err.c_str());
        CHECK(c2.Version() == 1, "ReceiveBundle 拒载后 version 仍 ==1 (实际 %u)", c2.Version());
        CHECK(ReadSp01MaxHp(c2) == 100, "ReceiveBundle 拒载后 sp01.maxHp 仍 ==100");
    }

    // 清理临时文件
    std::remove(sp01v1Path.c_str());
    std::remove(wavev1Path.c_str());

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
