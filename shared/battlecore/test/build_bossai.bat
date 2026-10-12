@echo off
REM build_bossai.bat - compile and run boss AI unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_bossai.exe test_bossai.cpp ..\BattleCore.cpp ..\BattleGame.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_bossai.exe
