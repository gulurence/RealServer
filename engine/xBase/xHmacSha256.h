/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xHmacSha256.h
      Purpose:   自包含 SHA-256 与 HMAC-SHA256 摘要实现

      纯头文件实现 FIPS 180-4 SHA-256 与 RFC 2104 HMAC-SHA256
      入口 HmacSha256Hex(key,msg) 输出 64 位小写十六进制摘要，无 OpenSSL 依赖
  *****************************************************************/

#pragma once

#include "xDefine.h"

#include <cstring>
#include <string>

// =============================================================================
// xHmacSha256 — 自包含 SHA-256 (FIPS 180-4) + HMAC-SHA256 (RFC 2104)
//
// 无外部依赖 (不依赖 OpenSSL / 系统 crypto), 供 authKey 派生等「离线可校验」
// 场景使用。仅暴露一个入口 HmacSha256Hex(key, msg): 输入密钥与消息, 输出
// 64 字符小写十六进制摘要。
// =============================================================================
namespace xHmacSha256
{

namespace detail
{

// 轮转常量 (FIPS 180-4 §4.2.2)
inline constexpr uint32 kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

// 初始哈希值 (FIPS 180-4 §5.3.3, 前 8 个素数的平方根小数部分前 32 位)
inline constexpr uint32 kSha256Init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

inline uint32 Ror(uint32 x, uint32 n)
{
    return (x >> n) | (x << (32 - n));
}

inline void Sha256Block(uint32 state[8], const uint8 *p)
{
    uint32 w[64];
    for (int32 i = 0; i < 16; ++i)
    {
        w[i] = ((uint32)p[i * 4] << 24) | ((uint32)p[i * 4 + 1] << 16) | ((uint32)p[i * 4 + 2] << 8)
               | (uint32)p[i * 4 + 3];
    }
    for (int32 i = 16; i < 64; ++i)
    {
        uint32 s0 = Ror(w[i - 15], 7) ^ Ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32 s1 = Ror(w[i - 2], 17) ^ Ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32 a = state[0], b = state[1], c = state[2], d = state[3];
    uint32 e = state[4], f = state[5], g = state[6], h = state[7];
    for (int32 i = 0; i < 64; ++i)
    {
        uint32 S1 = Ror(e, 6) ^ Ror(e, 11) ^ Ror(e, 25);
        uint32 ch = (e & f) ^ (~e & g);
        uint32 t1 = h + S1 + ch + kSha256K[i] + w[i];
        uint32 S0 = Ror(a, 2) ^ Ror(a, 13) ^ Ror(a, 22);
        uint32 maj = (a & b) ^ (a & c) ^ (b & c);
        uint32 t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

struct Sha256Ctx
{
    uint32 state[8];
    uint64 bitLen = 0; // 已处理的完整块位长 (不含当前缓冲)
    uint8 buf[64] = {0};
    uint32 bufLen = 0;
};

inline void Sha256Init(Sha256Ctx &ctx)
{
    for (int32 i = 0; i < 8; ++i)
    {
        ctx.state[i] = kSha256Init[i];
    }
    ctx.bitLen = 0;
    ctx.bufLen = 0;
}

inline void Sha256Update(Sha256Ctx &ctx, const uint8 *data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        ctx.buf[ctx.bufLen++] = data[i];
        if (ctx.bufLen == 64)
        {
            Sha256Block(ctx.state, ctx.buf);
            ctx.bitLen += 512;
            ctx.bufLen = 0;
        }
    }
}

inline void Sha256Final(Sha256Ctx &ctx, uint8 out[32])
{
    // 总位长 = 已处理块位长 + 当前缓冲位长 (在追加填充之前锁定)
    uint64 bitLen = ctx.bitLen + (uint64)ctx.bufLen * 8;

    uint8 pad = 0x80;
    Sha256Update(ctx, &pad, 1);
    uint8 zero = 0;
    while (ctx.bufLen != 56)
    {
        Sha256Update(ctx, &zero, 1);
    }
    uint8 lenBytes[8] = {0};
    for (int32 i = 0; i < 8; ++i)
    {
        lenBytes[i] = (uint8)(bitLen >> (56 - 8 * i)); // 大端
    }
    Sha256Update(ctx, lenBytes, 8);

    for (int32 i = 0; i < 8; ++i)
    {
        out[i * 4] = (uint8)(ctx.state[i] >> 24);
        out[i * 4 + 1] = (uint8)(ctx.state[i] >> 16);
        out[i * 4 + 2] = (uint8)(ctx.state[i] >> 8);
        out[i * 4 + 3] = (uint8)ctx.state[i];
    }
}

} // namespace detail

// HMAC-SHA256(key, msg) → 64 字符小写十六进制摘要
inline std::string HmacSha256Hex(const std::string &key, const std::string &msg)
{
    const size_t kBlockSize = 64;

    uint8 blockKey[kBlockSize] = {0};
    const uint8 *k = (const uint8 *)key.data();
    size_t kLen = key.size();
    if (kLen > kBlockSize)
    {
        uint8 hashed[32] = {0};
        detail::Sha256Ctx ctx;
        detail::Sha256Init(ctx);
        detail::Sha256Update(ctx, k, kLen);
        detail::Sha256Final(ctx, hashed);
        k = hashed;
        kLen = 32;
    }
    memcpy(blockKey, k, kLen); // 不足 64 字节时尾部已零填充

    uint8 inner[32] = {0};
    {
        uint8 buf[kBlockSize] = {0};
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            buf[i] = blockKey[i] ^ 0x36;
        }
        detail::Sha256Ctx ctx;
        detail::Sha256Init(ctx);
        detail::Sha256Update(ctx, buf, kBlockSize);
        detail::Sha256Update(ctx, (const uint8 *)msg.data(), msg.size());
        detail::Sha256Final(ctx, inner);
    }

    uint8 outer[32] = {0};
    {
        uint8 buf[kBlockSize] = {0};
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            buf[i] = blockKey[i] ^ 0x5c;
        }
        detail::Sha256Ctx ctx;
        detail::Sha256Init(ctx);
        detail::Sha256Update(ctx, buf, kBlockSize);
        detail::Sha256Update(ctx, inner, 32);
        detail::Sha256Final(ctx, outer);
    }

    static const char *kHex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (int32 i = 0; i < 32; ++i)
    {
        out.push_back(kHex[outer[i] >> 4]);
        out.push_back(kHex[outer[i] & 0xf]);
    }
    return out;
}

} // namespace xHmacSha256
