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
file(GLOB X_TRACE            "${SOURCE_DIR}/xTrace/*.h"     "${SOURCE_DIR}/xTrace/*.cpp")
file(GLOB X_GM               "${SOURCE_DIR}/xGm/*.h"        "${SOURCE_DIR}/xGm/*.cpp")
file(GLOB X_ENV              "${SOURCE_DIR}/xEnv/*.h"       "${SOURCE_DIR}/xEnv/*.cpp")

# 共享战斗核心 (客户端/服务器 同源, 仓库根 shared/battlecore/)
file(GLOB X_BATTLECORE       "${CMAKE_SOURCE_DIR}/shared/battlecore/*.h" "${CMAKE_SOURCE_DIR}/shared/battlecore/*.cpp")

# Lua 5.4 — compiled from source into the engine
# Exclude lua.c and luac.c (standalone interpreters with their own main())
file(GLOB X_LUA_SRC "${CMAKE_SOURCE_DIR}/third_part/lua-5.4.7/src/*.c")
list(REMOVE_ITEM X_LUA_SRC
    "${CMAKE_SOURCE_DIR}/third_part/lua-5.4.7/src/lua.c"
    "${CMAKE_SOURCE_DIR}/third_part/lua-5.4.7/src/luac.c")

# luacore SWIG wrapper generation — 拆分为模块化子 core:
#   luacore          (框架 + 登录/选角 + 通用 pb + 转发)
#   luacore_friend   (好友编解码)
#   luacore_room     (房间编解码)
#   luacore_chat     (聊天编解码)
#   luacore_battle   (战斗编解码 + 战斗核心)
#   luacore_spectate (观战编解码)
set(SWIG_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/luacore_generated")
file(MAKE_DIRECTORY ${SWIG_GENERATED_DIR})

# 子文件 (base/*.i, actor/*.i, pb_util.i) 被各模块 .i %include
file(GLOB SWIG_I_DEPENDS
    "${SOURCE_DIR}/luacore/*.i"
    "${SOURCE_DIR}/luacore/base/*.i"
    "${SOURCE_DIR}/luacore/actor/*.i")

set(LUACORE_MODULES
    luacore
    luacore_friend
    luacore_room
    luacore_chat
    luacore_battle
    luacore_spectate
    luacore_config)

set(SWIG_GENERATED_SOURCES "")
foreach(MOD ${LUACORE_MODULES})
    set(MOD_OUTPUT "${SWIG_GENERATED_DIR}/${MOD}_wrap.cxx")
    add_custom_command(
        OUTPUT ${MOD_OUTPUT}
        COMMAND ${SWIG_EXECUTABLE} -c++ -lua
                -I${SOURCE_DIR}/luacore
                -I${SOURCE_DIR}
                -o ${MOD_OUTPUT}
                ${SOURCE_DIR}/luacore/${MOD}.i
        DEPENDS ${SOURCE_DIR}/luacore/${MOD}.i ${SWIG_I_DEPENDS}
        COMMENT "SWIG: Generating Lua wrapper (${MOD})"
    )
    list(APPEND SWIG_GENERATED_SOURCES ${MOD_OUTPUT})
endforeach()

file(GLOB X_LUACORE_CPP "${SOURCE_DIR}/luacore/*.cpp" "${SOURCE_DIR}/luacore/*.h")

# 进程入口
file(GLOB X_MAIN "${SOURCE_DIR}/main.cpp")

# Proto-generated RPC 消息 schema (gRPC 已退役, 不再编译 *.grpc.pb.cc service stub)
# 自建 asio-TCP RPC 仅复用 actor_mgr.proto 的 PBRPC* 消息结构 (经 protoc 生成),
# 帧格式与传输见 engine/xRpc/RpcFrame.h。
file(GLOB X_PROTO_RPC
    "${CMAKE_SOURCE_DIR}/source/proto-cpp/protocol/rpc/*.pb.cc"
)
# *.pb.cc 会误匹配 *.grpc.pb.cc (gRPC 已退役), 显式剔除 service stub
list(FILTER X_PROTO_RPC EXCLUDE REGEX "\\.grpc\\.pb\\.cc$")

# Proto-generated message schemas (MsgID/Login/Player/PlayerItem/Account)
file(GLOB X_PROTO_MSG
    "${CMAKE_SOURCE_DIR}/source/proto-cpp/protocol/msg/*.pb.cc"
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
    ${X_TRACE}
    ${X_GM}
    ${X_ENV}
    ${X_BATTLECORE}
    ${X_LUACORE_CPP}
    ${SWIG_GENERATED_SOURCES}
    ${X_PROTO_RPC}
    ${X_PROTO_MSG}
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
source_group("src/xTrace"      FILES ${X_TRACE})
source_group("src/xGm"         FILES ${X_GM})
source_group("src/xEnv"        FILES ${X_ENV})
source_group("src/battlecore"  FILES ${X_BATTLECORE})
source_group("src/luacore"              FILES ${X_LUACORE_CPP})
source_group("src/luacore/generated"    FILES ${SWIG_GENERATED_SOURCES})
source_group("third_part/lua"         FILES ${X_LUA_SRC})

# ============================================================
# 版本自动提升 (git 派生)
#
# 进程二进制版本 = 短提交哈希 + 提交时间 (YYYYMMDDHHMM), 每次 configure 从当前 git
# 状态派生并生成 xVersion.h。提交即变化 → 进程注册 hash 的 version 字段自动变化,
# NodeManager 灰度滚动据此比对是否需升级。build_server.ps1 / CI-CD 每次干净 configure
# 都会重读 git, 因此无需手动 bump 版本。
# ============================================================
find_package(Git QUIET)
set(GIT_VERSION "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" log -1 --format=%cd --date=format:%Y%m%d%H%M
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(GIT_HASH AND GIT_DATE)
        set(GIT_VERSION "${GIT_HASH}-${GIT_DATE}")
    elseif(GIT_HASH)
        set(GIT_VERSION "${GIT_HASH}")
    endif()
endif()
message(STATUS "RealServer binary version (git): ${GIT_VERSION}")

set(VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen_version")
configure_file(
    "${SOURCE_DIR}/xCluster/xVersion.h.in"
    "${VERSION_HEADER_DIR}/xVersion.h"
    @ONLY)

# 生成可执行文件
if (MSVC)
    add_executable(RealServer ${SOURCES})
    set_target_properties(RealServer PROPERTIES OUTPUT_NAME "RealServer")
    add_definitions("-DUNICODE" "-D_UNICODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /sdl /Z7 /GR /FC /Od /Zc:preprocessor")
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
    ${VERSION_HEADER_DIR}                          # git 派生版本头 (xVersion.h, configure 生成)
    ${CMAKE_SOURCE_DIR}/source/proto-cpp/protocol  # Proto-generated headers
    ${CMAKE_SOURCE_DIR}/shared                     # 共享战斗核心 (battlecore/BattleCore.h)
)

if(MSVC)
    # --- vcpkg 依赖 (gRPC 已退役, 仅需 protobuf/hiredis) ---
    # protobuf/hiredis 统一走 vcpkg 安装目录, 与 bundled 版本一致
    set(VCPKG_INSTALLED_ROOT "D:/code/vcpkg/realserver-deps/installed/x64-windows"
        CACHE PATH "vcpkg installed prefix")
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_ROOT}")
    find_package(protobuf CONFIG REQUIRED)   # protobuf::libprotobuf
    find_package(hiredis CONFIG REQUIRED)    # hiredis::hiredis

    # mysql/libxml2 走 third_part 预编译; protobuf/hiredis 头文件由 find_package 提供
    target_include_directories(RealServer PUBLIC
        ${CMAKE_SOURCE_DIR}/third_part/mysql-connector-c-6.1.11-winx64  # mysql (mysql/mysql.h)
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/include  # libxml2
        ${CMAKE_SOURCE_DIR}/third_part/iconv_shim        # iconv stub (libxml2)
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/build_win # libxml2 generated (xmlversion.h)
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
    target_include_directories(RealServer PUBLIC
        ${MYSQL_INCLUDE_DIRS}
        ${LIBXML2_INCLUDE_DIRS}
        ${HIREDIS_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
    )
endif()

# ============================================================
# Bundled third-party library builds
# ============================================================
if(NOT MSVC)
    # nats.c — no system package on Debian (Linux only)
    set(NATS_BUILD_WITH_TLS ON CACHE BOOL "" FORCE)
    set(NATS_BUILD_STREAMING OFF CACHE BOOL "" FORCE)
    set(NATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(NATS_BUILD_LIB_STATIC ON CACHE BOOL "" FORCE)
    set(NATS_BUILD_LIB_SHARED OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_part/nats.c-main
                     ${CMAKE_BINARY_DIR}/nats-build)
endif()

# redis-plus-plus — 源码编译静态库 (两分支共用; sync RedisCluster 不依赖 libuv)
set(REDIS_PLUS_PLUS_CXX_STANDARD 17 CACHE STRING "" FORCE)
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(REDIS_PLUS_PLUS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REDIS_PLUS_PLUS_USE_TLS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus
                 ${CMAKE_BINARY_DIR}/redispp-build)

# ============================================================
# Link libraries
# ============================================================
if(NOT MSVC)
    target_link_libraries(RealServer PRIVATE
        PkgConfig::MYSQL
        PkgConfig::LIBXML2
        PkgConfig::HIREDIS
        PkgConfig::PROTOBUF
        nats_static
        redis++_static
        pthread
        dl
        ssl
        crypto
    )
else()
    # MSVC: protobuf/hiredis 走 vcpkg; mysql/libxml2 走 third_part 预编译 (gRPC 已退役)
    target_link_libraries(RealServer PRIVATE
        protobuf::libprotobuf
        hiredis::hiredis
        redis++_static
        ${CMAKE_SOURCE_DIR}/third_part/mysql-connector-c-6.1.11-winx64/lib/libmysql.lib
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/build_win/Debug/libxml2d.lib
    )
endif()

# ============================================================
# 部署: 每次编译后自动把产物拷贝到 bin/windows (Windows 运行部署目录),
# 与 bin/windows/start_cluster.ps1 的 $exe 路径一致。copy_if_different:
# 内容相同则跳过, 避免无谓的 mtime 抖动 (运行中的旧 exe 因被占用会报错)。
# ============================================================
if(MSVC)
    add_custom_command(TARGET RealServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/bin/windows"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:RealServer>"
                "${CMAKE_SOURCE_DIR}/bin/windows/RealServer.exe"
        COMMENT "Deploying RealServer.exe -> bin/windows")
endif()

# Lua 5.4 is compiled directly from source into the engine
# (no external link needed — see X_LUA_SRC in SOURCES)

# ============================================================
# NodeManager — 物理机守护进程 (独立轻量二进制)
#
# 只链接 xBase + xConfig + xRedis (RedisCluster), 不链接 Lua/Actor/RPC/asio。
# 与 RealServer.exe 同目录产出, 负责按期望数拉起/停止 RealServer 子进程。
# ============================================================
file(GLOB X_NODE_MANAGER "${SOURCE_DIR}/node_manager/*.h" "${SOURCE_DIR}/node_manager/*.cpp")
set(NODE_MANAGER_SOURCES
    ${X_BASE}
    ${X_LOG}
    ${X_CONFIG}
    ${X_REDIS}
    ${X_NODE_MANAGER}
)

add_executable(NodeManager ${NODE_MANAGER_SOURCES})
set_target_properties(NodeManager PROPERTIES OUTPUT_NAME "NodeManager")

target_include_directories(NodeManager PRIVATE
    ${SOURCE_DIR}
    ${VERSION_HEADER_DIR}
    ${CMAKE_SOURCE_DIR}/third_part
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/cxx17
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/no_tls
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/future
    ${CMAKE_SOURCE_DIR}/third_part/redis-plus-plus/src/sw/redis++/future/boost
    ${CMAKE_SOURCE_DIR}/third_part/spdlog/include
)

if(MSVC)
    target_include_directories(NodeManager PRIVATE
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/include
        ${CMAKE_SOURCE_DIR}/third_part/iconv_shim
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/build_win
    )
    target_link_libraries(NodeManager PRIVATE
        hiredis::hiredis
        redis++_static
        ${CMAKE_SOURCE_DIR}/third_part/libxml2/build_win/Debug/libxml2d.lib
    )
else()
    target_link_libraries(NodeManager PRIVATE
        PkgConfig::LIBXML2
        PkgConfig::HIREDIS
        redis++_static
        pthread
        dl
    )
endif()
