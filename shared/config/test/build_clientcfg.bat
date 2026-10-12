@echo off
REM build_clientcfg.bat - compile and run client config pull (C4) unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_clientcfg.exe test_clientcfg.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_clientcfg.exe
