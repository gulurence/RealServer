# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)

# 项目信息
project(gate)

# 设置源代码路径
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 获取源代码文件（包括子目录）
file(GLOB X_SRC            "${SOURCE_DIR}/*.h"       "${SOURCE_DIR}/*.cpp")

#message(STATUS "SOURCE_DIR: ${SOURCE_DIR}")

# 调试输出，查看找到的源文件
#message(STATUS "X_ASIO: ${X_ASIO}")
#message(STATUS "X_BASE: ${X_BASE}")

# 将所有源文件添加到一个变量中
set(SOURCES
    ${X_SRC}
)

#message(STATUS "SOURCES: ${SOURCES}")

# 将源文件分组（在 Visual Studio 中显示为目录结构）
source_group("src"       FILES ${X_SRC})

# 生成静态库（.lib 和 .a 文件）
if (MSVC)
    add_executable(gate ${SOURCES})
    set_target_properties(gate PROPERTIES OUTPUT_NAME "gate")    
    add_definitions("-DUNICODE" "-D_UNICODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /sdl /Zi /GR /FC /Od")
    add_definitions("/utf-8 /wd4200 /wd4099 /wd4267 /wd4244 /wd4521")
else()
    add_library(gate STATIC ${SOURCES})
    set_target_properties(gate PROPERTIES OUTPUT_NAME "libGateService")
endif()


# 设置头文件目录
target_include_directories(gate PUBLIC 
    ${CMAKE_SOURCE_DIR}/src/engine   # 确保包含 engine 目录下的所有头文件
    ${CMAKE_SOURCE_DIR}/third_part  # 如果需要第三方库头文件
    ${CMAKE_SOURCE_DIR}/third_part/mysql-connector-c-6.1.11-winx64
    #${CMAKE_SOURCE_DIR}/third_part/protobuf-3.19.2/src  # Protobuf 头文件路径
)

# 链接 engine 库
target_link_libraries(gate PRIVATE engine)
