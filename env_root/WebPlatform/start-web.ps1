# 启动 PHP 内置 Web 服务器, 服务 WebPlatform 后台 (8080)
$root = $PSScriptRoot
$php  = "E:\RealServer\env_root\php\php.exe"
$www  = "$root\www"

if (-not (Test-Path $php)) { Write-Error "php not found: $php"; exit 1 }

$up = Test-NetConnection -ComputerName 127.0.0.1 -Port 8080 -InformationLevel Quiet -WarningAction SilentlyContinue
if ($up) { Write-Output "PHP web already running (8080)"; exit 0 }

Start-Process -FilePath $php `
    -ArgumentList "-S","0.0.0.0:8080","-t","`"$www`"" `
    -WindowStyle Hidden `
    -RedirectStandardOutput "$root\php-server.log" `
    -RedirectStandardError  "$root\php-server.err.log"

for ($i = 0; $i -lt 15; $i++) {
    Start-Sleep -Seconds 1
    $up = Test-NetConnection -ComputerName 127.0.0.1 -Port 8080 -InformationLevel Quiet -WarningAction SilentlyContinue
    if ($up) { Write-Output "WebPlatform ready (http://127.0.0.1:8080/index.php)"; exit 0 }
}
Write-Error "WebPlatform did not become ready; check $root\php-server.err.log"
