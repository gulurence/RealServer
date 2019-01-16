echo " <<<<<<<<< begin copy to include dir ... .. .>>>>>>>>> "

cd /d %~dp0

XCOPY ..\src\xBase\*.h ..\include\libXBase\*.h /Y
XCOPY ..\src\xCommand\*.h ..\include\libXCommand\*.h /Y
XCOPY ..\src\xConfig\*.h ..\include\libXConfig\*.h /Y
XCOPY ..\src\xGame\*.h ..\include\libXGame\*.h /Y
XCOPY ..\src\xGameData\*.h ..\include\libXGameData\*.h /Y
XCOPY ..\src\xNet\*.h ..\include\libXNet\*.h /Y
XCOPY ..\src\xServer\*.h ..\include\libXServer\*.h /Y

echo " <<<<<<<<< to include copy success ! >>>>>>>>> "

XCOPY ..\lib\*.dll ..\..\Bin\*.dll /Y
XCOPY ..\lib\*.pdb ..\..\Bin\*.pdb /Y

