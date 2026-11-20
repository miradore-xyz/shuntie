# 瞬贴 · Shuntie

Windows 截图、可编辑标注、桌面贴图、长截图与区域录屏（GIF / MP4 视频）工具。使用小型深色圆角界面和 Lucide 图标，支持中文与 English（英语）。

- 主窗直接提供 **截图 / 长截图 / 录 GIF / 录视频** 2×2 快捷按钮，设置支持自定义 5 组全局快捷键（`F1`、`F3`、`Alt+G`、`Alt+L`、`Alt+V`）。
- 悬停识别窗口、拖动自由框选，选区四角和边缘可继续调整；方框、箭头、画笔和马赛克可选择、移动、缩放、旋转。
- 支持长截图：主窗快捷按钮、Alt+L、截图工具栏（或按 `L`）/ 托盘菜单均可进入，自动过滤滚动条与固定页眉/页脚，支持手动滚轮实时拼接与自动向下滚动。
- 录屏支持 **录 GIF** 与 **录视频（MP4）** 双模式（完成后均提供 **复制视频** 按钮），显示范围边框、暂停/继续、鼠标点击波纹高亮与暂停快照复制：GIF 采用单全局调色板（`Disposal=1` + 变化区域透明差分矩形 + Kornel 一维误差扩散有损 LZW）保留 **原生 2K（≤2560px）** 画质并大幅减小体积、兼容 QQ 群聊；MP4 优先保留 **原生 2K** 分辨率并通过 `libx264` CRF + VBV 自适应压至 20 MB 以内。
- 一键安装，默认勾选开机启动，可手动取消。

安装包和便携版见本仓库的 Releases（发行版）页面。详细操作和当前边界见 [中文说明](README.zh-CN.md)。

## 本地构建

安装 Zig 0.17.0 后，在 PowerShell 中执行：

```powershell
# 下载固定版本的录制和 GIF 编码器。
./tools/fetch_dependencies.ps1
# 编译主程序，并检查对象几何、中英文切换与视频 ≤20MB 压缩测试（含 video_compress_test）。
./build.ps1 -Test
# 后台 GIF 文件回归与 QQ 兼容测试：需 Python 与 Pillow，不操作桌面。
python ./tests/gif_native_test.py
python ./tests/gif_qq_compat_test.py
# 生成安装版和便携版，需要 Inno Setup 的 ISCC.exe。
./package.ps1 -InnoCompiler 'C:\路径\ISCC.exe'
```

原生应用核心使用 C++17、Win32、Direct2D 和 DirectWrite；Zig 负责构建，不需要 VC++ 运行库。第三方组件和许可见 [第三方说明](THIRD_PARTY_NOTICES.zh-CN.txt)。

首个发行版本仍处于早期阶段，验证范围见 [验收说明](VALIDATION.zh-CN.md)。
