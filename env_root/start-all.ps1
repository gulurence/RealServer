# 一键启动 env_root 本地测试环境: Redis Cluster (7000-7005) + MySQL (3306) + WebPlatform PHP (8080)
$root = $PSScriptRoot

Write-Output "=== [1/3] Redis Cluster ==="
& "$root\RedisCluster\start-cluster.ps1"

Write-Output "=== [2/3] MySQL ==="
& "$root\Mysql\start-mysql.ps1"

Write-Output "=== [3/3] WebPlatform (PHP) ==="
& "$root\WebPlatform\start-web.ps1"

Write-Output ""
Write-Output "全部启动完成:"
Write-Output "  Redis Cluster: 127.0.0.1:7000-7005  (3 主 3 从)"
Write-Output "  MySQL:         127.0.0.1:3306  root/rootpass  db=realserver"
Write-Output "  WebPlatform:   http://127.0.0.1:8080/index.php  (admin/admin)"
