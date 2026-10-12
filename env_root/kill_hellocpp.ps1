# kill_hellocpp.ps1 — 强力终止所有 HelloCpp.exe（含僵尸/崩溃进程）
$procs = Get-Process -Name HelloCpp -ErrorAction SilentlyContinue
if (-not $procs) { Write-Output "no HelloCpp"; exit 0 }

$id = [System.Security.Principal.WindowsIdentity]::GetCurrent()
$pr = New-Object System.Security.Principal.WindowsPrincipal($id)
Write-Output ("CurrentUser=" + $id.Name + " IsAdmin=" + $pr.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator))

foreach ($p in $procs) {
    Write-Output ("proc Id=" + $p.Id + " Path=" + $p.Path + " StartTime=" + $p.StartTime + " HasExited=" + $p.HasExited)
    # 1) 常规强杀
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
}

Start-Sleep -Seconds 1

# 2) 仍残留则用 taskkill /T /F 再补一刀
$left = Get-Process -Name HelloCpp -ErrorAction SilentlyContinue
if ($left) {
    foreach ($p in $left) {
        Write-Output ("retry kill Id=" + $p.Id)
        & taskkill /PID $p.Id /T /F 2>&1 | Out-Null
    }
    Start-Sleep -Seconds 1
}

$left2 = Get-Process -Name HelloCpp -ErrorAction SilentlyContinue
if ($left2) {
    Write-Output "STILL ALIVE:"
    $left2 | ForEach-Object { Write-Output ("  Id=" + $_.Id) }
    exit 1
} else {
    Write-Output "all killed"
    exit 0
}
