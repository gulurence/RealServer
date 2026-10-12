@echo off
REM build_sp01_exporter.bat - compile S3 enemyInfo plist→bin exporter (MSVC)
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /EHsc /W3 /std:c++14 /utf-8 /Fe:sp01_exporter.exe sp01_exporter.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
