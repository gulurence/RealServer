@echo off
path = %path%;
@echo on

rd /s /q "Out"
md "Out/go"
md "Out/cpp"
md "Out/pb/battle"
md "Out/pb/island"
md "Out/proto/battle"
md "Out/proto/island"
md "Out/cs"

TortoiseProc.exe /command:update /path:../designer/pb /closeonend:1
TortoiseProc.exe /command:update /path:./Excels /closeonend:1

cd ./Excels/

for /f "delims=" %%i in ('svn info ^| findstr "Rev:"') do set rev=%%i
set rev=%rev:~18%
echo %rev%>../Out/Version.txt

cd ../Tools/

xlsx2dat.exe -G=./../Excels -P=./../Out/proto/battle -D=./../Out/pb -N=TX -S=s

:: 旧项目目录
protoc.exe --cpp_out=./../../server/ws-server/excel --gogofaster_out=./../../server/tx-server/comm/excel/proto --proto_path=./../Out/proto/battle ./../Out/proto/battle/*.proto
:: 新项目目录
protoc.exe --cpp_out=./../../server/ws-server-scene/excel --gogofaster_out=./../../server/tx-server/comm/excel/proto --proto_path=./../Out/proto/battle ./../Out/proto/battle/*.proto

protoc.exe --gogofaster_out=./../../server/tx-server/comm/excel/proto --proto_path=./../Out/proto/island ./../Out/proto/island/*.proto

copy /y .\..\Out\pb\*.* ..\..\designer\pb\
copy /y .\..\Out\Version.txt ..\..\designer\pb\Version.txt
copy /y .\..\Map\*.json ..\..\designer\pb\map\
copy /y .\..\Map\*.lua ..\..\designer\pb\map\
copy /y .\..\..\client\SurvivaAndlKill\ExportedMap\*.obj ..\..\designer\pb\map\
copy /y .\..\..\client\SurvivaAndlKill\ExportedMap\*.bin ..\..\designer\pb\map\

@echo Generate Done!
::rd /s /q ".\..\Out"

TortoiseProc.exe /command:commit /path:../../designer/pb /closeonend:1

pause
