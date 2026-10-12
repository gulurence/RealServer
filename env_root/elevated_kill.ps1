# elevated_kill.ps1 — 以管理员权限终止 HelloCpp.exe（会弹 UAC）
Start-Process -FilePath "cmd.exe" -ArgumentList "/c","taskkill /F /IM HelloCpp.exe" -Verb RunAs
Write-Output "elevated taskkill launched (approve the UAC prompt)"
