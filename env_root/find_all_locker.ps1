# find_all_locker.ps1 — 遍历所有进程，找加载了 libCocosDenshion.dll 的那个
$dll = "libCocosDenshion.dll"
$all = Get-Process -ErrorAction SilentlyContinue
$found = @()
foreach ($p in $all) {
    try {
        $mods = $p.Modules
        foreach ($m in $mods) {
            if ($m.ModuleName -ieq $dll) {
                $found += ($p.ProcessName + " pid=" + $p.Id)
                break
            }
        }
    } catch {
        # 无权限枚举（elevated/终止进程），跳过
    }
}
if ($found.Count -eq 0) {
    Write-Output "no process has the DLL loaded (via accessible module enumeration)"
} else {
    Write-Output "DLL loaded by:"
    $found | ForEach-Object { Write-Output ("  " + $_) }
}
Write-Output ("total processes scanned: " + $all.Count)
