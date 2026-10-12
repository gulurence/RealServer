// 自动生成, 勿手改 (config_exporter 从 Excel/TSV 生成)
// 本文件与导出工具 GenerateHeader 输出严格一致, 供单测编译引用 (验证生成头可编译 + 可解码)。
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../CBinReader.h"

namespace rsconfig {

// 表 sp01
struct Sp01Info {
    uint32_t id = 0;
    std::string name = {};
    int32_t classId = 0;
    int32_t maxHp = 0;
    int32_t walkSpeed = 0;   // fx_t (Q16.16)
    int32_t hurtToHero = 0;
    std::vector<int32_t> dropTable = {};

    bool Load(const CBinReader& rd, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id = c.U32();
        { uint32_t _o = c.U32(); const char* _s = rd.GetString(_o); name = _s ? _s : ""; }
        classId = c.I32();
        maxHp = c.I32();
        walkSpeed = c.I32();
        hurtToHero = c.I32();
        { uint32_t _n = c.U32(); dropTable.clear(); for (uint32_t _i = 0; _i < _n; ++_i) dropTable.push_back(c.I32()); }
        return c.ok;
    }
};

// 表 wave
struct WaveInfo {
    uint32_t id = 0;
    int32_t sp01Id = 0;
    int32_t count = 0;
    int32_t interval = 0;

    bool Load(const CBinReader& rd, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id = c.U32();
        sp01Id = c.I32();
        count = c.I32();
        interval = c.I32();
        return c.ok;
    }
};

} // namespace rsconfig
