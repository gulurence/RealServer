@echo off
REM build_configmgr.bat - compile and run config manager (C3) unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_configmgr.exe test_configmgr.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_configmgr.exe
