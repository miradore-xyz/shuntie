#pragma once
#include "graphics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
#include <vector>

enum class StitchResult {
    Initial,
    Appended,
    NoChange,
    TooFast,
    MaxHeightReached,
    SizeMismatch
};

// 垂直长截图拼接器：自动过滤右侧滚动条干扰，识别固定页眉/页脚，使用由粗到精的 SSE2 图像配准。
struct ScrollStitcher {
    static constexpr int MAX_HEIGHT = 30000;
    static constexpr size_t MAX_BYTES = 256ull * 1024 * 1024;
    static constexpr int FP_BINS = 64;

    int width = 0;
    int bodyHeight = 0;
    int stickyTop = 0;
    int stickyBottom = 0;
    bool stickyDetected = false;
    Image prevFrame;
    std::vector<unsigned char> bodyPixels;
    std::vector<unsigned char> latestFooter;

    int currentHeight() const {
        if (width <= 0) return 0;
        int footerH = int(latestFooter.size() / (size_t(width) * 4));
        return bodyHeight + footerH;
    }

    void reset(const Image& firstFrame) {
        width = 0;
        bodyHeight = 0;
        stickyTop = 0;
        stickyBottom = 0;
        stickyDetected = false;
        prevFrame = {};
        bodyPixels.clear();
        latestFooter.clear();
        if (!firstFrame.valid()) return;
        width = firstFrame.width;
        bodyHeight = firstFrame.height;
        bodyPixels = firstFrame.pixels;
        prevFrame = firstFrame;
    }

    StitchResult append(const Image& nextFrame) {
        if (!prevFrame.valid()) {
            reset(nextFrame);
            return prevFrame.valid() ? StitchResult::Initial : StitchResult::SizeMismatch;
        }
        if (!nextFrame.valid() || nextFrame.width != width || nextFrame.height != prevFrame.height) {
            return StitchResult::SizeMismatch;
        }

        const int w = width;
        const int h = prevFrame.height;
        const int scrollMargin = std::min(24, w / 10);
        const int cmpW = std::max(1, w - scrollMargin);

        std::vector<float> rowDiff(h, 0.0f);
        for (int y = 0; y < h; ++y) {
            const uint8_t* a = prevFrame.pixels.data() + size_t(y) * w * 4;
            const uint8_t* b = nextFrame.pixels.data() + size_t(y) * w * 4;
            uint64_t sum = 0;
            int x = 0;
            for (; x + 4 <= cmpW; x += 4) {
                __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + x * 4));
                __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + x * 4));
                __m128i s = _mm_sad_epu8(va, vb);
                sum += uint64_t(_mm_cvtsi128_si64(s)) + uint64_t(_mm_cvtsi128_si64(_mm_srli_si128(s, 8)));
            }
            for (; x < cmpW; ++x) {
                sum += uint64_t(std::abs(int(a[x * 4 + 0]) - int(b[x * 4 + 0])));
                sum += uint64_t(std::abs(int(a[x * 4 + 1]) - int(b[x * 4 + 1])));
                sum += uint64_t(std::abs(int(a[x * 4 + 2]) - int(b[x * 4 + 2])));
            }
            rowDiff[y] = float(double(sum) / double(cmpW * 3));
        }

        int checkTop = stickyDetected ? stickyTop : 0;
        int checkBot = stickyDetected ? (h - stickyBottom) : h;
        double bandSum = 0.0;
        int changedRows = 0;
        for (int y = checkTop; y < checkBot; ++y) {
            bandSum += rowDiff[y];
            if (rowDiff[y] > 1.5f) ++changedRows;
        }
        double bandMean = checkBot > checkTop ? bandSum / double(checkBot - checkTop) : 0.0;
        if (changedRows < 3 && bandMean < 0.8) {
            return StitchResult::NoChange;
        }

        // 计算候选固定页眉/页脚高度（在配准成功前不提交任何状态变更）。
        int topUnchanged = 0;
        int maxSticky = h / 3;
        while (topUnchanged < maxSticky && rowDiff[topUnchanged] <= 1.2f) ++topUnchanged;
        int botUnchanged = 0;
        while (botUnchanged < maxSticky && rowDiff[h - 1 - botUnchanged] <= 1.2f) ++botUnchanged;

        int candTop = stickyTop;
        int candBot = stickyBottom;
        if (!stickyDetected) {
            candTop = topUnchanged >= 8 ? topUnchanged : 0;
            candBot = botUnchanged >= 8 ? botUnchanged : 0;
            int minContent = std::max(64, h / 3);
            if (h - candTop - candBot < minContent) {
                candTop = 0;
                candBot = 0;
            }
        } else {
            if (topUnchanged < candTop) candTop = topUnchanged >= 8 ? topUnchanged : 0;
            if (botUnchanged < candBot) candBot = botUnchanged >= 8 ? botUnchanged : 0;
        }

        const int contentTop = candTop;
        const int contentBottom = h - candBot;
        const int contentH = contentBottom - contentTop;
        const int minOverlap = std::clamp(contentH / 6, 24, 120);
        const int maxDy = contentH - minOverlap;
        if (maxDy < 1) return StitchResult::TooFast;

        // 粗搜索：为每行构建 64-byte 水平灰度指纹与梯度能量。
        std::vector<uint8_t> prevFp(size_t(h) * FP_BINS);
        std::vector<uint8_t> currFp(size_t(h) * FP_BINS);
        auto buildFingerprints = [&](const Image& im, std::vector<uint8_t>& fp) {
            for (int y = 0; y < h; ++y) {
                const uint8_t* row = im.pixels.data() + size_t(y) * w * 4;
                uint8_t* dst = fp.data() + size_t(y) * FP_BINS;
                for (int b = 0; b < FP_BINS; ++b) {
                    int x0 = (b * cmpW) / FP_BINS;
                    int x1 = std::max(x0 + 1, ((b + 1) * cmpW) / FP_BINS);
                    int step = std::max(1, (x1 - x0) / 8);
                    uint32_t acc = 0, cnt = 0;
                    for (int x = x0; x < x1; x += step) {
                        const uint8_t* px = row + x * 4;
                        acc += (uint32_t(px[2]) * 77u + uint32_t(px[1]) * 150u + uint32_t(px[0]) * 29u) >> 8;
                        ++cnt;
                    }
                    dst[b] = uint8_t(acc / cnt);
                }
            }
        };
        buildFingerprints(prevFrame, prevFp);
        buildFingerprints(nextFrame, currFp);

        std::vector<uint64_t> currEnergyPrefix(size_t(h + 1), 0);
        for (int y = 0; y < h; ++y) {
            const uint8_t* row = currFp.data() + size_t(y) * FP_BINS;
            const uint8_t* above = y > 0 ? currFp.data() + size_t(y - 1) * FP_BINS : row;
            uint64_t e = 0;
            for (int b = 1; b < FP_BINS; ++b) {
                e += uint64_t(std::abs(int(row[b]) - int(row[b - 1])));
                e += uint64_t(std::abs(int(row[b]) - int(above[b])));
            }
            currEnergyPrefix[y + 1] = currEnergyPrefix[y] + e;
        }
        double contentEnergy = double(currEnergyPrefix[contentBottom] - currEnergyPrefix[contentTop]) / double(std::max(1, contentH) * FP_BINS);

        struct Cand {
            int dy;
            double score;
        };
        std::vector<Cand> cands;
        cands.reserve(size_t(maxDy));
        for (int dy = 1; dy <= maxDy; ++dy) {
            int overlap = contentH - dy;
            uint64_t sadSum = 0;
            for (int k = 0; k < overlap; ++k) {
                const uint8_t* a = prevFp.data() + size_t(contentTop + dy + k) * FP_BINS;
                const uint8_t* b = currFp.data() + size_t(contentTop + k) * FP_BINS;
                for (int off = 0; off < FP_BINS; off += 16) {
                    __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + off));
                    __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + off));
                    __m128i s = _mm_sad_epu8(va, vb);
                    sadSum += uint64_t(_mm_cvtsi128_si64(s)) + uint64_t(_mm_cvtsi128_si64(_mm_srli_si128(s, 8)));
                }
            }
            double meanSad = double(sadSum) / double(size_t(overlap) * FP_BINS);
            double meanEnergy = double(currEnergyPrefix[contentTop + overlap] - currEnergyPrefix[contentTop]) / double(size_t(overlap) * FP_BINS);
            double score = meanSad + ((contentEnergy > 1.0 && meanEnergy < 0.25) ? 25.0 : 0.0) - 0.0002 * double(overlap);
            cands.push_back({dy, score});
        }

        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
        std::vector<int> topShifts;
        for (const auto& c : cands) {
            bool nearExisting = false;
            for (int s : topShifts) {
                if (std::abs(c.dy - s) <= 3) { nearExisting = true; break; }
            }
            if (!nearExisting) {
                topShifts.push_back(c.dy);
                if (topShifts.size() >= 3) break;
            }
        }

        // 细搜索：全像素精确 MAD 校验。
        auto evalExactDy = [&](int dy, double& outMad, double& outGoodFrac) {
            int overlap = contentH - dy;
            uint64_t totalSad = 0;
            uint64_t totalChecked = 0;
            uint64_t goodPixels = 0;
            int rowStep = overlap > 600 ? 2 : 1;
            int colStep = cmpW > 1600 ? 2 : 1;
            for (int k = 0; k < overlap; k += rowStep) {
                const uint8_t* a = prevFrame.pixels.data() + size_t(contentTop + dy + k) * w * 4;
                const uint8_t* b = nextFrame.pixels.data() + size_t(contentTop + k) * w * 4;
                for (int x = 0; x < cmpW; x += colStep) {
                    int d0 = std::abs(int(a[x * 4 + 0]) - int(b[x * 4 + 0]));
                    int d1 = std::abs(int(a[x * 4 + 1]) - int(b[x * 4 + 1]));
                    int d2 = std::abs(int(a[x * 4 + 2]) - int(b[x * 4 + 2]));
                    totalSad += uint64_t(d0 + d1 + d2);
                    if (d0 <= 20 && d1 <= 20 && d2 <= 20) ++goodPixels;
                    ++totalChecked;
                }
            }
            outMad = totalChecked ? double(totalSad) / double(totalChecked * 3) : 255.0;
            outGoodFrac = totalChecked ? double(goodPixels) / double(totalChecked) : 0.0;
        };

        int bestDy = -1;
        double bestMad = 1e9;
        double bestGood = 0.0;
        for (int center : topShifts) {
            int lo = std::max(1, center - 3);
            int hi = std::min(maxDy, center + 3);
            for (int dy = lo; dy <= hi; ++dy) {
                double mad = 255.0, good = 0.0;
                evalExactDy(dy, mad, good);
                if (mad < bestMad) {
                    bestMad = mad;
                    bestGood = good;
                    bestDy = dy;
                }
            }
        }

        if (bestDy < 1 || bestMad >= 9.0 || bestGood < 0.85) {
            return StitchResult::TooFast;
        }
        int bestOverlap = contentH - bestDy;
        double bestEnergy = double(currEnergyPrefix[contentTop + bestOverlap] - currEnergyPrefix[contentTop]) / double(size_t(bestOverlap) * FP_BINS);
        if (contentEnergy > 1.0 && bestEnergy < 0.05) {
            return StitchResult::TooFast;
        }

        // 配准成功，提交固定区域状态及新增内容。
        int adjustedBodyHeight = bodyHeight;
        if (!stickyDetected) {
            if (candBot > 0) adjustedBodyHeight -= candBot;
        } else if (candBot < stickyBottom) {
            adjustedBodyHeight += (stickyBottom - candBot);
        }

        const int maxRowsByMem = int(MAX_BYTES / (size_t(w) * 4));
        const int maxRowsAllowed = std::min(MAX_HEIGHT, maxRowsByMem);
        int room = maxRowsAllowed - adjustedBodyHeight - candBot;
        if (room <= 0) {
            return StitchResult::MaxHeightReached;
        }
        bool hitCap = false;
        int appendDy = bestDy;
        if (appendDy > room) {
            appendDy = room;
            hitCap = true;
        }

        if (!stickyDetected) {
            stickyTop = candTop;
            stickyBottom = candBot;
            stickyDetected = true;
            if (stickyBottom > 0) {
                size_t footerBytes = size_t(stickyBottom) * w * 4;
                if (bodyPixels.size() >= footerBytes) {
                    bodyPixels.resize(bodyPixels.size() - footerBytes);
                    bodyHeight -= stickyBottom;
                }
            }
        } else {
            stickyTop = candTop;
            if (candBot < stickyBottom) {
                int restoreRows = stickyBottom - candBot;
                const uint8_t* src = prevFrame.pixels.data() + size_t(h - stickyBottom) * w * 4;
                bodyPixels.insert(bodyPixels.end(), src, src + size_t(restoreRows) * w * 4);
                bodyHeight += restoreRows;
                stickyBottom = candBot;
            }
        }

        const uint8_t* appendSrc = nextFrame.pixels.data() + size_t(contentBottom - appendDy) * w * 4;
        bodyPixels.insert(bodyPixels.end(), appendSrc, appendSrc + size_t(appendDy) * w * 4);
        bodyHeight += appendDy;

        if (stickyBottom > 0) {
            const uint8_t* fSrc = nextFrame.pixels.data() + size_t(contentBottom) * w * 4;
            latestFooter.assign(fSrc, fSrc + size_t(stickyBottom) * w * 4);
        } else {
            latestFooter.clear();
        }
        prevFrame = nextFrame;

        return hitCap ? StitchResult::MaxHeightReached : StitchResult::Appended;
    }

    Image finalize() const {
        Image out;
        if (width <= 0 || bodyHeight <= 0) return out;
        int footerH = int(latestFooter.size() / (size_t(width) * 4));
        out.width = width;
        out.height = bodyHeight + footerH;
        out.pixels.reserve(bodyPixels.size() + latestFooter.size());
        out.pixels.insert(out.pixels.end(), bodyPixels.begin(), bodyPixels.end());
        out.pixels.insert(out.pixels.end(), latestFooter.begin(), latestFooter.end());
        return out;
    }

    Image tailPreview(int maxRows) const {
        Image out;
        if (width <= 0 || bodyHeight <= 0 || maxRows <= 0) return out;
        int footerH = int(latestFooter.size() / (size_t(width) * 4));
        int totalH = bodyHeight + footerH;
        int takeH = std::min(totalH, maxRows);
        int startRow = totalH - takeH;
        out.width = width;
        out.height = takeH;
        out.pixels.reserve(size_t(takeH) * size_t(width) * 4);
        if (startRow < bodyHeight) {
            size_t bodyStartByte = size_t(startRow) * size_t(width) * 4;
            out.pixels.insert(out.pixels.end(), bodyPixels.begin() + ptrdiff_t(bodyStartByte), bodyPixels.end());
            if (!latestFooter.empty()) {
                out.pixels.insert(out.pixels.end(), latestFooter.begin(), latestFooter.end());
            }
        } else {
            size_t footerStartByte = size_t(startRow - bodyHeight) * size_t(width) * 4;
            out.pixels.insert(out.pixels.end(), latestFooter.begin() + ptrdiff_t(footerStartByte), latestFooter.end());
        }
        return out;
    }
};
