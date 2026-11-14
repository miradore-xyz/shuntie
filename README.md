# 瞬贴 · Shuntie

Windows 截图、可编辑标注、桌面贴图和区域录屏工具。使用小型深色圆角界面和 Lucide 图标，支持中文与 English（英语）。

- 悬停识别窗口、拖动自由框选，选区四角和边缘可继续调整；方框、箭头、画笔和马赛克可选择、移动、缩放、旋转。
- 录屏支持范围边框、暂停/继续、复制已录 GIF 和结束录制。
- 一键安装，默认勾选开机启动，可手动取消。

安装包和便携版见本仓库的 Releases（发行版）页面。详细操作和当前边界见 [中文说明](README.zh-CN.md)。

## 本地构建

安装 Zig 0.17.0 后，在 PowerShell 中执行：

```powershell
# 编译主程序，并检查对象几何和中英文切换。
./build.ps1 -Test
# 下载固定版本的录制和 GIF 编码器。
./tools/fetch_dependencies.ps1
# 生成安装版和便携版，需要 Inno Setup 的 ISCC.exe。
./package.ps1 -InnoCompiler 'C:\路径\ISCC.exe'
```

原生应用核心使用 C++17、Win32、Direct2D 和 DirectWrite；Zig 负责构建，不需要 VC++ 运行库。第三方组件和许可见 [第三方说明](THIRD_PARTY_NOTICES.zh-CN.txt)。

首个发行版本仍处于早期阶段，验证范围见 [验收说明](VALIDATION.zh-CN.md)。
