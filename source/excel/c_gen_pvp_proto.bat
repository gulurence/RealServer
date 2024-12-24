@echo off

md "Out/Pvp"

cd ./Tools/

for %%i in (.\..\MsgProto\PvpProto\*.proto) do (
   echo gen %%i
   protoc_simple_client.exe --proto_path=..\MsgProto\PvpProto --csharp_out=.\..\Out\Pvp  %%i
   )

xcopy  /y .\..\Out\Pvp\WsPvp.cs ..\..\client\SurvivaAndlKill\Assets\Script\Generate\PvpProto /s /e
xcopy  /y .\..\Out\Pvp\WsScene.cs ..\..\client\SurvivaAndlKill\Assets\Script\Generate\PvpProto /s /e
xcopy  /y .\..\Out\Pvp\WsEntityTemplate.cs ..\..\client\SurvivaAndlKill\Assets\Script\Generate\PvpProto /s /e

rd /s /q ".\..\Out"

echo "generate client files successful!"

pause