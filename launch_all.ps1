$root = 'D:/code/RealServer'
$exe  = "$root/build_msvc/engine/Debug/RealServer.exe"
$procs = @(
  @{ type = 100; name = 'gate-01'  },
  @{ type = 101; name = 'login-01' },
  @{ type = 201; name = 'game-01'  }
)
foreach ($p in $procs) {
  $out = "$root/logs/$($p.name).out"
  $err = "$root/logs/$($p.name).err"
  $proc = Start-Process -FilePath $exe `
    -ArgumentList "--type=$($p.type)", "--name=$($p.name)", '--platform=dev' `
    -WorkingDirectory $root `
    -RedirectStandardOutput $out -RedirectStandardError $err `
    -WindowStyle Hidden -PassThru
  Write-Output ("LAUNCHED " + $p.name + " pid=" + $proc.Id)
}
