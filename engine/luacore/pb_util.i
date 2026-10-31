/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: pb_util.i
      Purpose:   protobuf 编解码公共辅助

      提供 static 函数 pb_split 按分隔符切分字符串，供各业务子 core 的 repeated 字段编码使用
      定义为内部链接，每个 wrap 文件各持一份副本，避免跨模块重复符号链接错误
  *****************************************************************/

// pb_util.i — 共享 protobuf 编解码辅助函数 (供各业务子 core 使用)
//
// pb_split: 按分隔符切分 "a,b,c" 字符串, 供 repeated 字段编码使用。
// 定义为 static (内部链接), 每个子 core 的 wrap 文件各持一份副本, 避免跨
// 模块重复符号链接错误。
%{
#include <string>
#include <vector>

static std::vector<std::string> pb_split(const std::string& s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}
%}
