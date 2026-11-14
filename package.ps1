param([string]$InnoCompiler)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
if (-not (Test-Path -LiteralPath 'dist\瞬贴.exe')) { & .\build.ps1 -Test }
if (-not (Test-Path -LiteralPath 'dist\bin\ffmpeg.exe') -or -not (Test-Path -LiteralPath 'dist\bin\gifski.exe')) { & .\tools\fetch_dependencies.ps1 }
Copy-Item -LiteralPath 'THIRD_PARTY_NOTICES.zh-CN.txt' -Destination 'dist\第三方说明.txt' -Force
New-Item -ItemType Directory -Force -Path 'dist\licenses','releases' | Out-Null
Copy-Item -Path 'licenses\*' -Destination 'dist\licenses' -Force
if (-not $InnoCompiler) {
    $candidate = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($candidate) { $InnoCompiler = $candidate.Source }
    elseif (Test-Path -LiteralPath 'tools\inno\ISCC.exe') { $InnoCompiler = Join-Path $PSScriptRoot 'tools\inno\ISCC.exe' }
    else { throw '请安装 Inno Setup，或通过 -InnoCompiler 指定 ISCC.exe。' }
}
& $InnoCompiler installer.iss
if ($LASTEXITCODE -ne 0) { throw '安装包生成失败。' }
$portableDir = Join-Path $PSScriptRoot 'tools\portable-package'
New-Item -ItemType Directory -Force -Path $portableDir | Out-Null
Copy-Item -LiteralPath 'dist\瞬贴.exe','README.zh-CN.md','THIRD_PARTY_NOTICES.zh-CN.txt' -Destination $portableDir -Force
Copy-Item -LiteralPath 'dist\bin','licenses' -Destination $portableDir -Recurse -Force
Compress-Archive -Path (Join-Path $portableDir '*') -DestinationPath 'releases\瞬贴-0.1.3-便携.zip' -Force
$hashes = Get-FileHash -LiteralPath 'releases\瞬贴-0.1.3-安装.exe','releases\瞬贴-0.1.3-便携.zip' -Algorithm SHA256
$hashes | ForEach-Object { $_.Hash.ToLowerInvariant() + '  ' + (Split-Path -Leaf $_.Path) } | Set-Content -LiteralPath 'releases\SHA256SUMS.txt' -Encoding utf8
