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
$gifArchive = Join-Path $cache 'gifski-1.34.0.tar.xz'
if (-not (Test-Path -LiteralPath $gifArchive)) {
    Invoke-WebRequest 'https://github.com/ImageOptim/gifski/releases/download/1.34.0/gifski-1.34.0.tar.xz' -OutFile $gifArchive
}
$gifDir = Join-Path $cache 'gifski'
New-Item -ItemType Directory -Force -Path $gifDir | Out-Null
& tar -xf $gifArchive -C $gifDir
if ($LASTEXITCODE -ne 0) { throw 'Gifski 解包失败。' }
Copy-Item -LiteralPath (Join-Path $gifDir 'win\gifski.exe') -Destination (Join-Path $bin 'gifski.exe') -Force
Write-Output 'FFmpeg 7.1.1 和 Gifski 1.34.0 已准备完成。'
