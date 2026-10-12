// xDefine.i -- Type aliases and constants for Lua
// We do NOT %include "xDefine.h" directly because it contains
// MSVC-specific windows.h includes that SWIG cannot parse.
// Instead, we declare only the type names SWIG needs to know.
//
// IMPORTANT: typedefs below are SWIG-level declarations only — they are
// NOT emitted to C++ (that's what %inline would do).  The real C++
// definitions come from xBase/xDefine.h which is included in luacore.i's
// %{ %} block.  Keeping these out of %inline avoids typedef conflicts on
// Linux where int64=long (not long long).

// Lua 5.4 numbers are floats by default (not integer subtype).
// Override the 'in' typemap for our custom integer types to accept
// any Lua number (lua_isnumber) instead of strict lua_isinteger.
%define ACCEPT_LUA_NUMBER(TYPE, LUA_TO_FN)
%typemap(in) TYPE {
    if (!lua_isnumber(L, $argnum)) {
        lua_pushstring(L, "Expected a number");
        SWIG_fail;
    }
    $1 = (TYPE)LUA_TO_FN(L, $argnum);
}
%enddef

// ---------------------------------------------------------------------------
// SWIG-level type declarations — tells SWIG about these names so typemaps
// and function signatures work.  NOT emitted to generated C++.
// ---------------------------------------------------------------------------
typedef char                int8;
typedef short               int16;
typedef int                 int32;
typedef long long           int64;
typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;

// Domain type aliases (from xDefine.h)
typedef uint64 ServiceID;
typedef int64  ActorID;

// Net types
typedef uint64 NetCID;
typedef uint64 GateID;

// Constants (accessible from Lua as luacore.INVALID_ID, etc.)
#define INVALID_ID (-1)
#define MAX_NAMESIZE 32

// Apply typemaps to all integer types and their domain aliases
ACCEPT_LUA_NUMBER(int8,   lua_tonumber)
ACCEPT_LUA_NUMBER(int16,  lua_tonumber)
ACCEPT_LUA_NUMBER(int32,  lua_tonumber)
ACCEPT_LUA_NUMBER(int64,  lua_tonumber)
ACCEPT_LUA_NUMBER(uint8,  lua_tonumber)
ACCEPT_LUA_NUMBER(uint16, lua_tonumber)
ACCEPT_LUA_NUMBER(uint32, lua_tonumber)
ACCEPT_LUA_NUMBER(uint64, lua_tonumber)
// Also apply to domain typedefs (SWIG may not resolve through all typedef chains)
ACCEPT_LUA_NUMBER(ActorID,   lua_tonumber)
ACCEPT_LUA_NUMBER(ServiceID, lua_tonumber)
ACCEPT_LUA_NUMBER(NetCID,    lua_tonumber)
ACCEPT_LUA_NUMBER(GateID,    lua_tonumber)

// ---------------------------------------------------------------------------
// 返回值精度: SWIG 对 long long (int64/uint64) 的默认 out typemap 用 lua_pushnumber
// (double), 超过 2^53 丢精度 —— 战斗哈希 (uint64 位重解释为 int64) 会被四舍五入到
// 最近 double (近 6.4e18 处 ULP≈1024), 导致「两端哈希失配」被 double 舍入掩盖成相等
// (M1-c 冒烟实测: 客户端 hash=...1400, 服务器 log 里读回 ...1168, 差值 232 即舍入)。
// 改为 push 成 Lua 整数 (lua_pushinteger, lua_Integer 64 位) 保持逐位精确。
// 注意: 只覆盖 int64/uint64 (int32 等 SWIG 默认已 push 整数, 无需改)。
%typemap(out) int64 {
    lua_pushinteger(L, (lua_Integer)$1);
    SWIG_arg++;
}
%typemap(out) uint64 {
    lua_pushinteger(L, (lua_Integer)$1);
    SWIG_arg++;
}

// =========================================================================
// Utility: convert Lua number → Lua integer
//
// In Lua 5.4, numeric literals (e.g. 100) are floats by default, not
// integers.  SWIG's integer typemaps use lua_isinteger() which rejects
// floats.  These helpers let Lua code explicitly cast a number to an
// integer before passing it to SWIG-wrapped C++ functions.
//
// Usage from Lua:
//   local t = luacore.toint32(100)      -- → integer suitable for actor type
//   local id = luacore.toint64(myid)    -- → integer suitable for ActorID
// =========================================================================
%inline %{
static int32  toint32(double n) { return (int32)n; }
static int64  toint64(double n) { return (int64)n; }
%}
