// =============================================================================
// test_config.cpp — 配置体系 (Excel→bin + CBinReader/CConfigTable) 单元测试
//
// 编译: cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_config.exe test_config.cpp
// 运行: test_config.exe  (退出码 0=通过)
//
// 验证点 (配置体系 C1/C2):
//   1) 确定性/幂等: BuildBin 同输入两次 → 逐字节一致; content_hash 稳定。
//   2) 往返: CBinWriter → CBinReader, 随机访问/字段类型/字符串表/索引升序一致。
//   3) 坏包拒载: 翻转一个字节 → content_hash 校验失败拒载。
//   4) TSV 导出: ExportTables 解析 TSV → bin + 生成头; 生成头可编译并解码正确。
//   5) 强类型表: CConfigTable<Sp01Info>/<WaveInfo> 按 id 取行, fx/数组/字符串正确。
// =============================================================================
#include "../ConfigExporter.h"
#include "../CConfigTable.h"
#include "gen_sample.h"

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

// 直接构造表 (绕过 TSV, 测试 CBinWriter/Reader 底层)
static TableData MakeSp01Table() {
    TableData t; t.name = "sp01";
    t.fields = {
        FieldDesc("id", kTypeI32),
        FieldDesc("name", kTypeStr),
        FieldDesc("classId", kTypeI32),
        FieldDesc("maxHp", kTypeI32),
        FieldDesc("walkSpeed", kTypeFx),
        FieldDesc("hurtToHero", kTypeI32),
        FieldDesc("dropTable", kTypeArrI32),
    };
    Row r1; r1.id = 1; r1.cells.resize(7);
    r1.cells[0].i = 1; r1.cells[1].s = "僵尸"; r1.cells[2].i = 0;
    r1.cells[3].i = 100; r1.cells[4].i = 327680; r1.cells[5].i = 1;
    r1.cells[6].a = { 1001, 1002 };
    Row r2; r2.id = 2; r2.cells.resize(7);
    r2.cells[0].i = 2; r2.cells[1].s = "守卫"; r2.cells[2].i = 10;
    r2.cells[3].i = 300; r2.cells[4].i = 229376; r2.cells[5].i = 3;
    r2.cells[6].a = { 1003 };
    t.rows = { r1, r2 };
    return t;
}

static TableData MakeWaveTable() {
    TableData t; t.name = "wave";
    t.fields = {
        FieldDesc("id", kTypeI32),
        FieldDesc("sp01Id", kTypeI32),
        FieldDesc("count", kTypeI32),
        FieldDesc("interval", kTypeI32),
    };
    Row r1; r1.id = 1; r1.cells.resize(4);
    r1.cells[0].i = 1; r1.cells[1].i = 1; r1.cells[2].i = 5; r1.cells[3].i = 60;
    Row r2; r2.id = 2; r2.cells.resize(4);
    r2.cells[0].i = 2; r2.cells[1].i = 2; r2.cells[2].i = 3; r2.cells[3].i = 120;
    t.rows = { r1, r2 };
    return t;
}

// 样例 TSV (Excel「另存为 制表符分隔文本」的产物)
static const char* kSp01Tsv =
    "id\tname\tclassId\tmaxHp\twalkSpeed\thurtToHero\tdropTable\n"
    "i32\tstr\ti32\ti32\tfx\ti32\tarr_i32\n"
    "小怪ID\t名字\t职业\t最大生命\t移速\t碰撞伤害\t掉落表\n"
    "1\t僵尸\t0\t100\t5.0\t1\t1001,1002\n"
    "2\t守卫\t10\t300\t3.5\t3\t1003\n";

static const char* kWaveTsv =
    "id\tsp01Id\tcount\tinterval\n"
    "i32\ti32\ti32\ti32\n"
    "波次ID\t小怪\t数量\t间隔\n"
    "1\t1\t5\t60\n"
    "2\t2\t3\t120\n";

int main() {
    printf("== 配置体系 (Excel→bin + CBinReader/CConfigTable) 单元测试 ==\n");

    // ---- 测试 1: BuildBin 幂等 + 底层往返 ----
    {
        std::vector<TableData> tables = { MakeSp01Table(), MakeWaveTable() };
        std::vector<uint8_t> b1, b2; std::string err;
        CHECK(BuildBin(tables, b1, err), "BuildBin 成功 (%s)", err.c_str());
        CHECK(BuildBin(tables, b2, err), "BuildBin 第二次成功 (%s)", err.c_str());
        CHECK(b1 == b2, "BuildBin 同输入两次逐字节一致 (幂等)");
        CHECK(!b1.empty() && b1.size() >= (size_t)kHeaderSize, "bin 非空");

        CBinReader r;
        CHECK(r.Load(b1.data(), b1.size()), "CBinReader.Load (%s)", r.Error());
        CHECK(r.Ok(), "reader Ok");
        CHECK(r.Version() == kBinVersion, "version == %u", kBinVersion);
        CHECK(r.TableCount() == 2, "tableCount == 2 (实际 %u)", r.TableCount());
        CHECK(r.TableName(0) && std::strcmp(r.TableName(0), "sp01") == 0, "TableName(0)==sp01");
        CHECK(r.TableName(1) && std::strcmp(r.TableName(1), "wave") == 0, "TableName(1)==wave");

        // 字段/schema
        CHECK(r.FieldCount("sp01") == 7, "sp01 fieldCount==7 (实际 %u)", r.FieldCount("sp01"));
        CHECK(r.FieldTypeAt("sp01", 4) == kTypeFx, "sp01[4] 类型 == fx");
        CHECK(r.FieldTypeAt("sp01", 6) == kTypeArrI32, "sp01[6] 类型 == arr_i32");
        CHECK(r.FieldNameAt("sp01", 3) && std::strcmp(r.FieldNameAt("sp01", 3), "maxHp") == 0, "sp01[3] 名 == maxHp");

        // 随机访问 + 逐字段解码
        uint32_t len = 0;
        const uint8_t* row = r.FindRow("sp01", 1, &len);
        CHECK(row != nullptr && len > 0, "FindRow(sp01,1) 命中");
        if (row) {
            RowCursor c(row, len);
            CHECK(c.U32() == 1, "行 id == 1");
            const char* nm = r.GetString(c.U32());
            CHECK(nm && std::strcmp(nm, "僵尸") == 0, "name == 僵尸 (实际 %s)", nm ? nm : "<null>");
            CHECK(c.I32() == 0, "classId == 0");
            CHECK(c.I32() == 100, "maxHp == 100");
            CHECK(c.I32() == 327680, "walkSpeed == 327680 (5.0<<16)");
            CHECK(c.I32() == 1, "hurtToHero == 1");
            CHECK(c.U32() == 2, "dropTable 长度 == 2");
            CHECK(c.I32() == 1001 && c.I32() == 1002, "dropTable == [1001,1002]");
            CHECK(c.ok, "行解码无越界");
        }

        row = r.FindRow("sp01", 2, &len);
        CHECK(row != nullptr, "FindRow(sp01,2) 命中");
        if (row) {
            RowCursor c(row, len);
            c.U32();
            const char* nm = r.GetString(c.U32());
            CHECK(nm && std::strcmp(nm, "守卫") == 0, "row2 name == 守卫");
            CHECK(c.I32() == 10, "row2 classId == 10");
            CHECK(c.I32() == 300, "row2 maxHp == 300");
            CHECK(c.I32() == 229376, "row2 walkSpeed == 229376 (3.5<<16)");
            c.I32();  // hurtToHero
            CHECK(c.U32() == 1, "row2 dropTable 长度 == 1");
            CHECK(c.I32() == 1003, "row2 dropTable == [1003]");
        }

        CHECK(r.FindRow("sp01", 999, &len) == nullptr, "FindRow(sp01,999) 不存在");

        // 索引升序遍历
        std::vector<uint32_t> ids;
        r.ForEachId("sp01", [&](uint32_t id) { ids.push_back(id); });
        CHECK(ids.size() == 2 && ids[0] == 1 && ids[1] == 2, "sp01 ForEachId 升序 [1,2]");
        ids.clear();
        r.ForEachId("wave", [&](uint32_t id) { ids.push_back(id); });
        CHECK(ids.size() == 2 && ids[0] == 1 && ids[1] == 2, "wave ForEachId 升序 [1,2]");
    }

    // ---- 测试 2: 坏包拒载 ----
    {
        std::vector<TableData> tables = { MakeSp01Table() };
        std::vector<uint8_t> b; std::string err;
        BuildBin(tables, b, err);
        std::vector<uint8_t> bad = b;
        bad[bad.size() / 2] ^= 0x5A;   // 翻转一个字节
        CBinReader r;
        CHECK(!r.Load(bad.data(), bad.size()), "损坏 bin 拒载");
        CHECK(std::strstr(r.Error(), "content_hash") != nullptr || std::strstr(r.Error(), "损坏") != nullptr,
              "损坏报 content_hash 错 (实际 %s)", r.Error());
    }

    // ---- 测试 3: ExportTables (TSV 路径) + 强类型表 + 生成头 ----
    {
        std::vector<TableSource> src = {
            { "sp01", "Sp01Info", kSp01Tsv },
            { "wave", "WaveInfo", kWaveTsv },
        };
        std::vector<uint8_t> bin; std::string header, err;
        CHECK(ExportTables(src, bin, header, err), "ExportTables 成功 (%s)", err.c_str());

        CBinReader r;
        CHECK(r.Load(bin.data(), bin.size()), "导出 bin 可读 (%s)", r.Error());

        // 强类型按 id 取行
        Sp01Info s1, s2;
        CHECK(CConfigTable<Sp01Info>(&r, "sp01").Get(1, s1), "Get(sp01,1)");
        CHECK(s1.id == 1 && s1.maxHp == 100 && s1.walkSpeed == 327680, "Sp01Info(1) maxHp/walkSpeed 正确");
        CHECK(std::strcmp(s1.name.c_str(), "僵尸") == 0, "Sp01Info(1) name == 僵尸");
        CHECK(s1.dropTable.size() == 2 && s1.dropTable[0] == 1001 && s1.dropTable[1] == 1002,
              "Sp01Info(1) dropTable == [1001,1002]");

        CHECK(CConfigTable<Sp01Info>(&r, "sp01").Get(2, s2), "Get(sp01,2)");
        CHECK(s2.maxHp == 300 && s2.walkSpeed == 229376 && s2.classId == 10, "Sp01Info(2) 正确");

        WaveInfo w1;
        CHECK(CConfigTable<WaveInfo>(&r, "wave").Get(1, w1), "Get(wave,1)");
        CHECK(w1.sp01Id == 1 && w1.count == 5 && w1.interval == 60, "WaveInfo(1) 正确");

        CHECK(!CConfigTable<Sp01Info>(&r, "sp01").Get(999, s1), "Get(sp01,999) 失败");

        // 生成头内容
        CHECK(header.find("struct Sp01Info") != std::string::npos, "生成头含 struct Sp01Info");
        CHECK(header.find("struct WaveInfo") != std::string::npos, "生成头含 struct WaveInfo");
        CHECK(header.find("int32_t maxHp") != std::string::npos, "生成头含 int32_t maxHp");
        CHECK(header.find("std::vector<int32_t> dropTable") != std::string::npos, "生成头含 dropTable 数组");
        CHECK(header.find("bool Load(") != std::string::npos, "生成头含 Load");

        // 导出幂等
        std::vector<uint8_t> bin2; std::string h2;
        CHECK(ExportTables(src, bin2, h2, err), "ExportTables 第二次成功 (%s)", err.c_str());
        CHECK(bin == bin2, "ExportTables 同输入两次逐字节一致 (幂等)");
    }

    if (g_fail == 0) {
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d FAILED\n", g_fail);
    return 1;
}
