// xThread.i -- Thread wrapper for Lua
// Exports: xThread with director feature for Lua thread_proc() override

%include "xBase/xThread.h"

// Allow Lua to subclass xThread via SWIG directors
%feature("director") xThread;
