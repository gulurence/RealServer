# Stop all RealServer processes (and the NodeManager daemon). ASCII-only on purpose (see start_cluster.ps1).
$nm = Get-Process NodeManager -ErrorAction SilentlyContinue
$rs = Get-Process RealServer -ErrorAction SilentlyContinue
if (-not $nm -and -not $rs) {
    Write-Output "No running RealServer/NodeManager process"
    exit 0
}
# NodeManager first so it cannot relaunch RealServer children while we stop them.
if ($nm) {
    $nm | Stop-Process -Force
    Write-Output ("Stopped {0} NodeManager process(es)" -f $nm.Count)
}
if ($rs) {
    $rs | Stop-Process -Force
    Write-Output ("Stopped {0} RealServer process(es)" -f $rs.Count)
}
