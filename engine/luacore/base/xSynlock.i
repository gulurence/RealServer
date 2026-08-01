// xSynlock.i -- Synchronization primitives for Lua
// Exports: CCriticalSection, CMutexLock, CSingleLock

%include "xBase/xSynlock.h"

// Note: ILockObj interface, CCriticalSection, CMutexLock, and CSingleLock
// are all non-templated, so SWIG can wrap them directly.
