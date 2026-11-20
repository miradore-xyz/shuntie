#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include "../src/scroll_stitch.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static Image makeSyntheticPage(int width, int height) {
    Image im;
    im.width = width;
    im.height = height;
    im.pixels.resize(size_t(width) * size_t(height) * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 每行构造非周期纹理与独一无二的纵向编码条带，防止伪重复匹配。
            uint8_t r = uint8_t((x * 3 + y * 5 + ((y / 23) * 37)) & 255);
            uint8_t g = uint8_t((x * 7 + y * 2 + ((y / 17) * 53)) & 255);
            uint8_t b = uint8_t((x * 11 + y * 13 + ((x / 31) * 29)) & 255);
            if (y % 40 < 4 && (x > 30 && x < width - 40)) {
                r = uint8_t(30 + (y / 40) * 19);
                g = uint8_t(210 - ((y / 40) * 7) % 180);
                b = 240;
            }
            size_t o = (size_t(y) * size_t(width) + size_t(x)) * 4;
            im.pixels[o + 0] = b;
            im.pixels[o + 1] = g;
            im.pixels[o + 2] = r;
            im.pixels[o + 3] = 255;
        }
    }
    return im;
}

static Image cropRows(const Image& src, int y0, int height) {
    Image out;
    out.width = src.width;
    out.height = height;
    size_t rowBytes = size_t(src.width) * 4;
    out.pixels.assign(
        src.pixels.begin() + ptrdiff_t(size_t(y0) * rowBytes),
        src.pixels.begin() + ptrdiff_t(size_t(y0 + height) * rowBytes)
    );
    return out;
}

static bool rowsEqual(const Image& a, int ay, const Image& b, int by, int rows, int cmpWidth) {
    for (int r = 0; r < rows; ++r) {
        const uint8_t* pa = a.pixels.data() + size_t(ay + r) * size_t(a.width) * 4;
        const uint8_t* pb = b.pixels.data() + size_t(by + r) * size_t(b.width) * 4;
        for (int x = 0; x < cmpWidth; ++x) {
            if (pa[x * 4 + 0] != pb[x * 4 + 0] ||
                pa[x * 4 + 1] != pb[x * 4 + 1] ||
                pa[x * 4 + 2] != pb[x * 4 + 2]) {
                return false;
            }
        }
    }
    return true;
}

int wmain() {
    // 测试 1：纯滚动页面多步拼接与逐像素校验
    {
        Image fullPage = makeSyntheticPage(640, 1400);
        ScrollStitcher stitcher;
        const int viewH = 400;
        const int offsets[] = {0, 120, 260, 410, 580, 740, 900, 1000};
        stitcher.reset(cropRows(fullPage, offsets[0], viewH));
        for (int i = 1; i < 8; ++i) {
            auto res = stitcher.append(cropRows(fullPage, offsets[i], viewH));
            assert(res == StitchResult::Appended);
        }
        Image result = stitcher.finalize();
        assert(result.width == 640 && result.height == 1400);
        assert(rowsEqual(result, 0, fullPage, 0, 1400, 640));
    }

    // 测试 2：60px 固定页眉 + 50px 固定页脚 + 右侧 18px 移动滚动条
    {
        const int w = 640, viewH = 400, headerH = 60, footerH = 50;
        const int bodyH = viewH - headerH - footerH; // 290
        Image body = makeSyntheticPage(w, 1200);
        Image header = makeSyntheticPage(w, headerH);
        Image footer = makeSyntheticPage(w, footerH);
        // 让页眉/页脚视觉区分明显
        for (size_t i = 0; i < header.pixels.size(); i += 4) header.pixels[i + 2] = 220;
        for (size_t i = 0; i < footer.pixels.size(); i += 4) footer.pixels[i + 1] = 220;

        auto makeStickyFrame = [&](int scrollY, int thumbStep) {
            Image frame;
            frame.width = w;
            frame.height = viewH;
            frame.pixels.resize(size_t(w) * size_t(viewH) * 4);
            size_t rowBytes = size_t(w) * 4;
            std::memcpy(frame.pixels.data(), header.pixels.data(), size_t(headerH) * rowBytes);
            std::memcpy(frame.pixels.data() + size_t(headerH) * rowBytes,
                        body.pixels.data() + size_t(scrollY) * rowBytes,
                        size_t(bodyH) * rowBytes);
            std::memcpy(frame.pixels.data() + size_t(headerH + bodyH) * rowBytes,
                        footer.pixels.data(),
                        size_t(footerH) * rowBytes);
            // 在右侧 18px 区域绘制随滚动下移的滚动条滑块
            int thumbY = headerH + 10 + thumbStep * 45;
            for (int y = 0; y < viewH; ++y) {
                for (int x = w - 18; x < w; ++x) {
                    size_t o = (size_t(y) * size_t(w) + size_t(x)) * 4;
                    bool inThumb = (y >= thumbY && y < thumbY + 40);
                    frame.pixels[o + 0] = inThumb ? 200 : 40;
                    frame.pixels[o + 1] = inThumb ? 200 : 40;
                    frame.pixels[o + 2] = inThumb ? 200 : 40;
                    frame.pixels[o + 3] = 255;
                }
            }
            return frame;
        };

        ScrollStitcher stitcher;
        stitcher.reset(makeStickyFrame(0, 0));
        for (int step = 1; step <= 4; ++step) {
            auto res = stitcher.append(makeStickyFrame(step * 90, step));
            assert(res == StitchResult::Appended);
        }
        assert(stitcher.stickyTop == headerH);
        assert(stitcher.stickyBottom == footerH);
        Image out = stitcher.finalize();
        int expectedBodyH = bodyH + 4 * 90; // 290 + 360 = 650
        int expectedTotalH = headerH + expectedBodyH + footerH; // 760
        assert(out.height == expectedTotalH);
        assert(rowsEqual(out, 0, header, 0, headerH, w - 24));
        assert(rowsEqual(out, headerH, body, 0, expectedBodyH, w - 24));
        assert(rowsEqual(out, headerH + expectedBodyH, footer, 0, footerH, w - 24));
    }

    // 测试 3：静止无变化(NoChange)、大幅跳跃失配(TooFast)保护及后续恢复拼接
    {
        Image fullPage = makeSyntheticPage(640, 1200);
        ScrollStitcher stitcher;
        stitcher.reset(cropRows(fullPage, 0, 400));
        assert(stitcher.append(cropRows(fullPage, 0, 400)) == StitchResult::NoChange);
        assert(stitcher.currentHeight() == 400);

        // 跨越 600px 无重叠区域 -> TooFast 且状态保持不变
        assert(stitcher.append(cropRows(fullPage, 600, 400)) == StitchResult::TooFast);
        assert(stitcher.currentHeight() == 400);

        // 回到有效重叠范围 -> 正常继续拼接
        assert(stitcher.append(cropRows(fullPage, 100, 400)) == StitchResult::Appended);
        assert(stitcher.currentHeight() == 500);
        Image out = stitcher.finalize();
        assert(rowsEqual(out, 0, fullPage, 0, 500, 640));
    }

    std::cout << "Long screenshot stitching tests passed. / 长截图拼接测试通过。\n";
    return 0;
}
