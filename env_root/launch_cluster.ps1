# 启动 RealServer 集群进程 (Gate/Login/Game/Global)
# 依赖: env_root 的 Redis Cluster + MySQL 已启动 (start-all.bat)
$root = 'E:\RealServer'
$exe  = "$root\build_msvc\engine\Debug\RealServer.exe"
$log  = "$root\logs"

if (-not (Test-Path $log)) { New-Item -ItemType Directory -Force $log | Out-Null }
if (-not (Test-Path $exe)) { Write-Output "RealServer.exe 不存在: $exe"; exit 1 }

# 顺序: 后端服务先起, Gate 最后起 (便于发现 peer)
$jobs = @(
    @{ type = 201; name = 'game-01'   },
    @{ type = 102; name = 'global-01' },
    @{ type = 101; name = 'login-01'  },
    @{ type = 100; name = 'gate-01'   }
)

foreach ($j in $jobs) {
    $name = $j.name; $type = $j.type
    $out = "$log\$name.out"; $err = "$log\$name.err"
    $proc = Start-Process -FilePath $exe `
        -ArgumentList "--type=$type", "--name=$name", '--platform=dev' `
        -WorkingDirectory $root `
        -RedirectStandardOutput $out -RedirectStandardError $err `
        -WindowStyle Hidden -PassThru
    Write-Output ("launched {0} type={1} pid={2}" -f $name, $type, $proc.Id)
    Start-Sleep -Seconds 2
}

Write-Output ""
Write-Output "集群已启动, 日志见 $log"
