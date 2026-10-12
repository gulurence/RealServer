#!/bin/bash
cd /d/code/RealServer || exit 1
export PATH="/d/code/vcpkg/realserver-deps/installed/x64-windows/debug/bin:/d/code/vcpkg/realserver-deps/installed/x64-windows/bin:$PATH"
./build_msvc/engine/Debug/RealServer.exe --type=100 --name=gate-01 --platform=dev > logs/gate.log 2>&1
