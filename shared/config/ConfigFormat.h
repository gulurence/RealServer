// =============================================================================
// ConfigFormat.h — 配置 bin 文件格式定义 (Excel→bin 管线的运行时格式)
//
// 目标: 客户端与服务器共用一份游戏内数据。Excel(编辑源) → 导出工具 → bin(唯一分发物)。
// 运行时(帧同步内核 + 游戏框架)只读内存里已解析的结构, 不读 Excel/JSON。
//
// 定点口径: 会被帧同步内核消费的数值一律 fx(Q16.16 原始 int32) 或整数, 禁止 f32。
//   fx 字段在 Excel 里写浮点(如 5.0), 导出工具用与内核同一份 FxFromFloat 转成定点位,
//   运行时读出来直接参与定点运算, 零转换。
//
// 字节序: 全文件 little-endian, 显式 LE 序列化, 与平台字节序无关(逐位确定)。
// 哈希: FNV-1a 64, content_hash 覆盖整个文件(计算时该字段按 0), schema_hash 覆盖
//   全部 schema section, 供「schema 与数据错配」/「坏包」校验。
// =============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace rsconfig {

// magic "RSCF"
constexpr uint8_t  kMagic0 = 'R', kMagic1 = 'S', kMagic2 = 'C', kMagic3 = 'F';
constexpr uint32_t kBinVersion = 1;

// 字段类型 (导出工具与运行时共享; 加类型需同步两边)
enum FieldType : uint8_t {
    kTypeI32    = 1,  // int32
    kTypeI64    = 2,  // int64
    kTypeFx     = 3,  // Q16.16 定点 (int32 原始位)
    kTypeF32    = 4,  // float (仅表现层, 不进内核)
    kTypeStr    = 5,  // 字符串 (偏移进字符串表)
    kTypeBool   = 6,  // uint8 0/1
    kTypeU8     = 7,  // uint8
    kTypeU16    = 8,  // uint16
    kTypeArrI32 = 9,  // 变长 int32 数组 (掉落表/波次表)
    kTypeArrFx  = 10, // 变长 fx 数组
};

// ---- header 布局 (40 字节) ----
constexpr uint32_t kHeaderSize     = 40;
constexpr uint32_t kOffBinVersion  = 4;
constexpr uint32_t kOffFlags       = 8;
constexpr uint32_t kOffContentHash = 12;  // uint64
constexpr uint32_t kOffTableCount  = 20;
constexpr uint32_t kOffSchemaHash  = 24;  // uint64
constexpr uint32_t kOffStringOff   = 32;
constexpr uint32_t kOffStringSize  = 36;

// ---- 目录项 (32 字节/表) ----
// name_off u32, row_count u32, schema_off u32, schema_size u32,
// data_off u32, data_size u32, index_off u32, index_size u32
constexpr uint32_t kDirEntrySize = 32;

// ---- schema 字段项 (8 字节/字段) ----
// name_off u32, type u8, flags u8, reserved u16
constexpr uint32_t kSchemaFieldSize = 8;

// ---- 索引项 (12 字节/行) ----
// id u32, row_off u32(相对 data 起点), len u32(该行编码长度)
constexpr uint32_t kIndexEntrySize = 12;

// ---- FNV-1a 64 ----
inline uint64_t Fnv1aInit() { return 0xcbf29ce484222325ULL; }
inline void Fnv1aStep(uint64_t& h, uint8_t b) { h ^= b; h *= 0x100000001b3ULL; }
// 把一个 uint64 按 little-endian 字节序折进哈希 (聚合多个子哈希用, 保证跨平台一致)
inline void Fnv1aStepU64(uint64_t& h, uint64_t v) {
    for (int i = 0; i < 8; ++i) Fnv1aStep(h, (uint8_t)(v >> (i * 8)));
}
inline uint64_t Fnv1aRange(const uint8_t* p, size_t n) {
    uint64_t h = Fnv1aInit();
    for (size_t i = 0; i < n; ++i) Fnv1aStep(h, p[i]);
    return h;
}

// ---- 显式 LE 序列化 ----
inline void PutU32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff); p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff); p[3] = (uint8_t)((v >> 24) & 0xff);
}
inline void PutU64(uint8_t* p, uint64_t v) {
    PutU32(p, (uint32_t)(v & 0xffffffffu)); PutU32(p + 4, (uint32_t)(v >> 32));
}
inline uint32_t GetU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline uint64_t GetU64(const uint8_t* p) {
    return (uint64_t)GetU32(p) | ((uint64_t)GetU32(p + 4) << 32);
}

// 字段定长字节数 (数组/变长返回 0)
inline size_t FieldWireSize(FieldType t) {
    switch (t) {
        case kTypeI32: case kTypeFx: case kTypeF32: case kTypeStr: return 4;
        case kTypeI64: return 8;
        case kTypeBool: case kTypeU8: return 1;
        case kTypeU16: return 2;
        case kTypeArrI32: case kTypeArrFx: return 0;  // 变长
        default: return 0;
    }
}
inline bool IsArrayType(FieldType t) { return t == kTypeArrI32 || t == kTypeArrFx; }

// ---- 数据模型 (导出工具构造 / 测试构造) ----
struct FieldDesc {
    std::string name;
    FieldType   type = kTypeI32;
    FieldDesc() {}
    FieldDesc(const std::string& n, FieldType t) : name(n), type(t) {}
};

struct Cell {
    FieldType            type = kTypeI32;
    int64_t              i = 0;   // i32/i64/fx/f32(位)/bool/u8/u16
    std::string          s;       // str
    std::vector<int32_t> a;       // arr_i32 / arr_fx
};

struct Row {
    uint32_t          id = 0;
    std::vector<Cell> cells;
};

struct TableData {
    std::string            name;
    std::vector<FieldDesc> fields;
    std::vector<Row>       rows;
};

} // namespace rsconfig
