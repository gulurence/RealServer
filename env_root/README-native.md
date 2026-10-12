# env_root 本地测试环境（原生 Windows · 免 Docker）

RealServer 测试所需的三个服务，全部以**原生 Windows 进程**运行在本目录下，无需 Docker / WSL2 / 重启。

## 组成

| 服务 | 端口 | 说明 |
|---|---|---|
| Redis Cluster | 7000–7005 | 6 节点，3 主 3 从（`cluster_state:ok`），127.0.0.1 可达 |
| MySQL 8.0.29 | 3306 | root / rootpass，库 `realserver`（users / account / player） |
| WebPlatform (PHP 8.2.34) | 8080 | PHP 内置 web server，pdo_mysql + phpredis，admin / admin |

## 启动 / 停止

直接**双击**对应 `.bat` 即可（内部以 `powershell -ExecutionPolicy Bypass` 调用 `.ps1`，无需改执行策略）：

| 服务 | 一键启动 | 一键停止 |
|---|---|---|
| 全部 | `start-all.bat` | `stop-all.bat` |
| Redis | `RedisCluster\start-cluster.bat` | `RedisCluster\stop-cluster.bat` |
| MySQL | `Mysql\start-mysql.bat` | `Mysql\stop-mysql.bat` |
| Web | `WebPlatform\start-web.bat` | `WebPlatform\stop-web.bat` |

> 命令行下也可直接 `start-all.bat`；或仍可 `powershell -ExecutionPolicy Bypass -File .\start-all.ps1`。

## 验证

```powershell
# Redis 集群状态
.\RedisCluster\redis-cli.exe -p 7000 cluster info        # cluster_state:ok

# MySQL
.\Mysql\mysql-8.0.29-winx64\bin\mysql.exe -uroot -prootpass realserver -e "SHOW TABLES;"

# Web 后台（登录 admin / admin）
start http://127.0.0.1:8080/index.php
```

## 目录说明

- `php\` — PHP 8.2.34 NTS x64（`ext\` 已装 `php_pdo_mysql.dll` + `php_redis.dll`）
- `Mysql\mysql-8.0.29-winx64\` — MySQL 服务端；`Mysql\data\` 数据目录；`Mysql\my.ini` 配置
- `RedisCluster\` — 原生 redis-server 6 节点集群（`cluster\7000-7005\redis.conf`）+ 启停脚本
- `WebPlatform\www\` — PHP 后台源码（`config.php` 已指向 `127.0.0.1`）
- `.downloads\` — 安装包缓存（可整目录删除以释放约 250MB）

## 与 RealServer 对接

`config/server.xml` 已指向本环境：Redis 集群 seed `127.0.0.1:7000`，MySQL `127.0.0.1:3306` root/rootpass。
起好本环境后直接运行 RealServer 即可注册节点、读写 DB。

## 关键差异说明

- MySQL 选用 **8.0.29**（华为云镜像源，国内下载快；且 `mysql_native_password` 仍默认可用，
  兼容 RealServer 的旧 `mysql-connector-c 6.1.11`）。8.0.34+ 会默认禁用该插件，需额外开关。
- Redis 集群节点的 `cluster-announce-ip` 已改为 `127.0.0.1`（原为过期的 `172.26.192.1`）。
- 原 `Mysql/README.md`、`RedisCluster/README.md`、`WebPlatform/README.md` 为 **Docker 版**说明；
  本机未装 Docker，以上为原生替代方案，两者端口/账号一致。
