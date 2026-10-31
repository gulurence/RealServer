#
# Copyright (c) 2026 GuMingYao,  All rights reserved
#
# File name: CMakeLists.txt
# Purpose:   项目顶层构建入口
#
# RealServer 顶层 CMake 配置：设定 C++20 标准、定位 third_part 下的 Lua 5.4 源码目录与 SWIG 可执行文件
# 通过 add_subdirectory 挂载 engine 子目录，不直接定义编译目标
#

cmake_minimum_required(VERSION 3.10)

# 项目信息
project(RealServer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)

# Lua 5.4 — compiled from source in third_part/lua-5.4.7
set(LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_part/lua-5.4.7/src")

# SWIG discovery (pre-built in third_part)
if(WIN32)
    find_program(SWIG_EXECUTABLE swig
        HINTS "${CMAKE_SOURCE_DIR}/third_part/swigwin-4.3.0/swigwin-4.3.0"
        REQUIRED)
else()
    find_program(SWIG_EXECUTABLE swig REQUIRED)
endif()

# 添加子目录
add_subdirectory(engine)

# 可以根据需要添加更多的子目录
