@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d D:\code\RealServer\shared\battlecore\test
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_prop.exe test_prop.cpp ..\BattleCore.cpp ..\BattleGame.cpp
