# WebPlatform 后台网站（PHP）

RealServer 测试环境的运维后台：**登录 + 进程容器节点状态展示**。PHP 8.2 + Apache，
`pdo_mysql` 做登录、`redis` 扩展读节点注册数据。

## 功能

| 页面 | 路径 | 说明 |
|---|---|---|
| 登录 | `index.php` | 账号密码登录，会话校验 |
| 登录处理 | `login.php` | 先查 MySQL `users` 表，失败走 `config.php` 兜底 admin |
| 退出 | `logout.php` | 销毁会话 |
| 节点状态 | `nodes.php` | 读 Redis `cluster:process:by_type:*` + `cluster:process:*:*`，展示 IP/端口/状态/心跳/验证Key |

## 启动

```bash
cd EnvRoot/WebPlatform
docker compose up -d --build
```

首次会 `--build` 构建镜像（装 pdo_mysql + redis 扩展）。访问 http://localhost:8080/index.php 。

## 登录账号

- 默认测试账号：**admin / admin**（MySQL `users` 表 + `config.php` 兜底，两处一致）。
- 生产部署务必改掉占位密码。

## 节点数据来源

`nodes.php` 读 Redis 里 RealServer 注册的节点：

```
cluster:process:by_type:100  → Set { 1, 2, ... }            # 某类型所有进程 ID
cluster:process:100:1        → Hash { type,id,name,host,ip,port,load,state,heartbeat,authkey }
```

所以要让页面显示节点，需要：

1. 起 `EnvRoot/RedisCluster` 的 `redis-standalone`（6379）——本工程 `web` 容器默认连的
   是**自带 redis**（容器内 `redis:6379`，映射到宿主机 `6380`），要读 RealServer 写的数据，
   需让 RealServer 与本工程的 redis 指向同一个实例。两种接法：
   - **改 `www/config.php`** 的 `REDIS_HOST/REDIS_PORT`，指向 RedisCluster 的 standalone；
   - 或把本工程 redis 换成 external 网络接到 RedisCluster。
2. 启动 RealServer（`config/server.xml` 的 `cluster_discovery` 指向同一 Redis），它会注册节点。

> 简化验证：本工程已自带 redis（宿主机 6380），可手动写入测试数据看页面效果：
> ```bash
> docker compose exec redis redis-cli SADD cluster:process:by_type:100 1
> docker compose exec redis redis-cli HSET cluster:process:100:1 id 1 name gate-01 ip 172.26.192.1 port 50051 state 2 heartbeat "$(date +%s)" authkey 2b88502f7aaf37c0
> ```
> 然后刷新 `nodes.php` 即可看到 Gate 节点。

## 与其它工程的关系

| 依赖 | 说明 |
|---|---|
| `../Mysql` | 同款 `users` 表结构；本工程自带 mysql 以自包含，可改为复用外部实例 |
| `../RedisCluster` | 节点状态数据源；本工程自带 redis，生产指向同一 Redis 即可 |

## 停止

```bash
docker compose down
```
