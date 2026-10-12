# 一键停止 env_root 本地测试环境
$root = $PSScriptRoot

& "$root\RedisCluster\stop-cluster.ps1"
& "$root\Mysql\stop-mysql.ps1"
& "$root\WebPlatform\stop-web.ps1"

Write-Output "全部已停止"
