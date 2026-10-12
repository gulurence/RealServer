@echo off
REM build_wavebin.bat - compile and run S1 wave plist→bin unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_wavebin.exe test_wavebin.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_wavebin.exe
