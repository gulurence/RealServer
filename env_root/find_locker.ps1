# find_locker.ps1 — 找哪个进程加载了 libCocosDenshion.dll（持有文件锁）
$dll = "libCocosDenshion.dll"
$procs = @("devenv", "HelloCpp", "VsDebugConsole", "VsHub", "MSBuild")
foreach ($n in $procs) {
    $ps = Get-Process -Name $n -ErrorAction SilentlyContinue
    foreach ($p in $ps) {
        $mods = $null
        try { $mods = $p.Modules } catch { $mods = $null }
        $hit = $false
        if ($mods) {
            foreach ($m in $mods) {
                if ($m.ModuleName -eq $dll) { $hit = $true; break }
            }
        }
        Write-Output ("proc=" + $n + " pid=" + $p.Id + " loadedDll=" + $hit + " moduleCount=" + $(if ($mods) { $mods.Count } else { -1 }))
    }
}
