#!/usr/bin/env bash
# =============================================================================
# RealServer run driver — 构建 / 启动 / 观测 / 停止 单进程服务器
#
# 用法:  driver.sh <command> [args]
#   build    配置(若未配置)并编译 Debug 产物
#   up       后台启动服务器, 等待 READY, 打印监听端口
#   status   显示运行状态 + 监听端口
#   probe    向网关端口发起一次 TCP 连接(验证网络层接受真实客户端)
#   logs     持续 tail 服务器日志 (Ctrl-C 退出)
#   down     停止服务器 (taskkill 按镜像名, 会杀掉所有 RealServer.exe)
#
# 所有命令都从仓库根目录运行, 因为引擎用相对路径加载 config/ 与 script/。
# =============================================================================
set -uo pipefail

SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SKILL_DIR/../../.." && pwd)"
EXE="$ROOT/build_msvc/engine/Debug/RealServer.exe"

# -----------------------------------------------------------------------------
# 运行时 DLL 搜索路径 (Windows/MSVC 动态链接)
#   - vcpkg:  grpc/protobuf/hiredis/abseil/cares/re2/zlib
#   - mysql:  libmysql.dll (import lib 对应)
#   - libxml2: libxml2d.dll
# vcpkg 前缀与 engine/CMakeLists.txt 里写死的 VCPKG_INSTALLED_ROOT 一致,
# 换机器时用环境变量 VCPKG_BIN 覆盖。
# -----------------------------------------------------------------------------
VCPKG_BIN="${VCPKG_BIN:-/d/code/vcpkg/realserver-deps/installed/x64-windows/debug/bin}"
MYSQL_LIB="$ROOT/third_part/mysql-connector-c-6.1.11-winx64/lib"
LIBXML2_DLL="$ROOT/third_part/libxml2/build_win/Debug"
export PATH="$VCPKG_BIN:$MYSQL_LIB:$LIBXML2_DLL:$PATH"

# 运行态产物目录 (服务器自己的日志在 logs/normal.log, 这里捕获 stdout)
LOG="$ROOT/logs/driver.log"

# =============================================================================
# 辅助函数
# =============================================================================
is_running() {
    tasklist //FI "IMAGENAME eq RealServer.exe" 2>/dev/null | grep -qi "RealServer.exe"
}

gateway_port() {
    grep -oE "listening on 0.0.0.0:[0-9]+" "$LOG" 2>/dev/null \
        | grep -oE "[0-9]+$" | head -1
}

# =============================================================================
# build — 配置(若需要) + 编译
# =============================================================================
cmd_build() {
    if [ ! -f "$ROOT/build_msvc/CMakeCache.txt" ]; then
        echo "[build] build_msvc 未配置, 执行 cmake configure..."
        (cd "$ROOT" && cmake -B build_msvc -G "Visual Studio 18 2026" -A x64) || return 1
    fi
    cmake --build "$ROOT/build_msvc" --config Debug
}

# =============================================================================
# up — 后台启动并等待 READY
# =============================================================================
cmd_up() {
    if is_running; then
        echo "[up] RealServer 已在运行, 先执行 down 再 up"
        return 1
    fi

    # config/server.xml 被 .gitignore 忽略, 干净检出时从 example 生成
    if [ ! -f "$ROOT/config/server.xml" ]; then
        echo "[up] config/server.xml 缺失, 从 server.xml.example 复制"
        cp "$ROOT/config/server.xml.example" "$ROOT/config/server.xml"
    fi

    if [ ! -f "$EXE" ]; then
        echo "[up] 未找到 $EXE — 先执行 driver.sh build"
        return 1
    fi

    : > "$LOG"   # 每次启动清空捕获日志, 避免上次 READY 造成误判
    (cd "$ROOT" && "$EXE" >> "$LOG" 2>&1) &
    echo "[up] 已启动 (stdout → $LOG), 等待 READY..."

    local i
    for i in $(seq 1 30); do
        grep -q "RealServer READY" "$LOG" && break
        sleep 1
    done

    if ! grep -q "RealServer READY" "$LOG"; then
        echo "[up] 30s 内未到达 READY, 最后 20 行日志:"
        tail -20 "$LOG"
        return 1
    fi

    echo "[up] READY"
    local gw; gw="$(gateway_port)"
    echo "[up] 监听: gRPC 50051, 网关 0.0.0.0:${gw:-<未知>}"
}

# =============================================================================
# status — 运行状态 + 监听端口
# =============================================================================
cmd_status() {
    if is_running; then
        echo "RUNNING"
    else
        echo "NOT RUNNING"
        return 0
    fi
    local gw; gw="$(gateway_port)"
    echo "--- 监听端口 ---"
    netstat -ano 2>/dev/null | grep -E ":(50051|${gw:-0}) " | grep LISTENING || true
    echo "--- 最后 5 行日志 ---"
    tail -5 "$LOG" 2>/dev/null || true
}

# =============================================================================
# probe — 向网关端口发起一次 TCP 连接并断开, 验证网络层接受真实客户端
# =============================================================================
cmd_probe() {
    if ! is_running; then
        echo "[probe] 服务器未运行"
        return 1
    fi
    local gw; gw="$(gateway_port)"
    if [ -z "$gw" ]; then
        echo "[probe] 日志里没找到网关端口"
        return 1
    fi
    # /dev/tcp 是 bash 内建; 连接成功后立刻关闭
    if exec 3<>"/dev/tcp/127.0.0.1/$gw"; then
        exec 3>&-; exec 3<&-
        sleep 1
        if grep -q "client disconnect" "$LOG"; then
            echo "[probe] OK — 网关 :$gw 接受连接并记录断开"
        else
            echo "[probe] 已连接 :$gw, 但日志未见 disconnect (仍可视为正常)"
        fi
    else
        echo "[probe] 无法连接网关 :$gw"
        return 1
    fi
}

# =============================================================================
# logs — 持续 tail
# =============================================================================
cmd_logs() {
    tail -f "$LOG"
}

# =============================================================================
# down — 停止
# =============================================================================
cmd_down() {
    if ! is_running; then
        echo "[down] 未在运行"
        return 0
    fi
    taskkill //F //IM RealServer.exe 2>&1 | sed 's/^/[down] /'
}

# =============================================================================
# 分发
# =============================================================================
case "${1:-}" in
    build)  cmd_build ;;
    up)     cmd_up ;;
    start)  cmd_up ;;
    status) cmd_status ;;
    probe)  cmd_probe ;;
    logs)   cmd_logs ;;
    log)    cmd_logs ;;
    down)   cmd_down ;;
    stop)   cmd_down ;;
    *)
        echo "用法: driver.sh <build|up|status|probe|logs|down>" >&2
        exit 2
        ;;
esac
