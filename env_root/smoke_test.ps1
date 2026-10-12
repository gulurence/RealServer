# smoke_test.ps1 — 联网对战冒烟测试编排器 (单轮)
#
# 流程: 清日志 → 起房主(admin) + 队友(test001) → 附加 VS2026 → 监控
#        崩溃(读 sog_crash.log 符号化栈) / 通关(两玩家都跑到 input frame>=200) / 超时(报告卡点)
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File EnvRoot/smoke_test.ps1                 # 一轮, 默认 JIT 附加
#   powershell -ExecutionPolicy Bypass -File EnvRoot/smoke_test.ps1 -MaxSec 120     # 放宽超时
#   powershell -ExecutionPolicy Bypass -File EnvRoot/smoke_test.ps1 -AttachMode Now # 立即附加 VS2026
#   powershell -ExecutionPolicy Bypass -File EnvRoot/smoke_test.ps1 -KillAfter      # 结束杀本轮客户端
#
# 退出码: 0=SMOKE PASS  2=CRASH(栈已打印)  3=TIMEOUT(报告卡点)  4=前置缺失
#
# -----------------------------------------------------------------------------------
# 崩溃栈抓取说明:
#   客户端内置崩溃处理器 (AppDelegate.cpp sogCrashFilter, dbghelp StackWalk64+SymFromAddr)
#   在未处理异常时把符号化调用栈写入 Debug.win32/sog_crash.log —— 全程无需调试器/管理员权限,
#   无人值守循环据此自动定位。附加 VS2026 有三种模式:
#     Jit (默认): 崩溃处理器先落盘 sog_crash.log, 之后 WER 按系统 JIT 调试器自动拉起 VS2026 ——
#                 既保证自动抓栈, 又满足"崩溃后 VS 自动出现"。
#     Now:        用 vsjitdebugger -p <pid> 立即附加到两个进程。适合人工盯着看; 注意 VS 先于
#                 崩溃处理器拦截异常, sog_crash.log 可能不落盘, 自动抓栈会失效。
#     Off:        不附加 VS, 只靠崩溃处理器抓栈 (最利于无人值守循环)。
# -----------------------------------------------------------------------------------
param(
    [int]$MaxSec = 90,
    [int]$FrameTarget = 200,
    [ValidateSet("Jit", "Now", "Off")]
    [string]$AttachMode = "Jit",
    [int]$Level = 0,
    [switch]$KillAfter
)

$ErrorActionPreference = "Continue"
$exe        = "D:\code\RealServer\client\soulofgun\Debug.win32\HelloCpp.exe"
$wd         = "D:\code\RealServer\client\soulofgun\Debug.win32"
$flowLog    = Join-Path $wd "sog_flow.log"
$crashLog   = Join-Path $wd "sog_crash.log"
$pidFile    = "D:\code\RealServer\EnvRoot\smoke_pids.txt"
$resultFile = "D:\code\RealServer\EnvRoot\smoke_result.txt"

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

# 清理上一轮本脚本启动的客户端 (只杀 pid 文件里的进程, 绝不碰僵尸 HelloCpp PID)
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

# 2) 起房主 + 队友 (房主创建房间时上送关卡, 队友经 ENTER_RSP 权威回传获得同关卡)
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
    "Jit" {
        Write-Host "AttachMode=Jit: crash handler writes sog_crash.log first, then VS2026 (JIT) auto-launches"
    }
    "Off" {
        Write-Host "AttachMode=Off: rely on crash handler only (best for unattended loop)"
    }
}

# 4) 监控
$deadline = (Get-Date).AddSeconds($MaxSec)
$verdict  = $null
$detail   = ""
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2

    # 崩溃检测: 崩溃处理器已写 sog_crash.log
    if ((Test-Path $crashLog) -and ((Get-Item $crashLog).Length -gt 0)) {
        $verdict = "CRASH"
        $detail  = "sog_crash.log present`n" + (Get-Content $crashLog -Raw)
        break
    }

    # 进程异常退出 (没到通关里程碑就死, 且没写崩溃日志 -> 视为崩溃)
    $hostAlive = Get-Process -Id $hostPid -ErrorAction SilentlyContinue
    $joinAlive = Get-Process -Id $joinPid -ErrorAction SilentlyContinue
    if ((-not $hostAlive) -or (-not $joinAlive)) {
        $which   = if (-not $hostAlive) { "HOST" } else { "JOIN" }
        $verdict = "CRASH"
        $detail  = "$which process exited unexpectedly before milestone (no sog_crash.log captured)"
        break
    }

    # 通关检测: 两玩家都跑到 input frame>=FrameTarget (20Hz 固定步长真实推进)
    $hostFrame = Grep-Log ("\[pid=$hostPid .*input frame=[2-9][0-9]{2}")
    $joinFrame = Grep-Log ("\[pid=$joinPid .*input frame=[2-9][0-9]{2}")
    if ($hostFrame -and $joinFrame) {
        $verdict = "PASS"
        $detail  = "both players reached battle frame>=$FrameTarget (host=$hostPid join=$joinPid)"
        break
    }
}

# 超时兜底: 报告每个客户端最后推进到哪一步
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

# 5) 结束本轮客户端 (可选)
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
