# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)

# 项目信息
project(application)

# 设置源代码路径
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# grpc
file(GLOB X_GRPCPROTO           "${CMAKE_CURRENT_SOURCE_DIR}/../source/proto-cpp/protocol/rpc/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/../source/proto-cpp/protocol/rpc/*.cc")
file(GLOB X_MSGPROTO            "${CMAKE_CURRENT_SOURCE_DIR}/../source/proto-cpp/protocol/msg/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/../source/proto-cpp/protocol/msg/*.cc")

# main
file(GLOB X_MAIN                "${SOURCE_DIR}/*.h"                         "${SOURCE_DIR}/*.cpp")

# service
file(GLOB X_SERVICE_CONFIG      "${SOURCE_DIR}/service/config/*.h"          "${SOURCE_DIR}/service/config/*.cpp")
file(GLOB X_SERVICE_LOG         "${SOURCE_DIR}/service/log/*.h"             "${SOURCE_DIR}/service/log/*.cpp")
file(GLOB X_SERVICE_PLATFORM    "${SOURCE_DIR}/service/platform/*.h"        "${SOURCE_DIR}/service/platform/*.cpp")
file(GLOB X_SERVICE_RECORD      "${SOURCE_DIR}/service/record/*.h"          "${SOURCE_DIR}/service/record/*.cpp")

# actor
file(GLOB X_ACTOR_GATE          "${SOURCE_DIR}/actor/gate/*.h"              "${SOURCE_DIR}/actor/gate/*.cpp")
file(GLOB X_ACTOR_LOGIN         "${SOURCE_DIR}/actor/login/*.h"              "${SOURCE_DIR}/actor/login/*.cpp")
file(GLOB X_ACTOR_PLAYER        "${SOURCE_DIR}/actor/player/*.h"             "${SOURCE_DIR}/actor/player/*.cpp")

# handle
file(GLOB X_HANDLE_GATE         "${SOURCE_DIR}/handle/gate/*.h"             "${SOURCE_DIR}/handle/gate/*.cpp")
file(GLOB X_HANDLE_LOGIN        "${SOURCE_DIR}/handle/login/*.h"            "${SOURCE_DIR}/handle/login/*.cpp")
file(GLOB X_HANDLE_PLAYER       "${SOURCE_DIR}/handle/player/*.h"           "${SOURCE_DIR}/handle/player/*.cpp")

# 将所有源文件添加到一个变量中
set(SOURCES
    ${X_GRPCPROTO}
    ${X_MSGPROTO}
    # main
    ${X_MAIN}
    # service
    ${X_SERVICE_CONFIG}
    ${X_SERVICE_LOG}
    ${X_SERVICE_PLATFORM}
    ${X_SERVICE_RECORD}
    # actor
    ${X_ACTOR_GATE}
    ${X_ACTOR_LOGIN}
    ${X_ACTOR_PLAYER}
    # handle
    ${X_HANDLE_GATE}
    ${X_HANDLE_LOGIN}
    ${X_HANDLE_PLAYER}
)

# 将源文件分组（在 Visual Studio 中显示为目录结构）
source_group("protobuf/rpc"         FILES ${X_GRPCPROTO})
source_group("protobuf/msg"         FILES ${X_MSGPROTO})
# main
source_group("main"                 FILES ${X_MAIN})
# service
source_group("service/config"       FILES ${X_SERVICE_CONFIG})
source_group("service/log"          FILES ${X_SERVICE_LOG})
source_group("service/platform"     FILES ${X_SERVICE_PLATFORM})
source_group("service/record"       FILES ${X_SERVICE_RECORD})
# actor
source_group("actor/gate"           FILES ${X_ACTOR_GATE})
source_group("actor/login"          FILES ${X_ACTOR_LOGIN})
source_group("actor/player"         FILES ${X_ACTOR_PLAYER})
# handle
source_group("handle/gate"          FILES ${X_HANDLE_GATE})
source_group("handle/login"         FILES ${X_HANDLE_LOGIN})
source_group("handle/player"        FILES ${X_HANDLE_PLAYER})

# 生成静态库（.lib 和 .a 文件）
if (MSVC)
    add_executable(application ${SOURCES})
    set_target_properties(application PROPERTIES OUTPUT_NAME "application")
    add_definitions("-DUNICODE" "-D_UNICODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /sdl /Zi /GR /FC /Od")
    add_definitions("/utf-8 /wd4200 /wd4099 /wd4267 /wd4244 /wd4521")
else()
    add_library(application STATIC ${SOURCES})
    set_target_properties(application PROPERTIES OUTPUT_NAME "libApplicationService")
endif()


# 设置头文件目录
target_include_directories(application PUBLIC 
    ${CMAKE_SOURCE_DIR}/src/engine   # 确保包含 engine 目录下的所有头文件
    ${CMAKE_SOURCE_DIR}/third_part  # 如果需要第三方库头文件
    ${CMAKE_SOURCE_DIR}/third_part/mysql-connector-c-6.1.11-winx64
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/proto-cpp/protocol
    #${CMAKE_SOURCE_DIR}/third_part/protobuf-3.19.2/src  # Protobuf 头文件路径
)

# 链接 engine 库
target_link_libraries(application PRIVATE engine)
