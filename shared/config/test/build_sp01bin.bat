@echo off
REM build_sp01bin.bat - compile and run S3 enemyInfo→bin unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_sp01bin.exe test_sp01bin.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_sp01bin.exe
