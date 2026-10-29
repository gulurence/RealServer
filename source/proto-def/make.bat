@echo off
REM ============================================================
REM  Regenerate protobuf C++ sources (protoc -> source/proto-cpp)
REM  Run from source/proto-def directory.
REM    protoc.exe      : libprotoc 29.3 (bundled in this dir)
REM ============================================================

set PROTOC=.\protoc.exe
set OUT=../proto-cpp/protocol

REM --- MSG protocols (--cpp_out only) ---
%PROTOC% --cpp_out=%OUT% --proto_path=. msg/msg_id.proto msg/account.proto msg/login.proto msg/player.proto msg/player_item.proto msg/friend.proto msg/room.proto msg/chat.proto msg/battle.proto msg/spectator.proto msg/config.proto
if errorlevel 1 goto :err

REM --- RPC protocols (--cpp_out only; service 块不生成 C++，已弃用 gRPC) ---
%PROTOC% --cpp_out=%OUT% --proto_path=. rpc/actor_mgr.proto
if errorlevel 1 goto :err

echo Done. Generated C++ sources under %OUT%
goto :eof

:err
echo protoc failed with errorlevel %errorlevel%
exit /b 1
