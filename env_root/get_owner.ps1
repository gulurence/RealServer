# get_owner.ps1 — 查 PID 13688 的所有者与完整性级别
param([int]$ProcId = 13688)
try {
    $owner = (Get-CimInstance Win32_Process -Filter "ProcessId=$ProcId").GetOwner()
    Write-Output ("Owner: " + $owner.Domain + "\" + $owner.User)
} catch {
    Write-Output ("GetOwner failed: " + $_.Exception.Message)
}
# 完整性级别（通过命令行工具 whoami /groups 对自身，这里只做进程状态快照）
$p = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcId"
Write-Output ("Name=" + $p.Name + " ExecPath=" + $p.ExecutablePath + " Cmd=" + $p.CommandLine)
Write-Output ("CreationDate=" + $p.CreationDate + " TermDate=" + $p.TerminationDate)
Write-Output ("ThreadCount=" + $p.ThreadCount + " HandleCount=" + $p.HandleCount)
