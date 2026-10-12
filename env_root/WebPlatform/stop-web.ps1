# 停止 PHP 内置 Web 服务器
Get-Process -Name php -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Write-Output "php web stopped"
