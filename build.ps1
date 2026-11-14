param([switch]$Test)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
New-Item -ItemType Directory -Force -Path 'dist' | Out-Null
& zig rc /fo tools/app.res assets/app.rc
if ($LASTEXITCODE -ne 0) { throw '图标资源编译失败' }
# 使用 Zig 随附的 C++ 编译器，静态链接标准库，无需安装 VC++ 运行库。
& zig c++ -std=c++17 -O2 -DNDEBUG -municode '-Wl,--subsystem,windows' src/main.cpp src/recorder.cpp tools/app.res -o dist/瞬贴.exe -luser32 -lgdi32 -lshell32 -lcomdlg32 -lole32 -luuid -ld2d1 -ldwrite -lwindowscodecs -ldwmapi -ldxgi -ladvapi32 -lcomctl32 -luxtheme
if ($LASTEXITCODE -ne 0) { throw '编译失败' }
New-Item -ItemType Directory -Force -Path 'dist\bin' | Out-Null
& zig c++ -std=c++17 -O2 -municode third_party/gifski-export.cpp -o dist/bin/gifski-export.exe -luser32
if ($LASTEXITCODE -ne 0) { throw 'GIF 转换器编译失败' }
if ($Test) {
    & zig c++ -std=c++17 -O2 -UNDEBUG tests/model_test.cpp -o tools/model-test.exe
    if ($LASTEXITCODE -ne 0) { throw '测试编译失败' }
    & .\tools\model-test.exe
    if ($LASTEXITCODE -ne 0) { throw '模型测试失败' }
    & zig c++ -std=c++17 -O2 -UNDEBUG tests/language_test.cpp -o tools/language-test.exe -luser32
    if ($LASTEXITCODE -ne 0) { throw '语言测试编译失败' }
    & .\tools\language-test.exe
    if ($LASTEXITCODE -ne 0) { throw '语言测试失败' }
    & zig c++ -std=c++17 -O2 -UNDEBUG tests/selection_render_test.cpp -o tools/selection-render-test.exe -luser32 -lgdi32 -lole32 -luuid -ld2d1 -ldwrite -lwindowscodecs
    if ($LASTEXITCODE -ne 0) { throw '选中框渲染检查编译失败' }
    & .\tools\selection-render-test.exe
    if ($LASTEXITCODE -ne 0) { throw '选中框渲染检查失败' }
}
Write-Output '构建完成：dist\瞬贴.exe'
