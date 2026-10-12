@echo off
REM build_wave_exporter.bat - compile S1 wave plist→bin exporter (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:wave_exporter.exe wave_exporter.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
