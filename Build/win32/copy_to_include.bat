echo " <<<<<<<<< begin copy to include dir ... .. .>>>>>>>>> "

cd /d %~dp0

XCOPY ..\..\Engine\src\xBase\*.h ..\..\Engine\include\libXBase\*.h /Y
XCOPY ..\..\Engine\src\xCommand\*.h ..\..\Engine\include\libXCommand\*.h /Y
XCOPY ..\..\Engine\src\xConfig\*.h ..\..\Engine\include\libXConfig\*.h /Y
XCOPY ..\..\Engine\src\xGame\*.h ..\..\Engine\include\libXGame\*.h /Y
XCOPY ..\..\Engine\src\xGameData\*.h ..\..\Engine\include\libXGameData\*.h /Y
XCOPY ..\..\Engine\src\xNet\*.h ..\..\Engine\include\libXNet\*.h /Y
XCOPY ..\..\Engine\src\xServer\*.h ..\..\Engine\include\libXServer\*.h /Y

XCOPY ..\..\Engine\src\xThiredTool\database\*.h ..\..\Engine\include\libXThiredTool\database\*.h /Y
XCOPY ..\..\Engine\src\xThiredTool\des\*.h ..\..\Engine\include\libXThiredTool\des\*.h /Y
XCOPY ..\..\Engine\src\xThiredTool\lua\*.h ..\..\Engine\include\libXThiredTool\lua\*.h /Y
XCOPY ..\..\Engine\src\xThiredTool\sha1\*.h ..\..\Engine\include\libXThiredTool\sha1\*.h /Y
XCOPY ..\..\Engine\src\xThiredTool\zlib\*.h ..\..\Engine\include\libXThiredTool\zlib\*.h /Y

echo " <<<<<<<<< to include copy success ! >>>>>>>>> "

XCOPY ..\..\Engine\lib\*.dll ..\..\Bin\*.dll /Y
XCOPY ..\..\Engine\lib\*.pdb ..\..\Bin\*.pdb /Y

