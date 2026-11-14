param([string]$LlvmMingwRoot)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$sourceRoot=Join-Path $PSScriptRoot 'gifski-source\gifski-1.34.0'
$archive=Join-Path $PSScriptRoot 'gifski-source.zip'
if (-not (Test-Path -LiteralPath $archive)) { Invoke-WebRequest 'https://github.com/ImageOptim/gifski/archive/refs/tags/1.34.0.zip' -OutFile $archive }
if ((Get-FileHash -LiteralPath $archive).Hash -ne 'F79E9EEA132439F0BCC2B4311302A751E3882586C1A96CC61E2742966DE866BA') { throw 'Gifski 源码校验不一致。' }
if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot 'Cargo.toml'))) { Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $PSScriptRoot 'gifski-source') -Force }
if (-not $LlvmMingwRoot) {
    $LlvmMingwRoot=Join-Path $PSScriptRoot 'llvm-mingw\llvm-mingw-20261104-msvcrt-x86_64'
    if (-not (Test-Path -LiteralPath $LlvmMingwRoot)) {
        $compilerArchive=Join-Path $PSScriptRoot 'llvm-mingw.zip'
        Invoke-WebRequest 'https://github.com/mstorsjo/llvm-mingw/releases/download/20261104/llvm-mingw-20261104-msvcrt-x86_64.zip' -OutFile $compilerArchive
        Expand-Archive -LiteralPath $compilerArchive -DestinationPath (Join-Path $PSScriptRoot 'llvm-mingw') -Force
    }
}
& rustup target add x86_64-pc-windows-gnu
if ($LASTEXITCODE -ne 0) { throw 'Rust GNU 目标不可用。' }
$env:SHUNTIE_GNU_LINKER=Join-Path $LlvmMingwRoot 'bin\x86_64-w64-mingw32-clang.exe'
$env:CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=Join-Path $PSScriptRoot 'gnu-link.cmd'
$env:CC_x86_64_pc_windows_gnu=$env:SHUNTIE_GNU_LINKER
$env:AR_x86_64_pc_windows_gnu=Join-Path $LlvmMingwRoot 'bin\llvm-ar.exe'
# 编入已公开的增量帧调色接口，复用同一版本的 imagequant。
Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\native-quantizer.rs') -Destination (Join-Path $sourceRoot 'src\shuntie_quantizer.rs') -Force
$librarySource=Join-Path $sourceRoot 'src\lib.rs'
$libraryText=[IO.File]::ReadAllText($librarySource)
if(-not $libraryText.Contains('mod shuntie_quantizer;')) { [IO.File]::WriteAllText($librarySource,$libraryText+"`nmod shuntie_quantizer;`n",[Text.UTF8Encoding]::new($false)) }
& cargo build --manifest-path (Join-Path $sourceRoot 'Cargo.toml') --target x86_64-pc-windows-gnu --release --locked --features capi
if ($LASTEXITCODE -ne 0) { throw 'Gifski 构建失败。' }
$target=Join-Path $sourceRoot 'target\x86_64-pc-windows-gnu\release'
$bin=Join-Path $projectRoot 'dist\bin';New-Item -ItemType Directory -Force $bin | Out-Null
Copy-Item -LiteralPath (Join-Path $target 'gifski.exe'),(Join-Path $target 'gifski.dll') -Destination $bin -Force
Write-Output 'Gifski 1.34.0 GNU 构建完成，仅依赖 Windows 系统组件。'
