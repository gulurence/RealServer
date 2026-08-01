cmake_minimum_required(VERSION 3.10)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)

# 设置源代码路径
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 获取源代码文件（包括子目录）
file(GLOB X_BASE            "${SOURCE_DIR}/xBase/*.h"       "${SOURCE_DIR}/xBase/*.cpp")
file(GLOB X_ERRORCODE       "${SOURCE_DIR}/xErrorCode/*.h"  "${SOURCE_DIR}/xErrorCode/*.cpp")
file(GLOB X_SHAREMEM        "${SOURCE_DIR}/xShareMem/*.h"   "${SOURCE_DIR}/xShareMem/*.cpp")
file(GLOB X_CONFIG          "${SOURCE_DIR}/xConfig/*.h"     "${SOURCE_DIR}/xConfig/*.cpp")
file(GLOB X_LOG             "${SOURCE_DIR}/xLog/*.h"        "${SOURCE_DIR}/xLog/*.cpp")
file(GLOB X_NET             "${SOURCE_DIR}/xNet/*.h"        "${SOURCE_DIR}/xNet/*.cpp")
file(GLOB X_DATABASE        "${SOURCE_DIR}/xDatabase/*.h"   "${SOURCE_DIR}/xDatabase/*.cpp")
file(GLOB X_REDIS           "${SOURCE_DIR}/xRedis/*.h"      "${SOURCE_DIR}/xRedis/*.cpp")
file(GLOB X_NATS            "${SOURCE_DIR}/xNats/*.h"       "${SOURCE_DIR}/xNats/*.cpp")
file(GLOB X_COMMAND         "${SOURCE_DIR}/xCommand/*.h"    "${SOURCE_DIR}/xCommand/*.cpp")
file(GLOB X_EVENT           "${SOURCE_DIR}/xEvent/*.h"      "${SOURCE_DIR}/xEvent/*.cpp")
file(GLOB X_SCHEDULER       "${SOURCE_DIR}/xScheduler/*.h"  "${SOURCE_DIR}/xScheduler/*.cpp")
file(GLOB X_RPC             "${SOURCE_DIR}/xRpc/*.h"        "${SOURCE_DIR}/xRpc/*.cpp")
file(GLOB X_SERVICE         "${SOURCE_DIR}/xService/*.h"    "${SOURCE_DIR}/xService/*.cpp")
file(GLOB X_TSERVICE        "${SOURCE_DIR}/tService/*.h"    "${SOURCE_DIR}/tService/*.cpp")
file(GLOB X_ACTOR           "${SOURCE_DIR}/xActor/*.h"      "${SOURCE_DIR}/xActor/*.cpp")
file(GLOB X_RECORD          "${SOURCE_DIR}/xRecord/*.h"     "${SOURCE_DIR}/xRecord/*.cpp")
file(GLOB X_SERVER          "${SOURCE_DIR}/xServer/*.h"     "${SOURCE_DIR}/xServer/*.cpp")
file(GLOB X_WEBCONSOLE      "${SOURCE_DIR}/xWebConsole/*.h" "${SOURCE_DIR}/xWebConsole/*.cpp")
file(GLOB X_CLUSTER          "${SOURCE_DIR}/xCluster/*.h"   "${SOURCE_DIR}/xCluster/*.cpp")

# Lua 5.4 — compiled from source into the engine
# Exclude lua.c and luac.c (standalone interpreters with their own main())
file(GLOB X_LUA_SRC "${CMAKE_SOURCE_DIR}/third_part/lua-5.4.7/src/*.c")
list(REMOVE_ITEM X_LUA_SRC
    "${CMAKE_SOURCE_DIR}/third_part/lua-5.4.7/src/lua.c"
    "${CMAKE_SOURCE_DIR}/third_part/lua-5.4.7/src/luac.c")

# luacore SWIG wrapper generation
set(SWIG_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/luacore_generated")
file(MAKE_DIRECTORY ${SWIG_GENERATED_DIR})

# All sub-files (base/*.i, actor/*.i) are %include'd by luacore.i
# Only luacore.i needs to be SWIG-processed; it pulls in everything.
file(GLOB SWIG_I_DEPENDS
    "${SOURCE_DIR}/luacore/*.i"
    "${SOURCE_DIR}/luacore/base/*.i"
    "${SOURCE_DIR}/luacore/actor/*.i")

set(SWIG_OUTPUT_CXX "${SWIG_GENERATED_DIR}/luacore_wrap.cxx")
add_custom_command(
    OUTPUT ${SWIG_OUTPUT_CXX}
    COMMAND ${SWIG_EXECUTABLE} -c++ -lua
            -I${SOURCE_DIR}/luacore
            -I${SOURCE_DIR}
            -o ${SWIG_OUTPUT_CXX}
            ${SOURCE_DIR}/luacore/luacore.i
    DEPENDS ${SOURCE_DIR}/luacore/luacore.i ${SWIG_I_DEPENDS}
    COMMENT "SWIG: Generating Lua wrapper (luacore)"
)
set(SWIG_GENERATED_SOURCES ${SWIG_OUTPUT_CXX})

file(GLOB X_LUACORE_CPP "${SOURCE_DIR}/luacore/*.cpp" "${SOURCE_DIR}/luacore/*.h")

# 进程入口
file(GLOB X_MAIN "${SOURCE_DIR}/main.cpp")

# Proto-generated RPC stubs (hand-written equivalents of protoc output)
file(GLOB X_PROTO_RPC
    "${CMAKE_SOURCE_DIR}/source/proto-cpp/protocol/rpc/*.pb.cc"
    "${CMAKE_SOURCE_DIR}/source/proto-cpp/protocol/rpc/*.grpc.pb.cc"
)

#message(STATUS "SOURCE_DIR: ${SOURCE_DIR}")

# 调试输出，查看找到的源文件
#message(STATUS "X_ASIO: ${X_ASIO}")
#message(STATUS "X_BASE: ${X_BASE}")

# 将所有源文件添加到一个变量中
set(SOURCES
    ${X_BASE}
    ${X_ERRORCODE}
    ${X_SHAREMEM}
    ${X_CONFIG}
    ${X_LOG}
    ${X_NET}
    ${X_DATABASE}
    ${X_REDIS}
    ${X_NATS}
    ${X_COMMAND}
    ${X_EVENT}
    ${X_SCHEDULER}
    ${X_RPC}
    ${X_SERVICE}
    ${X_TSERVICE}
    ${X_ACTOR}
    ${X_RECORD}
    ${X_SERVER}
    ${X_WEBCONSOLE}
    ${X_CLUSTER}
    ${X_LUACORE_CPP}
    ${SWIG_GENERATED_SOURCES}
    ${X_PROTO_RPC}
    ${X_MAIN}
    ${X_LUA_SRC}
)

#message(STATUS "SOURCES: ${SOURCES}")

# 将源文件分组（在 Visual Studio 中显示为目录结构）
source_group("src/xBase"       FILES ${X_BASE})
source_group("src/xErrorCode"  FILES ${X_ERRORCODE})
source_group("src/xShareMem"   FILES ${X_SHAREMEM})
source_group("src/xConfig"     FILES ${X_CONFIG})
source_group("src/xLog"        FILES ${X_LOG})
source_group("src/xNet"        FILES ${X_NET})
source_group("src/xDatabase"   FILES ${X_DATABASE})
source_group("src/xRedis"      FILES ${X_REDIS})
source_group("src/xNats"       FILES ${X_NATS})
source_group("src/xCommand"    FILES ${X_COMMAND})
source_group("src/xEvent"      FILES ${X_EVENT})
source_group("src/xScheduler"  FILES ${X_SCHEDULER})
source_group("src/xRpc"        FILES ${X_RPC})
source_group("src/xService"    FILES ${X_SERVICE})
source_group("src/tService"    FILES ${X_TSERVICE})
source_group("src/xActor"      FILES ${X_ACTOR})
source_group("src/xRecord"     FILES ${X_RECORD})
source_group("src/main"        FILES ${X_MAIN})
source_group("src/xServer"     FILES ${X_SERVER})
source_group("src/xWebConsole" FILES ${X_WEBCONSOLE})
source_group("src/xCluster"    FILES ${X_CLUSTER})
source_group("src/luacore"              FILES ${X_LUACORE_CPP})
source_group("src/luacore/generated"    FILES ${SWIG_GENERATED_SOURCES})
source_group("third_part/lua"         FILES ${X_LUA_SRC})

# 生成可执行文件
if (MSVC)
    add_executable(RealServer ${SOURCES})
    set_target_properties(RealServer PROPERTIES OUTPUT_NAME "RealServer")
    add_definitions("-DUNICODE" "-D_UNICODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /sdl /Z7 /GR /FC /Od")
    add_definitions("/utf-8 /wd4200 /wd4099 /wd4267 /wd4244 /wd4251 /wd4996")
    add_definitions("-DBOOST_ASIO_STANDALONE")
    add_definitions("-DNOMINMAX")
else()
    add_executable(RealServer ${SOURCES})
    set_target_properties(RealServer PROPERTIES OUTPUT_NAME "RealServer")
    target_compile_options(RealServer PRIVATE -Wall -Wno-sign-compare -Wno-return-type -Wno-delete-non-virtual-dtor -g -O0 -pthread)
    target_compile_definitions(RealServer PRIVATE BOOST_ASIO_STANDALONE)
endif()

# 设置头文件目录
target_include_directories(RealServer PUBLIC
    ${SOURCE_DIR}   # 确保包含 engine 目录下的所有头文件
    ${CMAKE_SOURCE_DIR}/third_part  # 如果需要第三方库头文件
    ${CMAKE_SOURCE_DIR}/third_part/nats.c-main  # nats 头文件路径
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/cxx17
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/no_tls
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/future
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/future/boost
    ${CMAKE_SOURCE_DIR}/third_part/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_part/asio/include     # Boost/ASIO (header-only)
    ${LUA_INCLUDE_DIR}
    ${SWIG_GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/source/proto-cpp/protocol  # Proto-generated headers
)

if(MSVC)
    target_include_directories(RealServer PUBLIC
        ${CMAKE_SOURCE_DIR}/third_part/mysql-connector-c-6.1.11-winx64
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/include  # libxml2
        ${CMAKE_SOURCE_DIR}/third_part/iconv_shim        # iconv stub (libxml2)
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/build_win # libxml2 generated (xmlversion.h)
        ${CMAKE_SOURCE_DIR}/third_part/hiredis          # hiredis (Redis C client)
        ${CMAKE_SOURCE_DIR}/third_part/protobuf/src      # protobuf
        ${CMAKE_SOURCE_DIR}/third_part/abseil-cpp         # abseil (protobuf dep)
        ${CMAKE_SOURCE_DIR}/third_part/protobuf/third_party/abseil-cpp  # bundled abseil
        ${CMAKE_SOURCE_DIR}/third_part/protobuf/third_party/utf8_range  # utf8_validity
        ${CMAKE_SOURCE_DIR}/third_part/grpc/include          # gRPC
    )
else()
    find_package(PkgConfig REQUIRED)
    # MariaDB/MySQL — Debian uses mariadb by default
    pkg_check_modules(MYSQL IMPORTED_TARGET mysqlclient)
    if(NOT MYSQL_FOUND)
        pkg_check_modules(MYSQL REQUIRED IMPORTED_TARGET mariadb)
    endif()
    pkg_check_modules(LIBXML2  REQUIRED IMPORTED_TARGET libxml-2.0)
    pkg_check_modules(HIREDIS  REQUIRED IMPORTED_TARGET hiredis)
    pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)
    pkg_check_modules(GRPC     REQUIRED IMPORTED_TARGET grpc++)
    target_include_directories(RealServer PUBLIC
        ${MYSQL_INCLUDE_DIRS}
        ${LIBXML2_INCLUDE_DIRS}
        ${HIREDIS_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
        ${GRPC_INCLUDE_DIRS}
    )
endif()

# ============================================================
# Bundled third-party library builds (Linux only)
# ============================================================
if(NOT MSVC)
    # nats.c — no system package on Debian
    set(NATS_BUILD_WITH_TLS ON CACHE BOOL "" FORCE)
    set(NATS_BUILD_STREAMING OFF CACHE BOOL "" FORCE)
    set(NATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(NATS_BUILD_LIB_STATIC ON CACHE BOOL "" FORCE)
    set(NATS_BUILD_LIB_SHARED OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_part/nats.c-main
                     ${CMAKE_BINARY_DIR}/nats-build)

    # redis-plus-plus — no system package on Debian
    set(REDIS_PLUS_PLUS_CXX_STANDARD 17 CACHE STRING "" FORCE)
    set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(REDIS_PLUS_PLUS_USE_TLS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus
                     ${CMAKE_BINARY_DIR}/redispp-build)
endif()

# ============================================================
# Link libraries
# ============================================================
if(NOT MSVC)
    target_link_libraries(RealServer PRIVATE
        PkgConfig::MYSQL
        PkgConfig::LIBXML2
        PkgConfig::HIREDIS
        PkgConfig::PROTOBUF
        PkgConfig::GRPC
        nats_static
        redis++_static
        pthread
        dl
        ssl
        crypto
    )
endif()

# Lua 5.4 is compiled directly from source into the engine
# (no external link needed — see X_LUA_SRC in SOURCES)
