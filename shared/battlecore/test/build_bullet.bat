@echo off
REM build_bullet.bat - compile and run bullet unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_bullet.exe test_bullet.cpp ..\BattleCore.cpp ..\BattleGame.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_bullet.exe
