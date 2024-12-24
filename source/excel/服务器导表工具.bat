@echo off
path = %path%;
@echo on

rd /s /q "Out"
md "Out/go"
md "Out/cpp"
md "Out/pb"
md "Out/proto"
md "Out/cs"

cd ./Tools/

xlsx2dat.exe -G=./../Excels -P=./../Out/proto -D=./../Out/pb -N=TX -S=s

protoc.exe --cpp_out=../../proto/excel --gogofaster_out=../../proto/excel --proto_path=../Out/proto ../Out/proto/*.proto

protoc.exe --gogofaster_out=../../proto/excel --proto_path=../Out/proto ../Out/proto/*.proto

::copy /y .\..\Out\pb\*.* ..\..\server\tx-server\island\pb\

@echo Generate Done!
::rd /s /q ".\..\Out"
pause
