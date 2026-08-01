// xCircularPool.i -- Lock-free ring buffer for Lua
// Exports: xCircularPool instantiated for xEvent* (used by xService)

%include "xBase/xCircularPool.h"

// Forward-declare xEvent so SWIG knows it's a valid pointer type
class xEvent;

%template(xEventCircularPool) xCircularPool<xEvent*>;
