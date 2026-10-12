@echo off
REM build_config.bat - compile and run config unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_config.exe test_config.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_config.exe
