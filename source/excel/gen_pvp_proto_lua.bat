@echo off



cd ./Tools/

:: 旧项目目录
protoc.exe --cpp_out=../../server/ws-server/proto --proto_path=../MsgProto/PvpProto ../MsgProto/PvpProto/*.proto
:: 新项目目录
protoc.exe --cpp_out=../../server/ws-server-scene/proto --proto_path=../MsgProto/PvpProto ../MsgProto/PvpProto/*.proto

echo "generate server files successful!"


pause