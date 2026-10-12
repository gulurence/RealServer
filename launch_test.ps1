# launch_test.ps1 — 启动全量测试集群: Gate / Login / Global / Game
$root = 'D:/code/RealServer'
$exe  = "$root/build_msvc/engine/Debug/RealServer.exe"

# 先清理旧进程 (避免端口/进程冲突)
Get-Process RealServer -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

$procs = @(
  @{ type = 201; name = 'game-01'   },
  @{ type = 101; name = 'login-01'  },
  @{ type = 102; name = 'global-01' },
  @{ type = 100; name = 'gate-01'   }
)

foreach ($p in $procs) {
  $out = "$root/logs/$($p.name).out"
  $err = "$root/logs/$($p.name).err"
  $proc = Start-Process -FilePath $exe `
    -ArgumentList "--type=$($p.type)", "--name=$($p.name)", '--platform=dev' `
    -WorkingDirectory $root `
    -RedirectStandardOutput $out -RedirectStandardError $err `
    -WindowStyle Hidden -PassThru
  Write-Output ("LAUNCHED " + $p.name + " type=" + $p.type + " pid=" + $proc.Id)
  Start-Sleep -Seconds 2
}
