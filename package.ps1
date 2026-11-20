param([string]$InnoCompiler)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
if (-not (Test-Path -LiteralPath 'dist\瞬贴.exe')) { & .\build.ps1 -Test }
if (-not (Test-Path -LiteralPath 'dist\bin\ffmpeg.exe') -or -not (Test-Path -LiteralPath 'dist\bin\gifski.dll') -or -not (Test-Path -LiteralPath 'dist\bin\swscale-8.dll')) { & .\tools\fetch_dependencies.ps1 }
Copy-Item -LiteralPath 'THIRD_PARTY_NOTICES.zh-CN.txt' -Destination 'dist\第三方说明.txt' -Force
New-Item -ItemType Directory -Force -Path 'dist\licenses','releases' | Out-Null
Copy-Item -Path 'licenses\*' -Destination 'dist\licenses' -Force
if (-not $InnoCompiler) {
    $candidate = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($candidate) { $InnoCompiler = $candidate.Source }
    elseif (Test-Path -LiteralPath 'tools\inno\ISCC.exe') { $InnoCompiler = Join-Path $PSScriptRoot 'tools\inno\ISCC.exe' }
    else { throw '请安装 Inno Setup，或通过 -InnoCompiler 指定 ISCC.exe。' }
}
$cnIsl = Join-Path (Split-Path -Parent $InnoCompiler) 'Languages\ChineseSimplified.isl'
if (-not (Test-Path -LiteralPath $cnIsl)) {
    Invoke-WebRequest 'https://raw.githubusercontent.com/jrsoftware/issrc/refs/heads/main/Files/Languages/ChineseSimplified.isl' -OutFile $cnIsl
}
& $InnoCompiler installer.iss
if ($LASTEXITCODE -ne 0) { throw '安装包生成失败。' }
$version = '0.1.8'
$portableDir = Join-Path $PSScriptRoot 'tools\portable-package'
if (Test-Path -LiteralPath $portableDir) { Remove-Item -LiteralPath $portableDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $portableDir | Out-Null
Copy-Item -LiteralPath 'dist\瞬贴.exe','README.zh-CN.md','THIRD_PARTY_NOTICES.zh-CN.txt' -Destination $portableDir -Force
$portableBin=Join-Path $portableDir 'bin';New-Item -ItemType Directory -Force -Path $portableBin | Out-Null
Copy-Item -LiteralPath 'dist\bin\ffmpeg.exe','dist\bin\gifski.exe','dist\bin\gifski.dll','dist\bin\gifski-export.exe' -Destination $portableBin -Force
foreach($file in 'avcodec-61.dll','avformat-61.dll','avutil-59.dll','swscale-8.dll','swresample-5.dll'){Copy-Item -LiteralPath (Join-Path 'dist/bin' $file) -Destination $portableBin -Force}
if(Test-Path -LiteralPath (Join-Path $portableBin 'gifski-export.pdb')) { Remove-Item -LiteralPath (Join-Path $portableBin 'gifski-export.pdb') }
Copy-Item -LiteralPath 'licenses' -Destination $portableDir -Recurse -Force
Compress-Archive -Path (Join-Path $portableDir '*') -DestinationPath "releases\瞬贴-$version-便携.zip" -Force
$hashes = Get-FileHash -LiteralPath "releases\瞬贴-$version-安装.exe","releases\瞬贴-$version-便携.zip" -Algorithm SHA256
$hashes | ForEach-Object { $_.Hash.ToLowerInvariant() + '  ' + (Split-Path -Leaf $_.Path) } | Set-Content -LiteralPath 'releases\SHA256SUMS.txt' -Encoding utf8
