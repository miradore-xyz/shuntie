$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $PSScriptRoot 'dependency-cache'
$bin = Join-Path $projectRoot 'dist\bin'
New-Item -ItemType Directory -Force -Path $cache,$bin | Out-Null
# 固定兼容版本，避免最新版 NVENC 接口要求高于现有显卡驱动。
$ffArchive = Join-Path $cache 'ffmpeg-7.1.1.zip'
if (-not (Test-Path -LiteralPath $ffArchive)) {
    Invoke-WebRequest 'https://github.com/GyanD/codexffmpeg/releases/download/7.1.1/ffmpeg-7.1.1-essentials_build.zip' -OutFile $ffArchive
}
Expand-Archive -LiteralPath $ffArchive -DestinationPath (Join-Path $cache 'ffmpeg') -Force
Copy-Item -LiteralPath (Join-Path $cache 'ffmpeg\ffmpeg-7.1.1-essentials_build\bin\ffmpeg.exe') -Destination (Join-Path $bin 'ffmpeg.exe') -Force
# 下载本项目按 tools/build_gifski.ps1 构建的独立编码器组件。
$gifArchive = Join-Path $cache 'Shuntie-Gifski-1.34.0-Windows-x64.zip'
if (-not (Test-Path -LiteralPath $gifArchive)) {
    Invoke-WebRequest 'https://github.com/miradore-xyz/shuntie/releases/download/v0.1.4/Shuntie-Gifski-1.34.0-Windows-x64.zip' -OutFile $gifArchive
}
Expand-Archive -LiteralPath $gifArchive -DestinationPath $bin -Force
Write-Output 'FFmpeg 7.1.1 和 Gifski 1.34.0 已准备完成。'
