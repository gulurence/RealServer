@echo off

md "Out/Msg"

cd ./Tools/

for %%i in (.\..\MsgProto\MsgProto\*.proto) do (
   echo gen %%i
   protoc_simple_client.exe --proto_path=..\MsgProto\MsgProto --csharp_out=.\..\Out\Msg  %%i
   )


xcopy  /y .\..\Out\Msg ..\..\client\SurvivaAndlKill\Assets\Script\Generate\MsgProto /s /e

rd /s /q ".\..\Out"

:: 旧项目目录
protoc.exe --cpp_out=../../server/ws-server/proto --proto_path=../MsgProto/PvpProto ../MsgProto/PvpProto/*.proto
:: 新项目目录
protoc.exe --cpp_out=../../server/ws-server-scene/proto --proto_path=../MsgProto/PvpProto ../MsgProto/PvpProto/*.proto

echo "generate files successful!"

pause