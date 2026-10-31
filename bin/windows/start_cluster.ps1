# Start RealServer cluster (Windows / x64), fully self-contained under bin\windows.
# Prereq: env_root Redis Cluster (7000-7005) + MySQL (3306) are up.
# Working dir = bin\windows (this dir), because the code loads config/server.xml
# and script/ by relative path from the working directory.
# ASCII-only on purpose (PowerShell 5.1 reads .ps1 as GBK on this machine;
# UTF-8 Chinese comments would swallow newlines).
#
# NodeManager.exe is the machine daemon that launches/stops the RealServer child
# processes (gate/login/global/game/battle) by converging on
# cluster:desired:{type}.count in Redis. This script only starts NodeManager and
# writes the desired counts; NodeManager does the actual process launch (one
# RealServer per type, staggered 2s apart for RPC port allocation).

$ErrorActionPreference = 'Stop'

$binDir    = Split-Path -Parent $MyInvocation.MyCommand.Path   # bin\windows
$log       = Join-Path $binDir 'logs'
$machineId = 1   # single physical machine id -> NodeManager registers cluster:node:1

$nmExe = Join-Path $binDir 'NodeManager.exe'
$rsExe = Join-Path $binDir 'RealServer.exe'   # NodeManager launches this same-dir exe

if (-not (Test-Path $nmExe))                                { Write-Output "Missing NodeManager.exe: $nmExe"; exit 1 }
if (-not (Test-Path $rsExe))                                { Write-Output "Missing RealServer.exe: $rsExe"; exit 1 }
if (-not (Test-Path (Join-Path $binDir 'config\server.xml'))) { Write-Output "Missing config\server.xml"; exit 1 }
if (-not (Test-Path (Join-Path $binDir 'script')))            { Write-Output "Missing script\"; exit 1 }
# Battle level data (frame-sync core must match client bit-for-bit; missing -> frozen heroes)
foreach ($b in @('wave.bin','map_collision.bin','sp01.bin')) {
    $p = Join-Path $binDir "shared\config\data\$b"
    if (-not (Test-Path $p)) { Write-Output "Missing level data: $p"; exit 1 }
}
if (-not (Test-Path $log))                                  { New-Item -ItemType Directory -Force $log | Out-Null }

# Close any already-running processes first (clean restart, no duplicates).
# NodeManager first so it cannot relaunch children while we tear down.
$existingNM = Get-Process -Name 'NodeManager' -ErrorAction SilentlyContinue
$existingRS = Get-Process -Name 'RealServer*' -ErrorAction SilentlyContinue
if ($existingNM) { $existingNM | Stop-Process -Force; Write-Output ("Stopping {0} existing NodeManager process(es)..." -f $existingNM.Count) }
if ($existingRS) { $existingRS | Stop-Process -Force; Write-Output ("Stopping {0} existing RealServer process(es)..." -f $existingRS.Count) }
if ($existingNM -or $existingRS) { Start-Sleep -Seconds 2 }   # release ports/handles before relaunch

# Topology: one process per type (Gate=100 Login=101 Global=102 Game=201 Battle=202).
$types = @(100, 101, 102, 201, 202)

# Sync cluster metadata BEFORE starting NodeManager so it converges on the right
# desired counts immediately (avoids a brief over-launch from stale desired keys):
# write cluster:desired:{type}.count = 1, and drop zombie by_type members
# (empty hash or heartbeat older than 15s). Non-fatal if redis-cli is unavailable.
$redisCli = Join-Path $binDir '..\..\env_root\RedisCluster\redis-cli.exe'
if (-not (Test-Path $redisCli)) {
    $rc = Get-Command redis-cli -ErrorAction SilentlyContinue
    $redisCli = if ($rc) { $rc.Source } else { $null }
}
if ($redisCli) {
    function Invoke-Redis([string[]]$a) { & $redisCli -c -p 7000 @a 2>$null }
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    foreach ($t in $types) {
        $tk = [string]$t
        Invoke-Redis @('hset', "cluster:desired:$tk", 'count', '1', 'mode', 'manual') | Out-Null
        foreach ($id in (Invoke-Redis @('smembers', "cluster:process:by_type:$tk"))) {
            if (-not $id) { continue }
            $idv = ([string]$id).Trim()
            $hb = ([string](Invoke-Redis @('hget', "cluster:process:${tk}:${idv}", 'heartbeat'))).Trim()
            $stale = $true
            if ($hb -ne '') {
                $hbInt = [int64]0
                if ([int64]::TryParse($hb, [ref]$hbInt) -and (($now - $hbInt) -le 15)) { $stale = $false }
            }
            if ($stale) {
                Invoke-Redis @('srem', "cluster:process:by_type:$tk", $idv) | Out-Null
                Invoke-Redis @('del', "cluster:process:${tk}:${idv}") | Out-Null
                Write-Output ("cleaned zombie type={0} id={1}" -f $t, $idv)
            }
        }
    }
    Write-Output "cluster metadata synced (desired = 1 per type, zombies cleaned)"
} else {
    Write-Output "redis-cli not found; skipped cluster metadata sync"
}

# Launch NodeManager. It converges cluster:desired:{type}.count to actual
# RealServer children (reconcile every 2s, each child staggered 2s).
$nmArgs = @("--machine-id=$machineId", '--platform=dev', '--config=config/server.xml')
$nm = Start-Process -FilePath $nmExe `
    -ArgumentList $nmArgs `
    -WorkingDirectory $binDir `
    -RedirectStandardOutput "$log\nodemanager.out" -RedirectStandardError "$log\nodemanager.err" `
    -WindowStyle Hidden -PassThru
Write-Output ("launched NodeManager machine={0} pid={1}" -f $machineId, $nm.Id)

Write-Output ""
Write-Output "Cluster launching via NodeManager (pid $($nm.Id)). Logs: $log"
Write-Output "Types: 201=Game 202=Battle 102=Global 101=Login 100=Gate"
Write-Output "NodeManager will launch one RealServer per type within a few seconds."
