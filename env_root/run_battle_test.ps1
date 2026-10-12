# run_battle_test.ps1 — 起两个客户端(房主+队友)进战斗, 不杀旧进程, 记录 PID
# 用法: powershell -ExecutionPolicy Bypass -File EnvRoot/run_battle_test.ps1
$exe = "D:\code\RealServer\client\soulofgun\Debug.win32\HelloCpp.exe"
$wd  = "D:\code\RealServer\client\soulofgun\Debug.win32"
$flowLog = Join-Path $wd "sog_flow.log"
$crashLog = Join-Path $wd "sog_crash.log"
$pidFile = "D:\code\RealServer\EnvRoot\battle_pids.txt"

# 清理日志(不杀任何进程)
if (Test-Path $flowLog)  { Remove-Item $flowLog -Force }
if (Test-Path $crashLog) { Remove-Item $crashLog -Force }

$hostProc = Start-Process -FilePath $exe -ArgumentList "--auto","--host","--account","admin","--password","admin" -WorkingDirectory $wd -PassThru
Write-Output "launched HOST  pid=$($hostProc.Id)  account=admin"
Start-Sleep -Seconds 3

$joinProc = Start-Process -FilePath $exe -ArgumentList "--auto","--join","--account","test001","--password","test001" -WorkingDirectory $wd -PassThru
Write-Output "launched JOIN  pid=$($joinProc.Id)  account=test001"

Set-Content -Path $pidFile -Value "host=$($hostProc.Id)`njoin=$($joinProc.Id)"
Write-Output "pids -> $pidFile"
