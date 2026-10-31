# Start RealServer cluster with 3x Login + 3x Game (scale-out for stress test).
# Backend first, Gate last. ASCII-only (PowerShell 5.1 reads .ps1 as GBK).
#   Types: 201=Game 202=Battle 102=Global 101=Login 100=Gate
# Working dir = bin\windows (config/ + script/ loaded by relative path).

$ErrorActionPreference = 'Stop'

$binDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$log    = Join-Path $binDir 'logs'

# Same exe as currently running cluster: newest versioned release under releases/
$exe = ''
$version = ''
$releasesDir = Join-Path $binDir 'releases'
if (Test-Path $releasesDir) {
    $latest = Get-ChildItem -Path $releasesDir -Filter 'RealServer_*.exe' |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($latest) {
        $exe = $latest.FullName
        if ($latest.Name -match 'RealServer_(.+)\.exe$') { $version = $Matches[1] }
    }
}
if (-not $exe -or -not (Test-Path $exe)) { $exe = Join-Path $binDir 'RealServer.exe' }
if (-not (Test-Path $exe)) { Write-Output "Missing RealServer.exe: $exe"; exit 1 }
if (-not (Test-Path (Join-Path $binDir 'config\server.xml'))) { Write-Output "Missing config\server.xml"; exit 1 }
if (-not (Test-Path (Join-Path $binDir 'script')))            { Write-Output "Missing script\"; exit 1 }
if (-not (Test-Path $log)) { New-Item -ItemType Directory -Force $log | Out-Null }

Write-Output ("exe = {0}" -f $exe)
if ($version) { Write-Output ("version = {0}" -f $version) }

# Clean restart: stop all RealServer first, then reset local.info (port registry)
# to avoid port-range exhaustion / RPC port conflicts across repeated restarts.
$existing = Get-Process -Name 'RealServer*' -ErrorAction SilentlyContinue
if ($existing) {
    Write-Output ("Stopping {0} existing RealServer process(es)..." -f $existing.Count)
    $existing | Stop-Process -Force
    Start-Sleep -Seconds 2
}

$localInfo = Join-Path $binDir 'local.info'
if (Test-Path $localInfo) {
    Copy-Item $localInfo "$localInfo.bak" -Force
    Set-Content -Path $localInfo -Value '# RealServer local node registry (reset by start_cluster_3x3.ps1)' -Encoding ASCII
    Write-Output "local.info reset (backup: local.info.bak)"
}

# Topology: 3 Game, 1 Battle, 1 Global, 3 Login, 1 Gate
$jobs = @(
    @{ type = 201; name = 'game-01'   },
    @{ type = 201; name = 'game-02'   },
    @{ type = 201; name = 'game-03'   },
    @{ type = 202; name = 'battle-01'; port = 10081 },
    @{ type = 102; name = 'global-01' },
    @{ type = 101; name = 'login-01'  },
    @{ type = 101; name = 'login-02'  },
    @{ type = 101; name = 'login-03'  },
    @{ type = 100; name = 'gate-01';   port = 10080 }
)

foreach ($j in $jobs) {
    $name = $j.name; $type = $j.type
    $out = "$log\$name.out"; $err = "$log\$name.err"
    $argList = @("--type=$type", "--name=$name", '--platform=dev')
    if ($j.port) { $argList += "--port=$($j.port)" }
    if ($version) { $argList += "--version=$version" }
    $proc = Start-Process -FilePath $exe `
        -ArgumentList $argList `
        -WorkingDirectory $binDir `
        -RedirectStandardOutput $out -RedirectStandardError $err `
        -WindowStyle Hidden -PassThru
    Write-Output ("launched {0} type={1} pid={2}" -f $name, $type, $proc.Id)
    Start-Sleep -Seconds 2
}

# Sync cluster metadata with this topology so the WebPlatform cluster page is
# consistent: write cluster:desired:{type}.count = launched count, and drop
# zombie by_type members (empty hash or heartbeat older than 15s). Non-fatal
# if redis-cli is unavailable.
$redisCli = Join-Path $binDir '..\..\env_root\RedisCluster\redis-cli.exe'
if (-not (Test-Path $redisCli)) {
    $rc = Get-Command redis-cli -ErrorAction SilentlyContinue
    $redisCli = if ($rc) { $rc.Source } else { $null }
}
if ($redisCli) {
    function Invoke-Redis([string[]]$a) { & $redisCli -c -p 7000 @a 2>$null }
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $desired = @{}
    foreach ($j in $jobs) { $tk = [string]$j.type; $desired[$tk] = [int]$desired[$tk] + 1 }
    foreach ($t in @(100, 101, 102, 201, 202)) {
        $tk = [string]$t
        Invoke-Redis @('hset', "cluster:desired:$tk", 'count', "$([int]$desired[$tk])", 'mode', 'manual') | Out-Null
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
    Write-Output "cluster metadata synced (desired = topology, zombies cleaned)"
} else {
    Write-Output "redis-cli not found; skipped cluster metadata sync"
}

Write-Output ""
Write-Output "Cluster (3x Login + 3x Game) launched. Logs: $log"
