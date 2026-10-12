$exe = "D:\code\RealServer\client\soulofgun\Debug.win32\HelloCpp.exe"
$wd  = "D:\code\RealServer\client\soulofgun\Debug.win32"
$clients = @(
  @("test_token_1"),
  @("test_token_2")
)
foreach ($c in $clients) {
  $token = $c[0]
  $env:SOG_TOKEN = $token
  Start-Process -FilePath $exe -WorkingDirectory $wd -WindowStyle Normal
  Write-Output "launched client token=$token"
  Start-Sleep -Seconds 2
}
