cmake_minimum_required(VERSION 3.10)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)

# 设置源代码路径
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 获取源代码文件（包括子目录）
file(GLOB X_BASE            "${SOURCE_DIR}/xBase/*.h"       "${SOURCE_DIR}/xBase/*.cpp")
file(GLOB X_SHAREMEM        "${SOURCE_DIR}/xShareMem/*.h"   "${SOURCE_DIR}/xShareMem/*.cpp")
file(GLOB X_CONFIG          "${SOURCE_DIR}/xConfig/*.h"     "${SOURCE_DIR}/xConfig/*.cpp")
file(GLOB X_LOG             "${SOURCE_DIR}/xLog/*.h"        "${SOURCE_DIR}/xLog/*.cpp")
file(GLOB X_NET             "${SOURCE_DIR}/xNet/*.h"        "${SOURCE_DIR}/xNet/*.cpp")
file(GLOB X_DATABASE        "${SOURCE_DIR}/xDatabase/*.h"   "${SOURCE_DIR}/xDatabase/*.cpp")
file(GLOB X_REDIS           "${SOURCE_DIR}/xRedis/*.h"      "${SOURCE_DIR}/xRedis/*.cpp")
file(GLOB X_NATS            "${SOURCE_DIR}/xNats/*.h"       "${SOURCE_DIR}/xNats/*.cpp")
file(GLOB X_COMMAND         "${SOURCE_DIR}/xCommand/*.h"    "${SOURCE_DIR}/xCommand/*.cpp")
file(GLOB X_SERVICE         "${SOURCE_DIR}/xService/*.h"    "${SOURCE_DIR}/xService/*.cpp")
file(GLOB X_WEBCONSOLE      "${SOURCE_DIR}/xWebConsole/*.h" "${SOURCE_DIR}/xWebConsole/*.cpp")

#message(STATUS "SOURCE_DIR: ${SOURCE_DIR}")

# 调试输出，查看找到的源文件
#message(STATUS "X_ASIO: ${X_ASIO}")
#message(STATUS "X_BASE: ${X_BASE}")

# 将所有源文件添加到一个变量中
set(SOURCES
    ${X_BASE}
    ${X_SHAREMEM}
    ${X_CONFIG}
    ${X_LOG}
    ${X_NET}
    ${X_DATABASE}
    ${X_REDIS}
    ${X_NATS}    
    ${X_COMMAND}
    ${X_SERVICE}
    ${X_WEBCONSOLE}
)

#message(STATUS "SOURCES: ${SOURCES}")

# 将源文件分组（在 Visual Studio 中显示为目录结构）
source_group("src/xBase"       FILES ${X_BASE})
source_group("src/xShareMem"   FILES ${X_SHAREMEM})
source_group("src/xConfig"     FILES ${X_CONFIG})
source_group("src/xLog"        FILES ${X_LOG})
source_group("src/xNet"        FILES ${X_NET})
source_group("src/xDatabase"   FILES ${X_DATABASE})
source_group("src/xRedis"      FILES ${X_REDIS})
source_group("src/xNats"       FILES ${X_NATS})
source_group("src/xCommand"    FILES ${X_COMMAND})
source_group("src/xService"    FILES ${X_SERVICE})
source_group("src/xWebConsole" FILES ${X_WEBCONSOLE})

# 生成静态库（.lib 和 .a 文件）
if (MSVC)
    add_library(engine STATIC ${SOURCES})
    set_target_properties(engine PROPERTIES OUTPUT_NAME "engine")
    add_definitions("-DUNICODE" "-D_UNICODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /sdl /Zi /GR /FC /Od")
    add_definitions("/utf-8 /wd4200 /wd4099 /wd4267 /wd4244 /wd4251")
else()
    add_library(engine STATIC ${SOURCES})
    set_target_properties(engine PROPERTIES OUTPUT_NAME "libcore")
endif()

# 设置头文件目录
target_include_directories(engine PUBLIC 
    ${SOURCE_DIR}   # 确保包含 engine 目录下的所有头文件
    ${CMAKE_SOURCE_DIR}/third_part  # 如果需要第三方库头文件
    ${CMAKE_SOURCE_DIR}/third_part/mysql-connector-c-6.1.11-winx64
    #${CMAKE_SOURCE_DIR}/third_part/protobuf-3.19.2/src  # Protobuf 头文件路径
)

# 设置所有目标的可执行文件输出目录
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bin")

# 设置 Debug 和 Release 类型的不同输出目录
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/build/debug")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build/release")

# 如果需要其他的依赖，使用 target_link_libraries 链接库
# target_link_libraries(engine PUBLIC other_dependencies)
