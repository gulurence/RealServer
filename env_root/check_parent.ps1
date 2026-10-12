# check_parent.ps1 — 查 PID 13688 的父进程，判断是否 VS 调试目标
param([int]$ProcId = 13688)
$c = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcId" -ErrorAction SilentlyContinue
if (-not $c) { Write-Output "cannot query PID $ProcId"; exit 0 }
Write-Output ("Name=" + $c.Name)
Write-Output ("Pid=" + $c.ProcessId)
Write-Output ("ParentPid=" + $c.ParentProcessId)
Write-Output ("CmdLine=" + $c.CommandLine)
Write-Output ("ExecutablePath=" + $c.ExecutablePath)
Write-Output ("CreationDate=" + $c.CreationDate)
if ($c.ParentProcessId) {
    $pp = Get-CimInstance Win32_Process -Filter "ProcessId=$($c.ParentProcessId)" -ErrorAction SilentlyContinue
    if ($pp) { Write-Output ("ParentName=" + $pp.Name + " ParentCmd=" + $pp.CommandLine) }
    else { Write-Output ("ParentPid=$($c.ParentProcessId) already gone") }
}
