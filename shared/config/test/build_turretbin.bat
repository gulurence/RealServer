@echo off
REM build_turretbin.bat - compile and run S4 turret enemyInfo/TMX→bin unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_turretbin.exe test_turretbin.cpp ..\..\battlecore\BattleCore.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_turretbin.exe
