# smoke_test_e.ps1 — 联网对战冒烟测试编排器 (单轮, 修正为 E:\RealServer 路径)
#
# 与 smoke_test.ps1 相同逻辑, 仅把写死的 D:\code\RealServer 改为 E:\RealServer。
# 用法:
#   powershell -ExecutionPolicy Bypass -File env_root/smoke_test_e.ps1
#   powershell -ExecutionPolicy Bypass -File env_root/smoke_test_e.ps1 -MaxSec 120
#   powershell -ExecutionPolicy Bypass -File env_root/smoke_test_e.ps1 -KillAfter
#
# 退出码: 0=SMOKE PASS  2=CRASH(栈已打印)  3=TIMEOUT(报告卡点)  4=前置缺失
param(
    [int]$MaxSec = 120,
    [int]$FrameTarget = 200,
    [ValidateSet("Jit", "Now", "Off")]
    [string]$AttachMode = "Jit",
    [int]$Level = 0,
    [switch]$KillAfter
)

$ErrorActionPreference = "Continue"
$exe        = "E:\RealServer\client\soulofgun\Debug.win32\HelloCpp.exe"
$wd         = "E:\RealServer\client\soulofgun\Debug.win32"
$flowLog    = Join-Path $wd "sog_flow.log"
$crashLog   = Join-Path $wd "sog_crash.log"
$pidFile    = "E:\RealServer\env_root\smoke_pids.txt"
$resultFile = "E:\RealServer\env_root\smoke_result.txt"

function Write-Result([string]$verdict, [string]$detail) {
    Set-Content -Path $resultFile -Value "$verdict`t$detail"
    Write-Host ""
    Write-Host "================ SMOKE $verdict ================"
    Write-Host $detail
    Write-Host "================================================"
}

function Grep-Log([string]$pattern) {
    if (-not (Test-Path $flowLog)) { return $false }
    return (Select-String -Path $flowLog -Pattern $pattern -Quiet)
}

# 0) 前置检查
if (-not (Test-Path $exe)) { Write-Result "FAIL" "exe not found: $exe"; exit 4 }

# 清理上一轮本脚本启动的客户端
if (Test-Path $pidFile) {
    foreach ($p in (Get-Content $pidFile | Where-Object { $_ -match '^\d+$' })) {
        $proc = Get-Process -Id $p -ErrorAction SilentlyContinue
        if ($proc -and $proc.ProcessName -eq "HelloCpp") {
            Write-Host "killing previous smoke client pid=$p"
            Stop-Process -Id $p -Force -ErrorAction SilentlyContinue
        }
    }
}

# 1) 清日志
if (Test-Path $flowLog)  { Remove-Item $flowLog -Force }
if (Test-Path $crashLog) { Remove-Item $crashLog -Force }

# 2) 起房主 + 队友
$hostArgs = @("--auto","--host","--account","admin","--password","admin")
if ($Level -gt 0) { $hostArgs += @("--level", "$Level") }
$hostProc = Start-Process -FilePath $exe -ArgumentList $hostArgs -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds 3
$joinProc = Start-Process -FilePath $exe -ArgumentList "--auto","--join","--account","test001","--password","test001" -WorkingDirectory $wd -PassThru
$hostPid = $hostProc.Id
$joinPid = $joinProc.Id
Set-Content -Path $pidFile -Value "$hostPid`n$joinPid"
Write-Host "HOST pid=$hostPid (admin -> player 46001)   JOIN pid=$joinPid (test001 -> player 47001)"

# 3) 附加 VS2026
switch ($AttachMode) {
    "Now" {
        foreach ($p in @($hostPid, $joinPid)) {
            Write-Host "attaching VS2026 to pid=$p"
            Start-Process -FilePath "C:\Windows\System32\vsjitdebugger.exe" -ArgumentList "-p", "$p" -ErrorAction SilentlyContinue
        }
    }
    "Jit" { Write-Host "AttachMode=Jit: crash handler writes sog_crash.log first" }
    "Off" { Write-Host "AttachMode=Off: rely on crash handler only" }
}

# 4) 监控
$deadline = (Get-Date).AddSeconds($MaxSec)
$verdict  = $null
$detail   = ""
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2

    if ((Test-Path $crashLog) -and ((Get-Item $crashLog).Length -gt 0)) {
        $verdict = "CRASH"
        $detail  = "sog_crash.log present`n" + (Get-Content $crashLog -Raw)
        break
    }

    $hostAlive = Get-Process -Id $hostPid -ErrorAction SilentlyContinue
    $joinAlive = Get-Process -Id $joinPid -ErrorAction SilentlyContinue
    if ((-not $hostAlive) -or (-not $joinAlive)) {
        $which   = if (-not $hostAlive) { "HOST" } else { "JOIN" }
        $verdict = "CRASH"
        $detail  = "$which process exited unexpectedly before milestone (no sog_crash.log captured)"
        break
    }

    $hostFrame = Grep-Log ("\[pid=$hostPid .*input frame=[2-9][0-9]{2}")
    $joinFrame = Grep-Log ("\[pid=$joinPid .*input frame=[2-9][0-9]{2}")
    if ($hostFrame -and $joinFrame) {
        $verdict = "PASS"
        $detail  = "both players reached battle frame>=$FrameTarget (host=$hostPid join=$joinPid)"
        break
    }
}

if (-not $verdict) {
    $hostMark = if (Grep-Log ("\[pid=$hostPid .*onEnterRsp: ec=0"))        { "battle-enter" }
                elseif (Grep-Log ("\[pid=$hostPid .*build: DONE"))         { "battle-build" }
                elseif (Grep-Log ("\[pid=$hostPid .*START_RSP OK"))        { "room-start" }
                elseif (Grep-Log ("\[pid=$hostPid .*CREATE_ROOM_RSP OK"))  { "room-create" }
                elseif (Grep-Log ("\[pid=$hostPid .*LOGIN_RSP OK"))        { "login" }
                else { "before-login" }
    $joinMark = if (Grep-Log ("\[pid=$joinPid .*onEnterRsp: ec=0"))        { "battle-enter" }
                elseif (Grep-Log ("\[pid=$joinPid .*build: DONE"))         { "battle-build" }
                elseif (Grep-Log ("\[pid=$joinPid .*JOIN_ROOM_RSP OK"))    { "room-join" }
                elseif (Grep-Log ("\[pid=$joinPid .*LOGIN_RSP OK"))        { "login" }
                else { "before-login" }
    $verdict = "TIMEOUT"
    $detail  = "host reached [$hostMark], join reached [$joinMark] (deadline ${MaxSec}s)"
}

if ($KillAfter) {
    foreach ($p in @($hostPid, $joinPid)) {
        Stop-Process -Id $p -Force -ErrorAction SilentlyContinue
    }
    Write-Host "killed this round's clients ($hostPid, $joinPid)"
}

Write-Result $verdict $detail

switch ($verdict) {
    "PASS"    { exit 0 }
    "CRASH"   { exit 2 }
    "TIMEOUT" { exit 3 }
    default   { exit 3 }
}
