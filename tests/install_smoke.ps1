$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$installer = Join-Path $projectRoot 'releases\瞬贴-0.1.3-安装.exe'
$installTarget = Join-Path $projectRoot 'tools\安装验证'
$registryPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
function Get-Startup {
    [Microsoft.Win32.Registry]::GetValue('HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run','Shuntie',$null)
}
$oldStartup = Get-Startup
$results = [ordered]@{}
function Invoke-Installer([string]$taskArgument) {
    $arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="' + $installTarget + '"'))
    if ($taskArgument) { $arguments += $taskArgument }
    $process = Start-Process -FilePath $installer -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw ('安装退出码：' + $process.ExitCode) }
}
try {
    # 第一轮不传任务覆盖参数，检查默认勾选的开机启动。
    Invoke-Installer ''
    $installedExe = Join-Path $installTarget '瞬贴.exe'
    $results.files_match = (Get-FileHash -LiteralPath $installedExe).Hash -eq (Get-FileHash -LiteralPath (Join-Path $projectRoot 'dist\瞬贴.exe')).Hash
    $results.encoders_present = (Test-Path -LiteralPath (Join-Path $installTarget 'bin\ffmpeg.exe')) -and (Test-Path -LiteralPath (Join-Path $installTarget 'bin\gifski.exe'))
    $startup = Get-Startup
    $results.default_autostart_enabled = $startup -eq ('"' + $installedExe + '" --tray')
    # 再次安装时取消勾选，确认也能关闭此前开启的开机启动。
    Invoke-Installer '/TASKS=""'
    $startup = Get-Startup
    $results.autostart_can_be_disabled = $null -eq $startup
    # 卸载不应删除用户输出的文件。
    $output = Join-Path $installTarget '输出'
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    Set-Content -LiteralPath (Join-Path $output '保留验证.txt') -Value '验证卸载保留用户输出。' -Encoding utf8
    $uninstall = Start-Process -FilePath (Join-Path $installTarget 'unins000.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -WindowStyle Hidden -Wait -PassThru
    $results.uninstall_ok = $uninstall.ExitCode -eq 0
    $results.user_output_preserved = Test-Path -LiteralPath (Join-Path $output '保留验证.txt')
} finally {
    # 验证结束后恢复本机原有的开机启动选择。
    if ($null -ne $oldStartup) { Set-ItemProperty -LiteralPath $registryPath -Name Shuntie -Value $oldStartup }
    else { Remove-ItemProperty -LiteralPath $registryPath -Name Shuntie -ErrorAction SilentlyContinue }
    $results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $projectRoot 'artifacts\installation-result.json') -Encoding utf8
}
$results | ConvertTo-Json
if ($results.Values -contains $false) { throw '安装验证未通过。' }
