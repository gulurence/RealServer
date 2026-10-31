RealServer 服务器集群 - Windows / x64 部署目录 (自包含)
========================================================

本目录包含启动 RealServer 集群所需的全部文件, 可独立运行 (不依赖仓库根目录)。

目录结构
--------
  RealServer.exe         服务器主程序 (单可执行文件, 进程身份由 --type 决定)
  libmysql.dll           MySQL 客户端库 (mysql-connector-c)
  libxml2d.dll           libxml2 (XML 配置解析)
  hiredisd.dll           hiredis (Redis 客户端)
  MSVCP140D.dll          MSVC 调试运行时 (Debug 构建)
  VCRUNTIME140D.dll      MSVC 调试运行时
  VCRUNTIME140_1D.dll    MSVC 调试运行时
  ucrtbased.dll          Windows 调试 UCRT
  config\server.xml      集群配置 (进程/集群/Redis/DB 连接池)
  config\server.xml.example  配置模板参考
  script\                Lua 业务脚本 (common/ actors/ dev/ test/ prod/)
  shared\config\data\    战斗关卡数据 (wave/map_collision/sp01.bin, 帧同步两端必须一致)
  start_cluster.bat      启动集群 (双击)
  start_cluster.ps1      启动集群 (PowerShell)
  stop_cluster.bat       停止集群 (双击)
  stop_cluster.ps1       停止集群 (PowerShell)
  logs\                  运行日志 (自动生成: {name}.out/.err + normal.log)
  local.info             本机节点注册表 (运行时自动生成)

启动方式
--------
  双击 start_cluster.bat, 或执行:
    powershell -ExecutionPolicy Bypass -File start_cluster.ps1

  集群进程 (按启动顺序):
    201 = Game   (game-01)
    102 = Global (global-01)
    101 = Login  (login-01)
    100 = Gate   (gate-01)

  启动时工作目录设为本目录 (bin\windows), 代码按相对路径加载
  config/server.xml 与 script/。

前置依赖
--------
  1. Redis Cluster 已启动: env_root\RedisCluster (节点 7000-7005)
  2. MySQL 已启动:        127.0.0.1:3306 (db=realserver)

配置同步说明
------------
  config\server.xml 与 script\ 是仓库根 E:\RealServer\config、E:\RealServer\script
  的副本。修改后需重新拷贝到本目录 (或直接在本目录修改), 二者需保持一致。
  server.xml 含 DB/Redis 明文连接信息, 部署到生产时务必替换为安全凭证。

说明
----
  - 当前为 Debug 构建, 因此附带了 MSVC 调试运行时 DLL (MSVCP140D.dll 等)。
    这些 DLL 仅用于本机开发/测试; 若需分发到无 VS 构建工具的机器, 请改用
    Release 构建并随附对应的 Release 运行时 (MSVCP140.dll 等)。
  - 进程 RPC 端口由 xProcessDefine.h 按进程类型自动分配, 并通过 Redis 服务发现
    相互发现, 无需手工配置端口。
