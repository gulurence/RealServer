# elev_test.ps1 — 验证能否通过 UAC 提权，并尝试强杀 HelloCpp
$marker = "D:\code\RealServer\EnvRoot\elev_marker.txt"
$inner = 'whoami > ' + $marker + ' 2>&1; taskkill /F /IM HelloCpp.exe >> ' + $marker + ' 2>&1; Get-Process -Name HelloCpp -ErrorAction SilentlyContinue | ForEach-Object { "STILL " + $_.Id } >> ' + $marker
$innerFile = "D:\code\RealServer\EnvRoot\elev_inner.ps1"
Set-Content -Path $innerFile -Value $inner -Encoding ASCII
if (Test-Path $marker) { Remove-Item $marker -Force }
Start-Process -FilePath "powershell.exe" -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-File",$innerFile -Verb RunAs -Wait
Start-Sleep -Seconds 2
if (Test-Path $marker) { Write-Output "=== marker content ==="; Get-Content $marker } else { Write-Output "NO MARKER (elevation did not run)" }
