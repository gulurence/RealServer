# inspect_proc.ps1 — 查看 HelloCpp 进程详情
param([int]$ProcId = 13688)
$p = Get-Process -Id $ProcId -ErrorAction SilentlyContinue
if (-not $p) { Write-Output "Get-Process cannot see PID $ProcId"; exit 0 }
Write-Output ("Name=" + $p.ProcessName)
Write-Output ("Id=" + $p.Id)
Write-Output ("Path=" + $p.Path)
Write-Output ("StartTime=" + $p.StartTime)
Write-Output ("Responding=" + $p.Responding)
Write-Output ("Threads=" + $p.Threads.Count)
Write-Output ("HandleCount=" + $p.HandleCount)
Write-Output ("WorkingSet64=" + $p.WorkingSet64)
Write-Output ("SessionId=" + $p.SessionId)
Write-Output ("MainWindowTitle=" + $p.MainWindowTitle)
