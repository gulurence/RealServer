# Sample per-process CPU over a window (ASCII only).
# Usage: powershell -NoProfile -File sample_cpu.ps1 <seconds>
param([int]$Seconds = 8)

function Snapshot {
    Get-CimInstance Win32_Process |
        Where-Object { $_.Name -eq 'RealServer.exe' } |
        Select-Object ProcessId,
            @{n='Type'; e={ if ($_.CommandLine -match '--type=(\d+)') { [int]$Matches[1] } else { -1 } }},
            @{n='K'; e={ [double]$_.KernelModeTime }},
            @{n='U'; e={ [double]$_.UserModeTime }}
}

$s1 = Snapshot
Start-Sleep -Seconds $Seconds
$s2 = Snapshot

$rows = foreach ($p2 in $s2) {
    $p1 = $s1 | Where-Object ProcessId -eq $p2.ProcessId
    if ($p1) {
        $dt = (($p2.K + $p2.U) - ($p1.K + $p1.U)) / 10000000.0
        [PSCustomObject]@{
            Type     = $p2.Type
            PID      = $p2.ProcessId
            CPU_pct  = [math]::Round($dt / $Seconds * 100.0, 1)
        }
    }
}

$rows | Sort-Object Type, PID | Format-Table -AutoSize

# Aggregate by type
$byType = $rows | Group-Object Type | ForEach-Object {
    [PSCustomObject]@{
        Type = $_.Name
        Count = $_.Count
        TotalCPU_pct = [math]::Round(($_.Group | Measure-Object CPU_pct -Sum).Sum, 1)
    }
}
Write-Output '--- by type ---'
$byType | Sort-Object Type | Format-Table -AutoSize
