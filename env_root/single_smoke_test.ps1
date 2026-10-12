# single_smoke_test.ps1 — 单机切核心 (M4-a) 冒烟测试编排器
#
# 流程: 清日志 → 起单机客户端 (--single, 直接进 level13) → 监控
#        崩溃(读 sog_crash.log) / 通关(核心打到 frame>=200 或胜负终态) / 超时(报告卡点)
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File EnvRoot/single_smoke_test.ps1              # 一轮
#   powershell -ExecutionPolicy Bypass -File EnvRoot/single_smoke_test.ps1 -MaxSec 120  # 放宽超时
#
# 退出码: 0=SMOKE PASS  2=CRASH  3=TIMEOUT  4=前置缺失
#
# 判据 (M4-a 单机切核心):
#   - 崩溃: sog_crash.log 出现 / 进程异常退出 (未到里程碑就死)
#   - 通过: [SingleCore] battlecore ready (init 成功) 且
#           ([SingleCore] frame=2xx 打点 或 [SingleCore] WIN/FAILED 终态)
#   - 超时: 卡在 loading / 教学 / 核心未起
# -----------------------------------------------------------------------------------
param(
    [int]$MaxSec = 90
)

$ErrorActionPreference = "Continue"
$exe        = "D:\code\RealServer\client\soulofgun\Debug.win32\HelloCpp.exe"
$wd         = "D:\code\RealServer\client\soulofgun\Debug.win32"
$flowLog    = Join-Path $wd "sog_flow.log"
$crashLog   = Join-Path $wd "sog_crash.log"
$pidFile    = "D:\code\RealServer\EnvRoot\single_smoke_pid.txt"
$resultFile = "D:\code\RealServer\EnvRoot\single_smoke_result.txt"

function Write-Result([string]$verdict, [string]$detail) {
    Set-Content -Path $resultFile -Value "$verdict`t$detail"
    Write-Host ""
    Write-Host "================ SINGLE SMOKE $verdict ================"
    Write-Host $detail
    Write-Host "========================================================"
}

function Grep-Log([string]$pattern) {
    if (-not (Test-Path $flowLog)) { return $false }
    return (Select-String -Path $flowLog -Pattern $pattern -Quiet)
}

# 0) 前置检查
if (-not (Test-Path $exe)) { Write-Result "FAIL" "exe not found: $exe"; exit 4 }

# 清理上一轮本脚本启动的客户端 (只杀 pid 文件里的进程, 绝不碰其它 HelloCpp PID)
if (Test-Path $pidFile) {
    foreach ($p in (Get-Content $pidFile | Where-Object { $_ -match '^\d+$' })) {
        $proc = Get-Process -Id $p -ErrorAction SilentlyContinue
        if ($proc -and $proc.ProcessName -eq "HelloCpp") {
            Write-Host "killing previous single smoke client pid=$p"
            Stop-Process -Id $p -Force -ErrorAction SilentlyContinue
        }
    }
}

# 1) 清日志
if (Test-Path $flowLog)  { Remove-Item $flowLog -Force }
if (Test-Path $crashLog) { Remove-Item $crashLog -Force }

# 2) 起单机客户端 (--single 直接进 level13)
$proc = Start-Process -FilePath $exe -ArgumentList "--single" -WorkingDirectory $wd -PassThru
$clientPid = $proc.Id
Set-Content -Path $pidFile -Value "$clientPid"
Write-Host "SINGLE client pid=$clientPid (--single -> level13)"

# 3) 监控
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
    $alive = Get-Process -Id $clientPid -ErrorAction SilentlyContinue
    if (-not $alive) {
        $verdict = "CRASH"
        $detail  = "process exited unexpectedly before milestone (no sog_crash.log captured)"
        break
    }

    # 通关检测: 核心 init 成功 且 (打到 frame>=200 / 胜负终态)
    $ready = Grep-Log "\[SingleCore\] battlecore ready"
    $frame200 = Grep-Log "\[SingleCore\] frame=2[0-9][0-9]"
    $terminal = (Grep-Log "\[SingleCore\] WIN") -or (Grep-Log "\[SingleCore\] FAILED")
    if ($ready -and ($frame200 -or $terminal)) {
        $verdict = "PASS"
        $detail  = "single core ready + $(if ($terminal) {'terminal'} else {'frame>=200'}) (pid=$clientPid)"
        break
    }
}

# 超时兜底: 报告推进到哪一步
if (-not $verdict) {
    $mark = if (Grep-Log "\[SingleCore\] battlecore ready") { "core-ready" }
            elseif (Grep-Log "\[loading\] step5")              { "loading-step5" }
            elseif (Grep-Log "\[loading\] step2")              { "loading-step2" }
            else { "before-bootstrap" }
    $verdict = "TIMEOUT"
    $detail  = "single client stuck at [$mark] (deadline ${MaxSec}s)"
}

# 4) 结束本轮客户端
$alive = Get-Process -Id $clientPid -ErrorAction SilentlyContinue
if ($alive) {
    Stop-Process -Id $clientPid -Force -ErrorAction SilentlyContinue
    Write-Host "killed single client pid=$clientPid"
}

Write-Result $verdict $detail

switch ($verdict) {
    "PASS"    { exit 0 }
    "CRASH"   { exit 2 }
    "TIMEOUT" { exit 3 }
    default   { exit 3 }
}
