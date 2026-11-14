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
# 固定共享解码库：只复制本转换器使用的五个库，不替换已验证的录屏程序。
$sharedArchive=Join-Path $cache 'ffmpeg-7.1.1-shared.zip'
if(-not (Test-Path -LiteralPath $sharedArchive)) { Invoke-WebRequest 'https://github.com/GyanD/codexffmpeg/releases/download/7.1.1/ffmpeg-7.1.1-full_build-shared.zip' -OutFile $sharedArchive }
if((Get-FileHash -LiteralPath $sharedArchive).Hash -ne '9F28727E8B472A04C1D2E520AAA425DCA82721B995139B35710091130EA6E699'){throw 'FFmpeg 解码库校验不一致。'}
$sharedRoot=Join-Path $cache 'ffmpeg-shared/ffmpeg-7.1.1-full_build-shared'
if(-not (Test-Path -LiteralPath "$sharedRoot/include/libavcodec/avcodec.h")){Expand-Archive -LiteralPath $sharedArchive -DestinationPath (Join-Path $cache 'ffmpeg-shared') -Force}
foreach($file in 'avcodec-61.dll','avformat-61.dll','avutil-59.dll','swscale-8.dll','swresample-5.dll'){Copy-Item -LiteralPath (Join-Path $sharedRoot "bin/$file") -Destination $bin -Force}
# 下载含原尺寸增量调色接口的 GNU 构建，源码与构建脚本均公开。
$gifArchive = Join-Path $cache 'Shuntie-Gifski-1.34.0-Native-v2-Windows-x64.zip'
if (-not (Test-Path -LiteralPath $gifArchive)) {
    Invoke-WebRequest 'https://github.com/miradore-xyz/shuntie/releases/download/v0.1.5/Shuntie-Gifski-1.34.0-Native-v2-Windows-x64.zip' -OutFile $gifArchive
}
if((Get-FileHash -LiteralPath $gifArchive).Hash -ne '0AB18F88A25E6299C314030827034BD08F579C211B0A4A21C552D6B804063340'){throw '原尺寸调色组件校验不一致。'}
Expand-Archive -LiteralPath $gifArchive -DestinationPath $bin -Force
Write-Output 'FFmpeg 7.1.1 和 Gifski 1.34.0 已准备完成。'
