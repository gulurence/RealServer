# PowerShell 脚本 - 批量将文件转换为 UTF-8 无 BOM
$files = Get-ChildItem -Recurse -Include *.cpp, *.h

foreach ($file in $files) {
    $content = Get-Content $file -Raw
    Set-Content -Path $file.FullName -Value $content -Encoding GBK

    $reader = New-Object System.IO.StreamReader $file.FullName
    $reader.Peek() | Out-Null
    $hasBom = $reader.CurrentEncoding.GetPreamble().Length -ne 0
    $reader.Close()
    
    # 输出结果
    if ($hasBom) {
        Write-Host "文件包含BOM"
    } else {
        Write-Host "文件不包含BOM"
    }
}


# 以管理员打开 PowerShell
# 如果报错使用下面的命令设置签名
# set-ExecutionPolicy RemoteSigned