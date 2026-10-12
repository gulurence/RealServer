@echo off
setlocal EnableExtensions
title Clean Visual Studio Build Files (incl. PDB)

echo ============================================================
echo  Cleaning VS generated intermediate files (incl. PDB)
echo  in the current directory and all subdirectories.
echo ============================================================
echo.

choice /c YN /m "Delete all build intermediate files (obj/pdb/ilk...) and build dirs? [Y/N]"
if errorlevel 2 goto :eof

echo.
echo [1/3] Removing build output / intermediate directories...
for /d /r %%F in (*) do (
    if /i "%%~nxF"=="Debug"   rd /s /q "%%F" 2>nul
    if /i "%%~nxF"=="Release" rd /s /q "%%F" 2>nul
    if /i "%%~nxF"=="x64"     rd /s /q "%%F" 2>nul
    if /i "%%~nxF"=="x86"     rd /s /q "%%F" 2>nul
    if /i "%%~nxF"=="Win32"   rd /s /q "%%F" 2>nul
    if /i "%%~nxF"==".vs"     rd /s /q "%%F" 2>nul
    if /i "%%~nxF"=="ipch"    rd /s /q "%%F" 2>nul
    if /i "%%~nxF"=="obj"     rd /s /q "%%F" 2>nul
    if /i "%%~nxF"=="bin"     rd /s /q "%%F" 2>nul
)

echo [2/3] Deleting intermediate files (obj / pdb / ilk / idb / iobj / ipdb / pch / ipch / exp / res / tlog ...)...
for %%E in (obj pdb ilk idb iobj ipdb pch ipch exp res tlog lastbuildstate suo user sdf opensdf ncb aps) do (
    del /a /f /s /q "*.%%E" 2>nul
)

echo [3/3] Deleting VS IntelliSense database files (*.VC.db / *.VC.VC.opendb)...
del /a /f /s /q "*.VC.db"        2>nul
del /a /f /s /q "*.VC.VC.opendb" 2>nul

echo.
echo Cleanup finished.
pause
