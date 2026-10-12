$exe = 'D:/code/RealServer/build_msvc/engine/Debug/RealServer.exe'
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exe
$psi.Arguments = '--type=100 --name=gate-01 --platform=dev'
$psi.WorkingDirectory = 'D:/code/RealServer'
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$p = New-Object System.Diagnostics.Process
$p.StartInfo = $psi
try { $null = $p.Start() } catch { Write-Output ("START_EXCEPTION=" + $_.Exception.Message); exit }
if (-not $p.WaitForExit(8000)) {
  Write-Output ("STILL_RUNNING pid=" + $p.Id)
  $p.Kill()
} else {
  Write-Output ("EXITCODE=0x{0:X} ({0})" -f $p.ExitCode)
  $so = $p.StandardOutput.ReadToEnd()
  $se = $p.StandardError.ReadToEnd()
  Write-Output ("STDOUT_LEN=" + $so.Length)
  Write-Output ("STDERR_LEN=" + $se.Length)
  Write-Output ("--- STDOUT ---")
  Write-Output $so
  Write-Output ("--- STDERR ---")
  Write-Output $se
}
