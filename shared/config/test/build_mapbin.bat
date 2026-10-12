@echo off
REM build_mapbin.bat - compile and run S2 collision TMX→bin unit test (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:test_mapbin.exe test_mapbin.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
.\test_mapbin.exe
