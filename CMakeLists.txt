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
