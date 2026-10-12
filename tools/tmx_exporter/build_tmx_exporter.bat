@echo off
REM build_tmx_exporter.bat - compile S2 collision TMX→bin exporter (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:tmx_exporter.exe tmx_exporter.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
