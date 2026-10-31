# Dev gray rolling restart.
# Replace running RealServer processes one batch at a time with a versioned exe copy.
# Windows locks a running single exe, so each process gets its own copy under releases/
# (RealServer_<version>.exe). ASCII-only (PowerShell 5.1 reads .ps1 as GBK).
#
# Two roll strategies, per process type:
#   - Battle (type 202): "drain then kill". Mark the process draining (no new battles),
#     wait for its load (battle actor count) to reach 0 (existing battles finish
#     naturally), then migrate_save + kill + relaunch. No mid-frame kill.
#   - Others (Game/Global/Login/Gate): "migrate then kill". migrate_save flushes
#     actor state to Redis and evicts cluster:actor leases; the new process restores
#     orphans on startup (ProcessEnv -> ActorMigration.run). After relaunch we wait
#     for the restore-complete key before rolling the next batch (queued restore).
param(
    [Parameter(Mandatory=$true)][string]$ExePath,
    [int]$BatchSize = 1,
    [int]$Type = 0,
    [int]$ExcludeType = 0,
    [string]$RedisCli = 'E:\RealServer\env_root\RedisCluster\redis-cli.exe',
    [string]$RedisHost = '127.0.0.1',
    [int]$RedisPort = 7000,
    [int]$MigrateSaveDelaySec = 2,
    [int]$DrainTimeoutSec = 360,
    [int]$RestoreTimeoutSec = 30,
    [int]$ShutdownTimeoutSec = 30,
    [switch]$NoMigrateSave
)
$ErrorActionPreference = 'Stop'

$binDir = 'E:\RealServer\bin\windows'
$logDir = Join-Path $binDir 'logs'
if ($BatchSize -lt 1) { $BatchSize = 1 }
if (-not (Test-Path $ExePath)) { Write-Output "missing exe: $ExePath"; exit 1 }
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Force $logDir | Out-Null }

# version to report on relaunch, parsed from exe filename RealServer_<version>.exe
$version = ''
if ($ExePath -match 'RealServer_(.+)\.exe$') { $version = $Matches[1] }

$hasRedisCli = $false
if (-not $NoMigrateSave) {
    if (-not (Test-Path $RedisCli)) {
        # fall back to a redis-cli.exe next to this script's sibling RedisCluster dir
        $alt = Join-Path (Split-Path (Split-Path $binDir -Parent) -Parent) 'env_root\RedisCluster\redis-cli.exe'
        if (Test-Path $alt) { $RedisCli = $alt }
    }
    $hasRedisCli = (Test-Path $RedisCli)
    if (-not $hasRedisCli) { Write-Output "WARN: redis-cli not found, skipping migrate_save/drain" }
}

# Run redis-cli against the cluster in cluster mode (-c follows MOVED redirects, so
# HGET/SMEMBERS/GET on any slot work; PUBLISH is cluster-wide regardless).
function Invoke-RedisCli([string[]]$Cmd) {
    return (& $RedisCli -h $RedisHost -p $RedisPort -c @Cmd)
}

# publish "migrate_save <os_pid>" to the process's type channel; only the process whose
# OS pid matches acts (GmCommandMgr filters by GetCurrentProcessId).
function Publish-MigrateSave([int]$procType, [int]$osPid) {
    if (-not $hasRedisCli) { return }
    Invoke-RedisCli @('PUBLISH', "cluster:gm:type:$procType", "migrate_save $osPid") | Out-Null
}

# publish "drain <os_pid>" to the process's type channel (Battle-only, same os_pid filter).
function Publish-Drain([int]$procType, [int]$osPid) {
    if (-not $hasRedisCli) { return }
    Invoke-RedisCli @('PUBLISH', "cluster:gm:type:$procType", "drain $osPid") | Out-Null
}

# publish "shutdown <os_pid>" to the process's type channel; the target process sets its
# graceful-shutdown flag and exits via OnStop (saveAll + deregister), no hard kill.
function Publish-Shutdown([int]$procType, [int]$osPid) {
    if (-not $hasRedisCli) { return }
    Invoke-RedisCli @('PUBLISH', "cluster:gm:type:$procType", "shutdown $osPid") | Out-Null
}

# poll until a process (by OS pid) has exited; returns $true on clean exit, $false on timeout.
function Wait-ProcessExit([int]$osPid, [int]$timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        if (-not (Get-Process -Id $osPid -ErrorAction SilentlyContinue)) { return $true }
        Start-Sleep -Seconds 1
    }
    return $false
}

# map os_pid -> cluster processId via cluster:process:<type>:<id> os_pid field
function Get-ProcessIdByOsPid([int]$procType, [int]$osPid) {
    if (-not $hasRedisCli) { return 0 }
    $members = Invoke-RedisCli @('SMEMBERS', "cluster:process:by_type:$procType")
    foreach ($m in $members) {
        $m = "$m".Trim()
        if ($m -eq '') { continue }
        $ownerOsPid = (Invoke-RedisCli @('HGET', "cluster:process:${procType}:$m", 'os_pid'))
        if ("$ownerOsPid".Trim() -eq "$osPid") { return [int64]$m }
    }
    return 0
}

# Client-facing TCP listen port for a process (0 = none). Read the live listen_port
# from Redis so a custom port survives the roll; fall back to the well-known dev ports
# (must match start_cluster.ps1: Gate=10080, Battle=10081).
function Get-ListenPort([int]$procType, [int]$osPid) {
    if ($hasRedisCli) {
        $processId = Get-ProcessIdByOsPid $procType $osPid
        if ($processId -ne 0) {
            $lp = (Invoke-RedisCli @('HGET', "cluster:process:${procType}:${processId}", 'listen_port')).Trim()
            if ($lp -ne '') { return [int]$lp }
        }
    }
    switch ($procType) {
        100 { return 10080 }   # Gate
        202 { return 10081 }   # Battle
        default { return 0 }   # no client listener
    }
}

# Count non-singleton (id != 0) actors owned by a process -- the true "active load".
# loadScore (cluster:process:...:load) also counts the permanent singleton/prototype
# (id=0), so it never reaches 0 for a live process (Battle bottoms out at 1). Count
# active actors directly via cluster:actor:{type}:{id} pid instead. -1 = unavailable.
function Get-ActiveActorCount([int64]$processId) {
    if ($processId -eq 0 -or -not $hasRedisCli) { return -1 }
    $count = 0
    # Battle process (202) hosts only Battle(203) actors (GetHostedActorTypes in C++).
    # Extend this list if drain is ever applied to another process type.
    foreach ($atype in @(203)) {
        $members = Invoke-RedisCli @('SMEMBERS', "cluster:actor:by_type:$atype")
        foreach ($m in $members) {
            $m = "$m".Trim()
            if ($m -eq '' -or $m -eq '0') { continue }   # skip singleton/prototype
            $owner = (Invoke-RedisCli @('HGET', "cluster:actor:${atype}:$m", 'pid')).Trim()
            if ($owner -eq "$processId") { $count++ }
        }
    }
    return $count
}

# wait until a battle process has no active (non-singleton) battles left, else timeout
function Wait-BattleDrain([int]$procType, [int]$osPid, [int]$timeoutSec) {
    if (-not $hasRedisCli) { return }
    $processId = Get-ProcessIdByOsPid $procType $osPid
    if ($processId -eq 0) {
        Write-Output "  WARN: cannot map os_pid=$osPid to processId, skip drain wait"
        return
    }
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $active = -1
    while ((Get-Date) -lt $deadline) {
        $active = Get-ActiveActorCount $processId
        if ($active -le 0) {
            Write-Output "  drain complete: process=$processId active=0"
            return
        }
        Write-Output "  waiting drain: process=$processId active=$active"
        Start-Sleep -Seconds 5
    }
    Write-Output "  WARN: drain timeout (${timeoutSec}s), force migrate with active=$active"
}

# wait for the new process to finish restoring its actors, signaled by
# cluster:migrate:restored:<new processId> (written by ActorMigration.run on startup).
function Wait-Restore([int]$procType, [string]$name, [int]$oldOsPid, [int]$timeoutSec) {
    if (-not $hasRedisCli) { return }
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $members = Invoke-RedisCli @('SMEMBERS', "cluster:process:by_type:$procType")
        foreach ($m in $members) {
            $m = "$m".Trim()
            if ($m -eq '') { continue }
            $key = "cluster:process:${procType}:$m"
            $nm = (Invoke-RedisCli @('HGET', $key, 'name'))
            $op = (Invoke-RedisCli @('HGET', $key, 'os_pid'))
            if ("$nm".Trim() -eq $name -and "$op".Trim() -ne "$oldOsPid") {
                $rv = (Invoke-RedisCli @('GET', "cluster:migrate:restored:$m"))
                if ("$rv".Trim() -ne '') {
                    Write-Output "  restore complete: name=$name process=$m restored=$($rv.Trim())"
                    return
                }
            }
        }
        Start-Sleep -Seconds 1
    }
    Write-Output "  WARN: restore wait timeout for name=$name"
}

# enumerate running RealServer processes (any RealServer*.exe) with --name in command line
$procs = Get-CimInstance Win32_Process | Where-Object {
    $_.Name -like 'RealServer*.exe' -and $_.CommandLine -match '--name='
}
if (-not $procs) { Write-Output 'no running RealServer processes found'; exit 1 }

$items = foreach ($p in $procs) {
    $name = 'unknown'; $procType = ''
    if ($p.CommandLine -match '--name=(\S+)') { $name = $Matches[1] }
    if ($p.CommandLine -match '--type=(\d+)')  { $procType = $Matches[1] }
    [pscustomobject]@{ Pid = [int]$p.ProcessId; Name = $name; Type = $procType }
}

# optional type filter (e.g. -Type 202 rolls only Battle processes)
if ($Type -gt 0) {
    $items = @($items | Where-Object { [int]$_.Type -eq $Type })
    if ($items.Count -eq 0) { Write-Output "no running processes of type $Type - nothing to roll"; exit 0 }
}

# optional type exclusion (e.g. -Type 0 -ExcludeType 100 rolls all but Gate: Gate update
# is a separate, client-disconnecting action invoked via its own panel button).
if ($ExcludeType -gt 0) {
    $items = @($items | Where-Object { [int]$_.Type -ne $ExcludeType })
}

# stable order: battle(202) -> game(201) -> global(102) -> login(101) -> gate(100)
# Battle drains first (stop new battles, finish in-flight), Gate last (client entry).
$ordered = @($items | Sort-Object { [int]$_.Type } -Descending)

$batches = @()
for ($i = 0; $i -lt $ordered.Count; $i += $BatchSize) {
    $end = [Math]::Min($i + $BatchSize - 1, $ordered.Count - 1)
    $batches += ,@($ordered[$i..$end])
}

foreach ($batch in $batches) {
    $names = ($batch | ForEach-Object { $_.Name }) -join ', '
    Write-Output "rolling batch: $names"

    # 1) battle: drain first; then flush actors to Redis before kill (migrate then kill)
    foreach ($it in $batch) {
        if ([int]$it.Type -eq 202 -and $hasRedisCli) {
            Publish-Drain ([int]$it.Type) $it.Pid
            # let peers' RefreshLoop (10s) observe the drain flag and stop routing new battles
            Start-Sleep -Seconds 12
            Wait-BattleDrain ([int]$it.Type) $it.Pid $DrainTimeoutSec
        }
        Publish-MigrateSave ([int]$it.Type) $it.Pid
    }
    if ($hasRedisCli -and $MigrateSaveDelaySec -gt 0) {
        Start-Sleep -Seconds $MigrateSaveDelaySec
    }

    # 2) graceful shutdown: publish "shutdown <os_pid>" and wait for clean exit,
    #    fall back to force-kill on timeout (or when redis-cli is unavailable).
    foreach ($it in $batch) {
        Publish-Shutdown ([int]$it.Type) $it.Pid
    }
    $forceKill = @()
    foreach ($it in $batch) {
        if ($hasRedisCli -and (Wait-ProcessExit $it.Pid $ShutdownTimeoutSec)) { continue }
        $forceKill += $it
    }
    foreach ($it in $forceKill) {
        Write-Output "  WARN: graceful shutdown unavailable/timed out, force-killing name=$($it.Name) pid=$($it.Pid)"
        Stop-Process -Id $it.Pid -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 2

    # 3) relaunch with the versioned exe
    foreach ($it in $batch) {
        $out = Join-Path $logDir ($it.Name + '.out')
        $err = Join-Path $logDir ($it.Name + '.err')
        $argList = @("--type=$($it.Type)", "--name=$($it.Name)", '--platform=dev')
        $lp = Get-ListenPort ([int]$it.Type) $it.Pid
        if ($lp -gt 0) { $argList += "--port=$lp" }
        if ($version) { $argList += "--version=$version" }
        Start-Process -FilePath $ExePath `
            -ArgumentList $argList `
            -WorkingDirectory $binDir `
            -RedirectStandardOutput $out -RedirectStandardError $err `
            -WindowStyle Hidden | Out-Null
    }
    Start-Sleep -Seconds 3

    # 4) wait for queued restore before rolling the next batch
    foreach ($it in $batch) {
        Wait-Restore ([int]$it.Type) $it.Name $it.Pid $RestoreTimeoutSec
    }
}
Write-Output "roll complete: $($ordered.Count) process(es) now on $ExePath"
