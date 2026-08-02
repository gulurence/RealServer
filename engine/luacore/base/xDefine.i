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
typedef int32  ActorType;
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
ACCEPT_LUA_NUMBER(ActorType, lua_tonumber)
ACCEPT_LUA_NUMBER(ActorID,   lua_tonumber)
ACCEPT_LUA_NUMBER(ServiceID, lua_tonumber)
ACCEPT_LUA_NUMBER(NetCID,    lua_tonumber)
ACCEPT_LUA_NUMBER(GateID,    lua_tonumber)

// =========================================================================
// Utility: convert Lua number → Lua integer
//
// In Lua 5.4, numeric literals (e.g. 100) are floats by default, not
// integers.  SWIG's integer typemaps use lua_isinteger() which rejects
// floats.  These helpers let Lua code explicitly cast a number to an
// integer before passing it to SWIG-wrapped C++ functions.
//
// Usage from Lua:
//   local t = luacore.toint32(100)      -- → integer suitable for ActorType
//   local id = luacore.toint64(myid)    -- → integer suitable for ActorID
// =========================================================================
%inline %{
static int32  toint32(double n) { return (int32)n; }
static int64  toint64(double n) { return (int64)n; }
%}
