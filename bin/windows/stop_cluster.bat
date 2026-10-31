@echo off
REM Stop all RealServer processes (double-click). ASCII-only on purpose.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0stop_cluster.ps1"
