"""QQ 兼容 GIF89a 格式结构、体积与转换耗时自动化测试。"""
import struct
import subprocess
import time
from pathlib import Path
from PIL import Image, ImageDraw, ImageSequence

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "artifacts" / "gif-qq-compat"
OUT.mkdir(parents=True, exist_ok=True)
FFMPEG = ROOT / "dist" / "bin" / "ffmpeg.exe"
EXPORTER = ROOT / "dist" / "bin" / "gifski-export.exe"


def generate_test_video(path: Path, width: int = 1920, height: int = 1080, fps: int = 30, seconds: int = 5):
    total_frames = fps * seconds
    cmd = [
        str(FFMPEG), "-hide_banner", "-loglevel", "error", "-y",
        "-f", "rawvideo", "-pixel_format", "rgb24",
        "-video_size", f"{width}x{height}", "-framerate", str(fps),
        "-i", "-", "-c:v", "libx264", "-preset", "ultrafast", "-crf", "18",
        "-pix_fmt", "yuv420p", str(path),
    ]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
    assert proc.stdin is not None

    # 预先构建一张 1920x1080 基础渐变 + UI 卡片底图，避免每帧重绘整张背景耗时过久。
    bg = Image.new("RGB", (width, height), (22, 26, 31))
    bd = ImageDraw.Draw(bg)
    for y in range(0, height, 6):
        c = int(22 + 30 * (y / height))
        bd.rectangle((0, y, width, y + 6), fill=(c, c + 6, c + 14))
    bd.rounded_rectangle((80, 80, 920, 980), radius=18, fill=(32, 38, 46), outline=(70, 81, 94), width=2)
    bd.rounded_rectangle((980, 80, 1840, 980), radius=18, fill=(246, 248, 250), outline=(210, 218, 226), width=2)
    for row in range(18):
        y = 140 + row * 44
        bd.rectangle((120, y, 420 + (row * 53) % 420, y + 18), fill=(124, 230, 197) if row % 5 == 0 else (180, 192, 204))
        bd.rectangle((1030, y, 1380 + (row * 67) % 380, y + 18), fill=(52, 120, 246) if row % 4 == 0 else (72, 82, 96))

    for i in range(total_frames):
        frame = bg.copy()
        d = ImageDraw.Draw(frame)
        # 模拟连续平滑移动的窗口卡片
        mx = 220 + int((i / total_frames) * 1050)
        my = 280 + int(120 * ((i % 45) / 45.0))
        d.rounded_rectangle((mx, my, mx + 380, my + 240), radius=14, fill=(38, 47, 58), outline=(124, 230, 197), width=3)
        d.rectangle((mx + 24, my + 24, mx + 220, my + 48), fill=(236, 240, 243))
        d.rectangle((mx + 24, my + 72, mx + 340, my + 190), fill=(52, 63, 78))
        # 模拟鼠标点击波纹（左键红圈与右键蓝圈）
        if 30 <= i <= 45:
            r = 10 + (i - 30) * 2
            d.ellipse((mx + 190 - r, my + 120 - r, mx + 190 + r, my + 120 + r), outline=(255, 90, 78), width=4)
        if 90 <= i <= 105:
            r = 10 + (i - 90) * 2
            d.ellipse((mx + 140 - r, my + 90 - r, mx + 140 + r, my + 90 + r), outline=(59, 130, 246), width=4)
        proc.stdin.write(frame.tobytes())

    proc.stdin.close()
    assert proc.wait() == 0


def parse_and_verify_gif89a(gif_path: Path, expected_w: int, expected_h: int):
    data = gif_path.read_bytes()
    assert data[:6] == b"GIF89a", f"Invalid GIF header: {data[:6]!r}"
    lsd_w, lsd_h, packed, _, _ = struct.unpack_from("<HHBBB", data, 6)
    assert (lsd_w, lsd_h) == (expected_w, expected_h), ((lsd_w, lsd_h), (expected_w, expected_h))
    gctf = (packed >> 7) & 1
    gct_size = 1 << ((packed & 7) + 1)
    assert gctf == 1, "Global Color Table missing!"

    pos = 13 + gct_size * 3
    has_netscape = False
    gces = []
    frames = []

    while pos < len(data):
        intro = data[pos]
        pos += 1
        if intro == 0x3B:  # Trailer
            break
        elif intro == 0x21:  # Extension
            label = data[pos]
            pos += 1
            if label == 0xF9:  # GCE
                blk_size = data[pos]
                assert blk_size == 4
                gce_packed, delay_cs, trans_idx = struct.unpack_from("<BHB", data, pos + 1)
                trans_flag = gce_packed & 1
                disposal = (gce_packed >> 2) & 7
                assert trans_flag == 0, f"Transparency flag must be 0 for QQ compatibility, got {trans_flag}"
                assert delay_cs >= 2, f"Frame delay too small: {delay_cs * 10}ms"
                gces.append((disposal, trans_flag, delay_cs))
                pos += 1 + blk_size
                assert data[pos] == 0
                pos += 1
            else:
                if label == 0xFF:
                    blk_len = data[pos]
                    app_id = data[pos + 1 : pos + 1 + blk_len]
                    if app_id == b"NETSCAPE2.0":
                        has_netscape = True
                while True:
                    sz = data[pos]
                    pos += 1
                    if sz == 0:
                        break
                    pos += sz
        elif intro == 0x2C:  # Image Descriptor
            left, top, w, h, img_packed = struct.unpack_from("<HHHHB", data, pos)
            pos += 9
            lctf = (img_packed >> 7) & 1
            assert lctf == 0, "Local Color Table must be 0!"
            assert (left, top, w, h) == (0, 0, expected_w, expected_h), ((left, top, w, h), (expected_w, expected_h))
            frames.append((left, top, w, h))
            min_code_size = data[pos]
            assert 2 <= min_code_size <= 8
            pos += 1
            while True:
                sz = data[pos]
                pos += 1
                if sz == 0:
                    break
                pos += sz
        else:
            raise AssertionError(f"Unexpected GIF block introducer 0x{intro:02x} at {pos - 1}")

    assert has_netscape, "NETSCAPE2.0 loop extension missing"
    assert len(frames) > 10 and len(frames) == len(gces), (len(frames), len(gces))

    total_ms = sum(d * 10 for _, _, d in gces)
    assert 4850 <= total_ms <= 5150, f"Unexpected total duration: {total_ms}ms"

    # Pillow 逐帧解码校验无损坏
    with Image.open(gif_path) as im:
        assert im.size == (expected_w, expected_h)
        assert im.info.get("loop") == 0
        decoded_count = 0
        for fr in ImageSequence.Iterator(im):
            fr.convert("RGB")
            decoded_count += 1
        assert decoded_count == len(frames)

    return len(frames), total_ms


def main():
    video = OUT / "synthetic_1080p_5s.mp4"
    generate_test_video(video)

    # 1) 均衡模式：15 fps，最长边 1080 -> 1080x608，且体积 <= 12 MB
    balanced_gif = OUT / "balanced_1080.gif"
    balanced_log = OUT / "balanced_1080.log"
    balanced_log.unlink(missing_ok=True)
    t0 = time.perf_counter()
    subprocess.run(
        [str(EXPORTER), str(FFMPEG), "gifski.exe", str(video), str(balanced_gif), str(balanced_log),
         "15", "1920", "1080", "1080", "1"],
        check=True, creationflags=subprocess.CREATE_NO_WINDOW
    )
    elapsed_bal = time.perf_counter() - t0
    bal_frames, bal_ms = parse_and_verify_gif89a(balanced_gif, 1080, 608)
    bal_bytes = balanced_gif.stat().st_size
    assert bal_bytes <= 12 * 1024 * 1024, f"Balanced GIF too large: {bal_bytes} bytes"
    assert elapsed_bal < 5.0, f"Balanced GIF export too slow: {elapsed_bal:.2f}s"

    # 2) 小体积模式：12 fps，最长边 720 -> 720x404，且体积 <= 6 MB
    compact_gif = OUT / "compact_720.gif"
    compact_log = OUT / "compact_720.log"
    compact_log.unlink(missing_ok=True)
    t1 = time.perf_counter()
    subprocess.run(
        [str(EXPORTER), str(FFMPEG), "gifski.exe", str(video), str(compact_gif), str(compact_log),
         "15", "1920", "1080", "720", "0"],
        check=True, creationflags=subprocess.CREATE_NO_WINDOW
    )
    elapsed_cmp = time.perf_counter() - t1
    cmp_frames, cmp_ms = parse_and_verify_gif89a(compact_gif, 720, 404)
    cmp_bytes = compact_gif.stat().st_size
    assert cmp_bytes <= 6 * 1024 * 1024, f"Compact GIF too large: {cmp_bytes} bytes"
    assert cmp_bytes < bal_bytes, (cmp_bytes, bal_bytes)
    assert elapsed_cmp < 5.0, f"Compact GIF export too slow: {elapsed_cmp:.2f}s"

    print(f"Balanced (1080x608, {bal_frames} frames, {bal_ms}ms): {bal_bytes / (1024*1024):.2f} MB in {elapsed_bal:.3f}s")
    print(f"Compact  (720x404,  {cmp_frames} frames, {cmp_ms}ms): {cmp_bytes / (1024*1024):.2f} MB in {elapsed_cmp:.3f}s")
    print("QQ compatibility & performance checks passed.")


if __name__ == "__main__":
    main()
