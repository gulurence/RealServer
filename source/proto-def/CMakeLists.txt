#
# Copyright (c) 2026 GuMingYao,  All rights reserved
#
# File name: CMakeLists.txt
# Purpose:   proto 静态库构建定义
#
# 创建 protocol 静态库（源文件 main.cpp）并导出当前目录为公共头文件搜索路径
# 实际 proto 生成主要由 make.bat 驱动 protoc，产物落在 source/proto-cpp
#

# 创建 protocol 静态库
add_library(protocol STATIC 
    main.cpp
)

# 设置头文件路径
target_include_directories(protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
