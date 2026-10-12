@echo off
REM build_hashverify.bat - compile and run hash verify + snapshot resync (F3) unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_hashverify.exe test_hashverify.cpp ..\BattleCore.cpp ..\BattleGame.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_hashverify.exe
