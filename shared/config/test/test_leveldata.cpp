// =============================================================================
// test_leveldata.cpp — C6 S5 收口: LevelDataLoader 从真实关卡 bin 注入内核 单测
//
// 覆盖:
//   1) 关卡 11 注入: 碰撞 1408x832 / people 29 / bullet 35 / 出生点 3 / 波次 1 /
//      出生组 3 / 小怪配置非空 / 炮台配置 6 (ptinfo_0..5) / 无炮台出生点。
//   2) 关卡 12 注入: 炮台出生点 1 (paota_point 有一处)。
//   3) 关卡 68 注入 (最多炮台 9): 首帧 SpawnInitialTurrets 出生 9 炮台 + 两端哈希一致。
//   4) 不存在关卡 → 注入失败 (碰撞缺失, 不部分注入)。
//
// 编译: build_leveldata.bat (MSVC, /std:c++14 /utf-8; 链接 BattleCore.cpp)。
// 运行: test_leveldata.exe [dataDir]  (默认 ../data, 即 shared/config/data/)
// =============================================================================
#include "../LevelDataLoader.h"
#include "../../battlecore/BattleCore.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace rsconfig;
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

static void Init(CBattleCore& c, uint32_t seed) {
    c.Init(seed, { 101 });
}

int main(int argc, char** argv) {
    const char* dir = (argc > 1) ? argv[1] : "../data";
    printf("== LevelDataLoader 真实 bin 注入单测 (dir=%s) ==\n", dir);

    // 测试 1: 关卡 11 (1408x832, people29/bullet35/entry3, 1 波 3 组, 无炮台)
    {
        CBattleCore c; Init(c, 0xB0B0);
        std::string err;
        bool ok = LevelDataLoader::InjectFromDir(c, dir, 11, 0, err);
        CHECK(ok, "关卡 11 注入成功 (err=%s)", err.c_str());
        const MapCollision& col = c.GetCollision();
        CHECK(col.mapWidth == 1408 && col.mapHeight == 832, "关卡 11 地图 1408x832 (实际 %dx%d)", col.mapWidth, col.mapHeight);
        CHECK(col.people.size() == 29, "关卡 11 people 矩形 29 (实际 %zu)", col.people.size());
        CHECK(col.bullet.size() == 35, "关卡 11 bullet 矩形 35 (实际 %zu)", col.bullet.size());
        CHECK(col.entries.size() == 3, "关卡 11 出生点 3 (实际 %zu)", col.entries.size());
        CHECK(c.GetWavePlan().size() == 1, "关卡 11 波次 1 (实际 %zu)", c.GetWavePlan().size());
        if (!c.GetWavePlan().empty())
            CHECK(c.GetWavePlan()[0].groups.size() == 3, "关卡 11 出生组 3 (实际 %zu)", c.GetWavePlan()[0].groups.size());
        CHECK(!c.GetSp01Config().empty(), "小怪配置非空 (实际 %zu)", c.GetSp01Config().size());
        CHECK(c.GetTurretConfig().size() == 6, "炮台配置 6 = ptinfo_0..5 (实际 %zu)", c.GetTurretConfig().size());
        CHECK(c.GetTurretSpawns().empty(), "关卡 11 无炮台 (实际 %zu)", c.GetTurretSpawns().size());
    }

    // 测试 2: 关卡 12 (paota_point 一处 → 1 炮台出生点)
    {
        CBattleCore c; Init(c, 0xB0B0);
        std::string err;
        bool ok = LevelDataLoader::InjectFromDir(c, dir, 12, 0, err);
        CHECK(ok, "关卡 12 注入成功 (err=%s)", err.c_str());
        CHECK(c.GetTurretSpawns().size() == 1, "关卡 12 炮台出生点 1 (实际 %zu)", c.GetTurretSpawns().size());
    }

    // 测试 3: 关卡 68 (最多炮台 9): 首帧全出生 + 两端哈希一致
    {
        CBattleCore c1, c2;
        Init(c1, 0xB0B0); Init(c2, 0xB0B0);
        std::string e1, e2;
        CHECK(LevelDataLoader::InjectFromDir(c1, dir, 68, 1, e1), "关卡 68 注入 (err=%s)", e1.c_str());
        CHECK(LevelDataLoader::InjectFromDir(c2, dir, 68, 1, e2), "关卡 68 注入 2 (err=%s)", e2.c_str());
        CHECK(c1.GetTurretSpawns().size() == 9, "关卡 68 炮台出生点 9 (实际 %zu)", c1.GetTurretSpawns().size());
        for (uint32_t f = 1; f <= 3; ++f) { c1.Tick(f); c2.Tick(f); }
        CHECK(c1.GetTurrets().size() == 9, "关卡 68 首帧出生 9 炮台 (实际 %zu)", c1.GetTurrets().size());
        CHECK(c1.HashState() == c2.HashState(), "同关卡同难度注入 + 3 帧后两端哈希一致");
    }

    // 测试 4: 不存在关卡 → 碰撞缺失, 注入失败且不部分注入
    {
        CBattleCore c; Init(c, 0xB0B0);
        std::string err;
        bool ok = LevelDataLoader::InjectFromDir(c, dir, 9999, 0, err);
        CHECK(!ok, "关卡 9999 注入应失败 (err=%s)", err.c_str());
        CHECK(c.GetCollision().empty(), "失败后碰撞保持空 (未部分注入)");
    }

    // 测试 5: InjectFromBlobs (客户端网络拉取路径) 与 InjectFromDir 逐位一致
    {
        // 读三份 bin → blobs (模拟网络拉取的完整 bundle 字节)
        const char* names[3] = { "wave.bin", "map_collision.bin", "sp01.bin" };
        std::vector<BinBlob> blobs;
        for (int i = 0; i < 3; ++i) {
            std::string path = std::string(dir) + "/" + names[i];
            FILE* f = std::fopen(path.c_str(), "rb");
            CHECK(f != nullptr, "读 bin %s", path.c_str());
            if (!f) continue;
            std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
            BinBlob b; b.name = names[i]; b.bytes.resize(n > 0 ? (size_t)n : 0);
            if (n > 0) std::fread(b.bytes.data(), 1, (size_t)n, f);
            std::fclose(f);
            blobs.push_back(std::move(b));
        }

        CBattleCore a, b;
        Init(a, 0xB0B0); Init(b, 0xB0B0);
        std::string ea, eb;
        CHECK(LevelDataLoader::InjectFromDir(a, dir, 11, 0, ea), "dir 注入 (err=%s)", ea.c_str());
        CHECK(LevelDataLoader::InjectFromBlobs(b, blobs, 11, 0, eb), "blob 注入 (err=%s)", eb.c_str());
        CHECK(a.GetCollision().mapWidth  == b.GetCollision().mapWidth &&
              a.GetCollision().mapHeight == b.GetCollision().mapHeight,
              "dir/blob 地图一致 (%dx%d vs %dx%d)",
              a.GetCollision().mapWidth, a.GetCollision().mapHeight,
              b.GetCollision().mapWidth, b.GetCollision().mapHeight);
        CHECK(a.GetWavePlan().size()   == b.GetWavePlan().size(),   "dir/blob 波次一致 (实际 %zu vs %zu)", a.GetWavePlan().size(), b.GetWavePlan().size());
        CHECK(a.GetSp01Config().size() == b.GetSp01Config().size(), "dir/blob 小怪配置一致 (实际 %zu vs %zu)", a.GetSp01Config().size(), b.GetSp01Config().size());
        CHECK(a.GetTurretConfig().size() == b.GetTurretConfig().size(), "dir/blob 炮台配置一致 (实际 %zu vs %zu)", a.GetTurretConfig().size(), b.GetTurretConfig().size());
        for (uint32_t f = 1; f <= 3; ++f) { a.Tick(f); b.Tick(f); }
        CHECK(a.HashState() == b.HashState(), "dir/blob 注入后 3 帧核心哈希一致");

        // 缺 bin → 注入失败
        CBattleCore c; Init(c, 0xB0B0);
        std::vector<BinBlob> partial = { blobs[0], blobs[1] };   // 缺 sp01.bin
        std::string ec;
        CHECK(!LevelDataLoader::InjectFromBlobs(c, partial, 11, 0, ec), "缺 bin → 注入失败 (err=%s)", ec.c_str());
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
