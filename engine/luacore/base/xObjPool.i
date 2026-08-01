// xObjPool.i -- Pre-allocated object pool for Lua
// Exports: xObjPoolBase, xObjPool<T>

%include "xBase/xObjPool.h"

// Note: xObjPool<T> requires T to extend xObjPoolBase.
// Concrete instantiations depend on project-specific usage.
// Types known to use xObjPool: RecordDataST
class RecordDataST;

%template(RecordDataObjPool) xObjPool<RecordDataST>;
