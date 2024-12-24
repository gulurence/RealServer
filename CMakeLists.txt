cmake_minimum_required(VERSION 3.10)

# 项目信息
project(RealServer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)

# 添加子目录
add_subdirectory(src/engine)
add_subdirectory(src/service)

# 可以根据需要添加更多的子目录
