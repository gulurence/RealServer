# launch_smoke_test.ps1 — 自动化冒烟测试：起两个客户端，房主建房 / 队友加入
#
# 前置：服务器集群(4 进程) + Redis + WebPlatform(8080) 已起（launch_cluster.ps1）
# 用法：powershell -ExecutionPolicy Bypass -File EnvRoot/launch_smoke_test.ps1
# 账号：admin/admin(房主)  test001/test001(队友)
$exe = "D:\code\RealServer\client\soulofgun\Debug.win32\HelloCpp.exe"
$wd  = "D:\code\RealServer\client\soulofgun\Debug.win32"
$log = Join-Path $wd "sog_flow.log"

# 1. 清理旧客户端 + 旧日志
Get-Process HelloCpp -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
if (Test-Path $log) { Remove-Item $log -Force }

# 2. 起房主（admin/admin）
Start-Process -FilePath $exe -ArgumentList "--auto","--host","--account","admin","--password","admin" -WorkingDirectory $wd -WindowStyle Normal
Write-Output "launched HOST  account=admin"
Start-Sleep -Seconds 3

# 3. 起队友（test001/test001）
Start-Process -FilePath $exe -ArgumentList "--auto","--join","--account","test001","--password","test001" -WorkingDirectory $wd -WindowStyle Normal
Write-Output "launched JOIN  account=test001"
Write-Output "flow log: $log"
