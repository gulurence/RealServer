@echo off
REM build_leveldata.bat - compile and run S5 LevelDataLoader real-bin injection unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_leveldata.exe test_leveldata.cpp ..\..\battlecore\BattleCore.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_leveldata.exe
