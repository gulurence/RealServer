@echo off
path = %path%;
@echo on


TortoiseProc.exe /command:update /path:../designer/ /closeonend:1

cd ./Tools/

binary2text.exe  ../../client/SurvivaAndlKill/Assets/Arts/Scenes/UnityScenes/1001/1001/NavMesh.asset ..\..\designer\pb\map\1001.asset.txt
binary2text.exe  ../../client/SurvivaAndlKill/Assets/Arts/Scenes/UnityScenes/1001/1002/NavMesh.asset ..\..\designer\pb\map\1002.asset.txt
binary2text.exe  ../../client/SurvivaAndlKill/Assets/Arts/Scenes/UnityScenes/1003/1003/NavMesh.asset ..\..\designer\pb\map\1003.asset.txt
binary2text.exe  ../../client/SurvivaAndlKill/Assets/Arts/Scenes/UnityScenes/1004/1004/NavMesh.asset ..\..\designer\pb\map\1004.asset.txt

copy /y .\..\..\client\SurvivaAndlKill\ExportedMap\*.obj ..\..\designer\pb\map\
copy /y .\..\..\client\SurvivaAndlKill\ExportedMap\*.bin ..\..\designer\pb\map\

copy /y .\..\..\client\SurvivaAndlKill\ExportedMap\*.obj ..\Map\
copy /y .\..\..\client\SurvivaAndlKill\ExportedMap\*.bin ..\Map\

copy /y .\..\Map\*.lua ..\..\designer\pb\map\

@echo Generate Done!

TortoiseProc.exe /command:commit /path:../../designer/pb /closeonend:1
TortoiseProc.exe /command:commit /path:../../designer/script /closeonend:1
TortoiseProc.exe /command:commit /path:../Map /closeonend:1
pause
