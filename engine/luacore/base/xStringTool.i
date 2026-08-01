// xStringTool.i -- String manipulation utilities for Lua
// Exports: StringTool static methods

%include "xBase/xStringTool.h"

// Instantiate templated methods for common types
%template(format_index_string)     StringTool::format_index<std::string>;
%template(tostring_int32)          StringTool::tostring<int32>;
%template(tostring_uint32)         StringTool::tostring<uint32>;
%template(tostring_int64)          StringTool::tostring<int64>;
%template(tostring_uint64)         StringTool::tostring<uint64>;
%template(tostring_bool)           StringTool::tostring<bool>;
%template(tostring_double)         StringTool::tostring<double>;
