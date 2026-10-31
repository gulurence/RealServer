@echo off
REM Start RealServer cluster (double-click). ASCII-only on purpose.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0start_cluster.ps1"
if errorlevel 1 (
  echo.
  echo [START FAILED] Check the error above, and make sure env_root Redis/MySQL are up.
  pause
)
