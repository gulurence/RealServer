// xUniqueIDManager.i -- Unique ID pool for Lua

%include "xBase/xUniqueIDManager.h"

// Instantiate template for types used in the project
%template(UniqueUInt32IDManager) UniqueIDManager<uint32>;
%template(UniqueUInt16IDManager) UniqueIDManager<uint16>;
