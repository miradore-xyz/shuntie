"""后台文件回归：原尺寸、逐像素细线、局部更新、换场和播放时间。无桌面操作。"""
import json
import subprocess
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageSequence

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "artifacts" / "native-gif-regression"
OUT.mkdir(parents=True, exist_ok=True)


def run(*args):
    subprocess.run([str(x) for x in args], check=True, creationflags=subprocess.CREATE_NO_WINDOW,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def check(width, height):
    folder = OUT / f"{width}x{height}"
    folder.mkdir(exist_ok=True)
    base = Image.new("RGB", (width, height), (0, 0, 0))
    draw = ImageDraw.Draw(base)
    # 单像素黑白条纹可发现任何缩小再放大；左右和底部标记检查完整画幅。
    for x in range(64, 192, 2):
        draw.line((x, 32, x, 128), fill="white")
    draw.rectangle((0, 0, 8, 8), fill="red")
    draw.rectangle((width - 9, height - 9, width - 1, height - 1), fill="green")
    moved = base.copy()
    ImageDraw.Draw(moved).rectangle((201, 202, 211, 212), fill="red")
    moved_again = base.copy()
    ImageDraw.Draw(moved_again).rectangle((250, 220, 260, 230), fill="red")
    scene = Image.new("RGB", (width, height), (0, 0, 255))
    ImageDraw.Draw(scene).line((0, height // 2, width - 1, height // 2), fill="white", width=1)
    expected = [base, base, moved, moved, moved_again, moved_again, scene, scene]
    for n, image in enumerate(expected):
        image.save(folder / f"frame{n:02d}.png")
    video = folder / "source.mp4"
    run(ROOT / "dist/bin/ffmpeg.exe", "-hide_banner", "-loglevel", "error", "-y",
        "-framerate", "10", "-i", folder / "frame%02d.png", "-c:v", "libx264rgb",
        "-crf", "0", "-preset", "ultrafast", video)
    for fps in (30, 50):
        output = folder / f"result-{fps}.gif"
        run(ROOT / "tools/gif-export-test.exe", video, output, width, height, fps)
        image = Image.open(output)
        assert image.size == (width, height), (image.size, width, height)
        assert image.info.get("loop") == 0, "无限循环扩展不正确"
        elapsed = 0
        for frame in ImageSequence.Iterator(image):
            index = min(len(expected) - 1, int((elapsed + 15) / 100))
            assert ImageChops.difference(frame.convert("RGB"), expected[index]).getbbox() is None, (width, fps, elapsed, "像素或增量叠加不一致")
            delay = frame.info["duration"]
            assert delay >= 20, "帧延迟不能低于浏览器常用的 20 毫秒限制"
            elapsed += delay
        assert elapsed == 800, (elapsed, "静止画面也必须保留播放时间")
        print(json.dumps({"尺寸": image.size, "帧率目标": fps, "总时长毫秒": elapsed,
                          "逐像素": "通过", "文件字节": output.stat().st_size}, ensure_ascii=False))


for size in ((640, 480), (2560, 1600)):
    check(*size)
