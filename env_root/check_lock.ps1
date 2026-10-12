# check_lock.ps1 — 非破坏性检测文件是否被独占锁（只 open/close，不写入）
param([string]$Path = "D:\code\RealServer\client\soulofgun\Debug.win32\libCocosDenshion.dll")

# 尝试以独占方式打开（FileShare.None 会与任何已打开的句柄冲突）
try {
    $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    $fs.Close()
    Write-Output "NOT LOCKED (exclusive open succeeded)"
} catch {
    Write-Output ("LOCKED: " + $_.Exception.Message)
}

# 再试：只读 + 共享读（模拟 LoadLibrary 后的状态）
try {
    $fs2 = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
    $fs2.Close()
    Write-Output "WRITE with FileShare.Read: OK"
} catch {
    Write-Output ("WRITE with FileShare.Read FAILED: " + $_.Exception.Message)
}
