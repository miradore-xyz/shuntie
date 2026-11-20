// SPDX-License-Identifier: AGPL-3.0-or-later
// 高速、低体积、QQ/微信全平台兼容的 GIF89a 编码器：
// 单一全局调色板 + 全画幅不透明帧（无局部调色板、无透明增量帧、无残影），支持多线程量化与 LZW 压缩。
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "native-video.hpp"

namespace native_gif_detail {

// SSE2 帧间相似判定：tol == 0 为逐字节精确相等；tol > 0 允许极小噪声容差，但任意单像素超出容差立即判定变化（保留小光标/点击特效）。
inline bool framesSimilar(const BYTE* a, const BYTE* b, size_t bytes, int tol) {
    size_t n = 0;
    if (tol <= 0) {
        for (; n + 16 <= bytes; n += 16) {
            if (_mm_movemask_epi8(_mm_cmpeq_epi8(
                    _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + n)),
                    _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + n)))) != 0xFFFF)
                return false;
        }
        for (; n < bytes; ++n) if (a[n] != b[n]) return false;
        return true;
    }
    const __m128i tv = _mm_set1_epi8(char(std::min(tol, 127)));
    const __m128i zero = _mm_setzero_si128();
    for (; n + 16 <= bytes; n += 16) {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + n));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + n));
        __m128i diff = _mm_or_si128(_mm_subs_epu8(va, vb), _mm_subs_epu8(vb, va));
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_subs_epu8(diff, tv), zero)) != 0xFFFF)
            return false;
    }
    for (; n < bytes; ++n) {
        int d = int(a[n]) - int(b[n]);
        if (d < -tol || d > tol) return false;
    }
    return true;
}

struct Rgb {
    uint8_t r, g, b;
};

// Xiaolin Wu 3D RGB 方差最小化全局调色板量化器（5-bit 直方图，约 3ms）。
struct WuQuantizer {
    struct Box {
        int r0, r1, g0, g1, b0, b1;
        int64_t vol;
    };
    static constexpr int SIDE = 33;
    static constexpr int TOTAL = SIDE * SIDE * SIDE;

    std::vector<int64_t> wt, mr, mg, mb;
    std::vector<double> m2;

    WuQuantizer() : wt(TOTAL, 0), mr(TOTAL, 0), mg(TOTAL, 0), mb(TOTAL, 0), m2(TOTAL, 0.0) {}

    static inline int idx(int r, int g, int b) {
        return (r * SIDE + g) * SIDE + b;
    }

    void addSamples(const uint32_t* rgba, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t px = rgba[i];
            int r = int(px & 0xFF);
            int g = int((px >> 8) & 0xFF);
            int b = int((px >> 16) & 0xFF);
            int ir = (r >> 3) + 1;
            int ig = (g >> 3) + 1;
            int ib = (b >> 3) + 1;
            int k = idx(ir, ig, ib);
            wt[k] += 1;
            mr[k] += r;
            mg[k] += g;
            mb[k] += b;
            m2[k] += double(r * r + g * g + b * b);
        }
    }

    void buildMoments() {
        for (int r = 1; r < SIDE; ++r) {
            int64_t area[SIDE]{}, areaR[SIDE]{}, areaG[SIDE]{}, areaB[SIDE]{};
            double area2[SIDE]{};
            for (int g = 1; g < SIDE; ++g) {
                int64_t line = 0, lineR = 0, lineG = 0, lineB = 0;
                double line2 = 0.0;
                for (int b = 1; b < SIDE; ++b) {
                    int k = idx(r, g, b);
                    line += wt[k];
                    lineR += mr[k];
                    lineG += mg[k];
                    lineB += mb[k];
                    line2 += m2[k];
                    area[b] += line;
                    areaR[b] += lineR;
                    areaG[b] += lineG;
                    areaB[b] += lineB;
                    area2[b] += line2;
                    int prev = idx(r - 1, g, b);
                    wt[k] = wt[prev] + area[b];
                    mr[k] = mr[prev] + areaR[b];
                    mg[k] = mg[prev] + areaG[b];
                    mb[k] = mb[prev] + areaB[b];
                    m2[k] = m2[prev] + area2[b];
                }
            }
        }
    }

    template <typename T>
    static inline T vol(const Box& c, const std::vector<T>& m) {
        return m[idx(c.r1, c.g1, c.b1)] - m[idx(c.r1, c.g1, c.b0)]
             - m[idx(c.r1, c.g0, c.b1)] + m[idx(c.r1, c.g0, c.b0)]
             - m[idx(c.r0, c.g1, c.b1)] + m[idx(c.r0, c.g1, c.b0)]
             + m[idx(c.r0, c.g0, c.b1)] - m[idx(c.r0, c.g0, c.b0)];
    }

    template <typename T>
    static inline T bottom(const Box& c, int dir, const std::vector<T>& m) {
        if (dir == 0) return -m[idx(c.r0, c.g1, c.b1)] + m[idx(c.r0, c.g1, c.b0)] + m[idx(c.r0, c.g0, c.b1)] - m[idx(c.r0, c.g0, c.b0)];
        if (dir == 1) return -m[idx(c.r1, c.g0, c.b1)] + m[idx(c.r1, c.g0, c.b0)] + m[idx(c.r0, c.g0, c.b1)] - m[idx(c.r0, c.g0, c.b0)];
        return               -m[idx(c.r1, c.g1, c.b0)] + m[idx(c.r1, c.g0, c.b0)] + m[idx(c.r0, c.g1, c.b0)] - m[idx(c.r0, c.g0, c.b0)];
    }

    template <typename T>
    static inline T top(const Box& c, int dir, int pos, const std::vector<T>& m) {
        if (dir == 0) return m[idx(pos, c.g1, c.b1)] - m[idx(pos, c.g1, c.b0)] - m[idx(pos, c.g0, c.b1)] + m[idx(pos, c.g0, c.b0)];
        if (dir == 1) return m[idx(c.r1, pos, c.b1)] - m[idx(c.r1, pos, c.b0)] - m[idx(c.r0, pos, c.b1)] + m[idx(c.r0, pos, c.b0)];
        return               m[idx(c.r1, c.g1, pos)] - m[idx(c.r1, c.g0, pos)] - m[idx(c.r0, c.g1, pos)] + m[idx(c.r0, c.g0, pos)];
    }

    double variance(const Box& c) const {
        int64_t w = vol(c, wt);
        if (w <= 0) return 0.0;
        double dr = double(vol(c, mr)), dg = double(vol(c, mg)), db = double(vol(c, mb));
        double xx = vol(c, m2);
        return xx - (dr * dr + dg * dg + db * db) / double(w);
    }

    double maximize(const Box& c, int dir, int first, int last, int& cut,
                    int64_t wholeR, int64_t wholeG, int64_t wholeB, int64_t wholeW) const {
        int64_t baseR = bottom(c, dir, mr), baseG = bottom(c, dir, mg), baseB = bottom(c, dir, mb), baseW = bottom(c, dir, wt);
        double maxGain = 0.0;
        cut = -1;
        for (int i = first; i < last; ++i) {
            int64_t halfW = baseW + top(c, dir, i, wt);
            if (halfW <= 0) continue;
            int64_t remW = wholeW - halfW;
            if (remW <= 0) continue;
            double halfR = double(baseR + top(c, dir, i, mr));
            double halfG = double(baseG + top(c, dir, i, mg));
            double halfB = double(baseB + top(c, dir, i, mb));
            double temp = (halfR * halfR + halfG * halfG + halfB * halfB) / double(halfW);
            double remR = double(wholeR) - halfR;
            double remG = double(wholeG) - halfG;
            double remB = double(wholeB) - halfB;
            temp += (remR * remR + remG * remG + remB * remB) / double(remW);
            if (temp > maxGain) {
                maxGain = temp;
                cut = i;
            }
        }
        return maxGain;
    }

    bool cutBox(Box& set1, Box& set2) const {
        int64_t wholeR = vol(set1, mr), wholeG = vol(set1, mg), wholeB = vol(set1, mb), wholeW = vol(set1, wt);
        int cutR = -1, cutG = -1, cutB = -1;
        double maxR = maximize(set1, 0, set1.r0 + 1, set1.r1, cutR, wholeR, wholeG, wholeB, wholeW);
        double maxG = maximize(set1, 1, set1.g0 + 1, set1.g1, cutG, wholeR, wholeG, wholeB, wholeW);
        double maxB = maximize(set1, 2, set1.b0 + 1, set1.b1, cutB, wholeR, wholeG, wholeB, wholeW);
        int dir = -1;
        if (maxR >= maxG && maxR >= maxB) {
            if (cutR < 0) return false;
            dir = 0;
        } else if (maxG >= maxR && maxG >= maxB) {
            if (cutG < 0) return false;
            dir = 1;
        } else {
            if (cutB < 0) return false;
            dir = 2;
        }
        set2 = set1;
        if (dir == 0) {
            set2.r0 = set1.r1 = cutR;
        } else if (dir == 1) {
            set2.g0 = set1.g1 = cutG;
        } else {
            set2.b0 = set1.b1 = cutB;
        }
        set1.vol = int64_t(set1.r1 - set1.r0) * (set1.g1 - set1.g0) * (set1.b1 - set1.b0);
        set2.vol = int64_t(set2.r1 - set2.r0) * (set2.g1 - set2.g0) * (set2.b1 - set2.b0);
        return true;
    }

    std::vector<Rgb> quantize(int maxColors) {
        buildMoments();
        maxColors = std::clamp(maxColors, 2, 256);
        std::vector<Box> cube(maxColors);
        std::vector<double> vv(maxColors, 0.0);
        cube[0] = {0, 32, 0, 32, 0, 32, 0};
        int next = 0, K = 1;
        for (int i = 1; i < maxColors; ++i) {
            if (cutBox(cube[next], cube[i])) {
                vv[next] = cube[next].vol > 1 ? variance(cube[next]) : 0.0;
                vv[i] = cube[i].vol > 1 ? variance(cube[i]) : 0.0;
                K = i + 1;
            } else {
                vv[next] = 0.0;
                i--;
            }
            next = 0;
            double best = vv[0];
            for (int k = 1; k < K; ++k) {
                if (vv[k] > best) {
                    best = vv[k];
                    next = k;
                }
            }
            if (best <= 0.0) break;
        }
        std::vector<Rgb> out;
        out.reserve(K);
        for (int k = 0; k < K; ++k) {
            int64_t w = vol(cube[k], wt);
            if (w <= 0) continue;
            uint8_t r = uint8_t(std::clamp(int(std::llround(double(vol(cube[k], mr)) / double(w))), 0, 255));
            uint8_t g = uint8_t(std::clamp(int(std::llround(double(vol(cube[k], mg)) / double(w))), 0, 255));
            uint8_t b = uint8_t(std::clamp(int(std::llround(double(vol(cube[k], mb)) / double(w))), 0, 255));
            out.push_back({r, g, b});
        }
        if (out.empty()) out.push_back({0, 0, 0});
        return out;
    }
};

inline uint8_t nearestColor(int r, int g, int b, const std::vector<Rgb>& palette) {
    uint32_t bestDist = UINT32_MAX;
    uint8_t bestIdx = 0;
    for (size_t i = 0; i < palette.size(); ++i) {
        int dr = r - int(palette[i].r);
        int dg = g - int(palette[i].g);
        int db = b - int(palette[i].b);
        uint32_t dist = uint32_t(2 * dr * dr + 4 * dg * dg + 3 * db * db);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = uint8_t(i);
            if (dist == 0) break;
        }
    }
    return bestIdx;
}

struct PaletteMapper {
    std::vector<Rgb> palette;
    std::vector<std::atomic<uint16_t>> lut18; // 64*64*64 = 262144; 0xFFFF=未计算, 0xFFFE=18位精确冲突
    bool exactMode = false;

    PaletteMapper() : lut18(262144) {
        for (auto& v : lut18) v.store(0xFFFFu, std::memory_order_relaxed);
    }

    void init(std::vector<Rgb> pal, bool exact) {
        palette = std::move(pal);
        exactMode = exact;
        if (exactMode) {
            for (size_t i = 0; i < palette.size(); ++i) {
                uint32_t k18 = ((uint32_t(palette[i].r) >> 2) << 12)
                             | ((uint32_t(palette[i].g) >> 2) << 6)
                             |  (uint32_t(palette[i].b) >> 2);
                uint16_t cur = lut18[k18].load(std::memory_order_relaxed);
                if (cur == 0xFFFFu) lut18[k18].store(uint16_t(i), std::memory_order_relaxed);
                else lut18[k18].store(0xFFFEu, std::memory_order_relaxed);
            }
        }
    }

    inline uint8_t mapPixel(int r, int g, int b) {
        uint32_t k18 = ((uint32_t(r) >> 2) << 12) | ((uint32_t(g) >> 2) << 6) | (uint32_t(b) >> 2);
        uint16_t v = lut18[k18].load(std::memory_order_relaxed);
        if (v < 0xFFFEu) return uint8_t(v);
        if (v == 0xFFFEu) return nearestColor(r, g, b, palette);
        uint8_t best = nearestColor((r & 0xFC) | 2, (g & 0xFC) | 2, (b & 0xFC) | 2, palette);
        lut18[k18].store(best, std::memory_order_relaxed);
        return best;
    }
};

// 高速 GIF89a LZW 压缩器（每线程独立字典，支持 16384 槽位 O(1) 代际清空）。
struct LzwEncoder {
    static constexpr uint32_t HASH_SIZE = 16384;
    static constexpr uint32_t HASH_MASK = HASH_SIZE - 1;
    std::vector<uint32_t> keys;
    std::vector<uint16_t> vals;
    std::vector<uint32_t> gens;
    uint32_t gen = 1;

    LzwEncoder() : keys(HASH_SIZE, 0), vals(HASH_SIZE, 0), gens(HASH_SIZE, 0) {}

    std::vector<uint8_t> compress(const uint8_t* pixels, size_t count, int minCodeSize) {
        std::vector<uint8_t> out;
        out.reserve(std::max<size_t>(64, count / 4));
        out.push_back(uint8_t(minCodeSize));

        uint8_t sub[256];
        int subLen = 0;
        uint32_t accum = 0;
        int bitCount = 0;

        auto flushSub = [&]() {
            if (subLen > 0) {
                out.push_back(uint8_t(subLen));
                out.insert(out.end(), sub, sub + subLen);
                subLen = 0;
            }
        };
        auto emitCode = [&](uint32_t code, int bits) {
            accum |= (code << bitCount);
            bitCount += bits;
            while (bitCount >= 8) {
                sub[subLen++] = uint8_t(accum & 0xFFu);
                if (subLen == 255) flushSub();
                accum >>= 8;
                bitCount -= 8;
            }
        };

        const uint32_t clearCode = 1u << minCodeSize;
        const uint32_t eoiCode = clearCode + 1u;
        uint32_t nextCode = eoiCode + 1u;
        int codeSize = minCodeSize + 1;

        auto resetTable = [&]() {
            ++gen;
            if (gen == 0) {
                std::fill(gens.begin(), gens.end(), 0u);
                gen = 1;
            }
            nextCode = eoiCode + 1u;
            codeSize = minCodeSize + 1;
        };

        resetTable();
        emitCode(clearCode, codeSize);
        if (count == 0) {
            emitCode(eoiCode, codeSize);
            if (bitCount > 0) {
                sub[subLen++] = uint8_t(accum & 0xFFu);
            }
            flushSub();
            out.push_back(0);
            return out;
        }

        uint32_t prefix = pixels[0];
        for (size_t i = 1; i < count; ++i) {
            uint8_t k = pixels[i];
            uint32_t key = (prefix << 8) | k;
            uint32_t slot = ((key * 2654435761u) >> 18) & HASH_MASK;
            bool found = false;
            while (gens[slot] == gen) {
                if (keys[slot] == key) {
                    prefix = vals[slot];
                    found = true;
                    break;
                }
                slot = (slot + 1u) & HASH_MASK;
            }
            if (!found) {
                emitCode(prefix, codeSize);
                if (nextCode < 4096u) {
                    if (nextCode == (1u << codeSize) && codeSize < 12) {
                        ++codeSize;
                    }
                    gens[slot] = gen;
                    keys[slot] = key;
                    vals[slot] = uint16_t(nextCode);
                    ++nextCode;
                } else {
                    emitCode(clearCode, codeSize);
                    resetTable();
                }
                prefix = k;
            }
        }

        emitCode(prefix, codeSize);
        emitCode(eoiCode, codeSize);
        if (bitCount > 0) {
            sub[subLen++] = uint8_t(accum & 0xFFu);
        }
        flushSub();
        out.push_back(0);
        return out;
    }
};

inline void packRgbaToRgb24(const BYTE* rgba, uint8_t* rgb, size_t pixelCount) {
    for (size_t i = 0; i < pixelCount; ++i) {
        rgb[i * 3 + 0] = rgba[i * 4 + 0];
        rgb[i * 3 + 1] = rgba[i * 4 + 1];
        rgb[i * 3 + 2] = rgba[i * 4 + 2];
    }
}

inline void writeGifHeader(std::ofstream& out, int w, int h, const std::vector<Rgb>& palette, int gctBits) {
    int gctSize = 1 << gctBits;
    uint8_t header[13] = {
        'G', 'I', 'F', '8', '9', 'a',
        uint8_t(w & 0xFF), uint8_t((w >> 8) & 0xFF),
        uint8_t(h & 0xFF), uint8_t((h >> 8) & 0xFF),
        uint8_t(0xF0 | (gctBits - 1)),
        0, 0
    };
    out.write(reinterpret_cast<const char*>(header), sizeof(header));

    std::vector<uint8_t> gct(size_t(gctSize) * 3, 0);
    for (size_t i = 0; i < palette.size() && i < size_t(gctSize); ++i) {
        gct[i * 3 + 0] = palette[i].r;
        gct[i * 3 + 1] = palette[i].g;
        gct[i * 3 + 2] = palette[i].b;
    }
    out.write(reinterpret_cast<const char*>(gct.data()), gct.size());

    // Netscape 2.0 无限循环扩展。
    static constexpr uint8_t netscape[19] = {
        0x21, 0xFF, 0x0B,
        'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
        0x03, 0x01, 0x00, 0x00, 0x00
    };
    out.write(reinterpret_cast<const char*>(netscape), sizeof(netscape));
}

inline void writeGifFrame(std::ofstream& out, int w, int h, unsigned totalDelayCs, const std::vector<uint8_t>& lzwData) {
    // 若单帧总延时超过 65535 厘秒，拆分为多帧避免 16 位溢出。
    unsigned remaining = std::max(1u, totalDelayCs);
    while (remaining > 0) {
        unsigned d = std::min(remaining, 65535u);
        uint8_t gce[8] = {
            0x21, 0xF9, 0x04,
            0x04, // Disposal = 1 (保留当前全画幅)，TransparencyFlag = 0 (无透明像素，杜绝 QQ 拖影)
            uint8_t(d & 0xFFu), uint8_t((d >> 8) & 0xFFu),
            0x00, 0x00
        };
        uint8_t desc[10] = {
            0x2C,
            0x00, 0x00, 0x00, 0x00,
            uint8_t(w & 0xFF), uint8_t((w >> 8) & 0xFF),
            uint8_t(h & 0xFF), uint8_t((h >> 8) & 0xFF),
            0x00 // LCTF = 0 (仅用全局调色板)
        };
        out.write(reinterpret_cast<const char*>(gce), sizeof(gce));
        out.write(reinterpret_cast<const char*>(desc), sizeof(desc));
        out.write(reinterpret_cast<const char*>(lzwData.data()), lzwData.size());
        remaining -= d;
    }
}

inline void mapFrameRowRange(
    const uint8_t* currRgb, uint8_t* stabRgb, uint8_t* prevIdx, uint8_t* outIdx,
    int outW, int y0, int y1, PaletteMapper& mapper, int ditherAmp, int stabTol, bool firstFrame
) {
    static constexpr int BAYER4[4][4] = {
        {-15,  1, -11,  5},
        {  9, -7,  13, -3},
        { -9,  7, -13,  3},
        { 15, -1,  11, -5}
    };
    for (int y = y0; y < y1; ++y) {
        size_t rowBase = size_t(y) * outW;
        const int* bayerRow = BAYER4[y & 3];
        for (int x = 0; x < outW; ++x) {
            size_t p = rowBase + size_t(x);
            size_t o = p * 3;
            uint8_t r = currRgb[o + 0];
            uint8_t g = currRgb[o + 1];
            uint8_t b = currRgb[o + 2];
            if (!firstFrame && stabTol > 0) {
                int dr = int(r) - int(stabRgb[o + 0]);
                int dg = int(g) - int(stabRgb[o + 1]);
                int db = int(b) - int(stabRgb[o + 2]);
                if (dr >= -stabTol && dr <= stabTol &&
                    dg >= -stabTol && dg <= stabTol &&
                    db >= -stabTol && db <= stabTol) {
                    outIdx[p] = prevIdx[p];
                    continue;
                }
            }
            int rr = r, gg = g, bb = b;
            if (ditherAmp > 0) {
                int d = (bayerRow[x & 3] * ditherAmp) >> 4;
                rr = std::clamp(rr + d, 0, 255);
                gg = std::clamp(gg + d, 0, 255);
                bb = std::clamp(bb + d, 0, 255);
            }
            uint8_t idx = mapper.mapPixel(rr, gg, bb);
            outIdx[p] = idx;
            if (stabTol > 0) {
                prevIdx[p] = idx;
                stabRgb[o + 0] = r;
                stabRgb[o + 1] = g;
                stabRgb[o + 2] = b;
            }
        }
    }
}

} // namespace native_gif_detail

// qualityMode: 0=小体积(默认最长边800,96色), 1=均衡(默认最长边1280,128色), 2=原尺寸高保真(不缩放,256色,无损容差)
inline bool encodeNativeGif(
    const std::wstring& tool, const std::wstring& video, const std::wstring& gif, const std::wstring& log,
    int fps, int width, int height, int maxWidth = 0, int qualityMode = 2
) {
    using namespace native_gif_detail;
    ULONGLONG began = GetTickCount64(), readMs = 0, quantMs = 0, encodeMs = 0;

    NativeVideo decoder(video);
    if (decoder.error < 0) return false;
    if (width <= 0 || height <= 0) {
        width = decoder.sourceWidth();
        height = decoder.sourceHeight();
    }
    if (width < 2 || height < 2) return false;

    int effMaxWidth = maxWidth;
    int maxColors = 256, dedupeTol = 0, stabTol = 0, ditherAmp = 0;
    if (qualityMode == 0) {
        if (effMaxWidth <= 0) effMaxWidth = 800;
        maxColors = 96; dedupeTol = 4; stabTol = 5; ditherAmp = 8;
    } else if (qualityMode == 1) {
        if (effMaxWidth <= 0) effMaxWidth = 1280;
        maxColors = 128; dedupeTol = 3; stabTol = 4; ditherAmp = 6;
    }

    int outW = width, outH = height;
    if (effMaxWidth > 0 && std::max(width, height) > effMaxWidth) {
        if (width >= height) {
            outW = effMaxWidth & ~1;
            outH = std::max(2, int(std::llround(double(height) * outW / width)) & ~1);
        } else {
            outH = effMaxWidth & ~1;
            outW = std::max(2, int(std::llround(double(width) * outH / height)) & ~1);
        }
    }
    outW = std::clamp(outW, 2, 65535);
    outH = std::clamp(outH, 2, 65535);

    const size_t pixelCount = size_t(outW) * size_t(outH);
    const size_t rgbaBytes = pixelCount * 4;
    const size_t rgbBytes = pixelCount * 3;
    const size_t maxRamCacheBytes = 256ull * 1024 * 1024;

    std::vector<BYTE> current(rgbaBytes), previous(rgbaBytes);
    std::vector<std::vector<uint8_t>> cachedRgb;
    size_t cachedBytes = 0;
    bool streamingPass2 = false;

    struct KeptMeta {
        unsigned startSample;
        unsigned endSample;
        unsigned delayCs;
    };
    std::vector<KeptMeta> kept;

    std::unordered_set<uint32_t> uniqueColors;
    bool tooManyUnique = false;
    std::vector<uint32_t> samples;
    samples.reserve(256000);
    constexpr size_t MAX_SAMPLES = 240000;

    auto addFrameSamples = [&](const BYTE* curr, const BYTE* prev, bool hasPrev) {
        const uint32_t* pCurr = reinterpret_cast<const uint32_t*>(curr);
        const uint32_t* pPrev = reinterpret_cast<const uint32_t*>(prev);
        if (!tooManyUnique) {
            for (size_t i = 0; i < pixelCount; ++i) {
                uint32_t c = pCurr[i] & 0x00FFFFFFu;
                uniqueColors.insert(c);
                if (uniqueColors.size() > size_t(maxColors)) {
                    tooManyUnique = true;
                    break;
                }
            }
        }
        size_t uniformBudget = hasPrev ? 4096 : 16384;
        size_t step = std::max<size_t>(1, pixelCount / uniformBudget);
        for (size_t i = 0; i < pixelCount; i += step) {
            samples.push_back(pCurr[i]);
        }
        if (hasPrev) {
            size_t changedBudget = 4096;
            size_t cStep = std::max<size_t>(1, pixelCount / 16384);
            size_t added = 0;
            for (size_t i = 0; i < pixelCount && added < changedBudget; i += cStep) {
                if ((pCurr[i] & 0x00FFFFFFu) != (pPrev[i] & 0x00FFFFFFu)) {
                    samples.push_back(pCurr[i]);
                    ++added;
                }
            }
        }
        if (samples.size() > MAX_SAMPLES) {
            size_t write = 0;
            for (size_t read = 0; read < samples.size(); read += 2) {
                samples[write++] = samples[read];
            }
            samples.resize(write);
        }
    };

    unsigned readFrames = 0;
    unsigned pending = 0;
    bool hasPending = false;
    bool decodeOk = true;

    while (decodeOk) {
        ULONGLONG step = GetTickCount64();
        if (!decoder.next()) break;
        unsigned index = decoder.sampleIndex(fps);
        if (readFrames && index < readFrames) continue;
        if (!decoder.rgb(current, outW, outH)) {
            decodeOk = false;
            break;
        }
        readMs += GetTickCount64() - step;

        if (!hasPending) {
            previous.swap(current);
            pending = index;
            hasPending = true;
            addFrameSamples(previous.data(), nullptr, false);
            if (!streamingPass2 && cachedBytes + rgbBytes <= maxRamCacheBytes) {
                std::vector<uint8_t> rgb(rgbBytes);
                packRgbaToRgb24(previous.data(), rgb.data(), pixelCount);
                cachedRgb.push_back(std::move(rgb));
                cachedBytes += rgbBytes;
            } else {
                streamingPass2 = true;
                std::vector<std::vector<uint8_t>>().swap(cachedRgb);
                cachedBytes = 0;
            }
        } else if (!framesSimilar(current.data(), previous.data(), rgbaBytes, dedupeTol)) {
            unsigned dur = unsigned((uint64_t(index) * 100 + fps / 2) / fps - (uint64_t(pending) * 100 + fps / 2) / fps);
            kept.push_back({pending, index, std::max(1u, dur)});
            addFrameSamples(current.data(), previous.data(), true);
            previous.swap(current);
            pending = index;
            if (!streamingPass2 && cachedBytes + rgbBytes <= maxRamCacheBytes) {
                std::vector<uint8_t> rgb(rgbBytes);
                packRgbaToRgb24(previous.data(), rgb.data(), pixelCount);
                cachedRgb.push_back(std::move(rgb));
                cachedBytes += rgbBytes;
            } else {
                streamingPass2 = true;
                std::vector<std::vector<uint8_t>>().swap(cachedRgb);
                cachedBytes = 0;
            }
        }
        readFrames = index + 1;
    }

    if (!decodeOk || decoder.error < 0 || !hasPending || readFrames == 0) {
        return false;
    }
    unsigned lastEndIdx = std::max(readFrames, decoder.endIndex(fps));
    unsigned lastDur = unsigned((uint64_t(lastEndIdx) * 100 + fps / 2) / fps - (uint64_t(pending) * 100 + fps / 2) / fps);
    kept.push_back({pending, lastEndIdx, std::max(1u, lastDur)});

    // 全局调色板生成（全程仅执行一次）。
    ULONGLONG qStart = GetTickCount64();
    std::vector<Rgb> palette;
    bool exactPalette = !tooManyUnique && !uniqueColors.empty() && uniqueColors.size() <= size_t(maxColors);

    if (exactPalette) {
        palette.reserve(uniqueColors.size());
        for (uint32_t c : uniqueColors) {
            palette.push_back({uint8_t(c & 0xFFu), uint8_t((c >> 8) & 0xFFu), uint8_t((c >> 16) & 0xFFu)});
        }
    } else {
        bool usedGifski = false;
        if (maxColors >= 255 && !samples.empty()) {
            using Quantize = int(*)(const void*, size_t, BYTE*, UINT*);
            size_t slash = tool.find_last_of(L"\\/");
            std::wstring dllPath = (slash == std::wstring::npos ? L"" : tool.substr(0, slash) + L"\\") + L"gifski.dll";
            HMODULE library = LoadLibraryW(dllPath.c_str());
            if (library) {
                auto quantize = reinterpret_cast<Quantize>(GetProcAddress(library, "shuntie_quantize_rgba"));
                if (quantize) {
                    std::vector<BYTE> dummy(samples.size());
                    UINT cols[256]{};
                    int cnt = quantize(samples.data(), samples.size(), dummy.data(), cols);
                    if (cnt >= 1 && cnt <= 255) {
                        palette.reserve(cnt);
                        for (int i = 0; i < cnt; ++i) {
                            palette.push_back({uint8_t((cols[i] >> 16) & 0xFFu), uint8_t((cols[i] >> 8) & 0xFFu), uint8_t(cols[i] & 0xFFu)});
                        }
                        usedGifski = true;
                    }
                }
                FreeLibrary(library);
            }
        }
        if (!usedGifski) {
            WuQuantizer wu;
            wu.addSamples(samples.data(), samples.size());
            palette = wu.quantize(maxColors);
        }
    }

    int gctBits = 2;
    while ((1 << gctBits) < int(palette.size()) && gctBits < 8) ++gctBits;
    const int minCodeSize = gctBits;

    PaletteMapper mapper;
    mapper.init(palette, exactPalette);
    quantMs = GetTickCount64() - qStart;

    // 打开输出 GIF 文件并写入头信息与全局调色板。
    ULONGLONG eStart = GetTickCount64();
    std::ofstream gifOut(gif.c_str(), std::ios::binary);
    if (!gifOut) return false;
    writeGifHeader(gifOut, outW, outH, palette, gctBits);

    const unsigned numThreads = std::clamp(std::thread::hardware_concurrency(), 1u, 16u);
    const size_t totalFrames = kept.size();

    if (!streamingPass2 && cachedRgb.size() == totalFrames) {
        // 全内存模式：先并行切片做色彩映射与时域稳定，再并行进行 LZW 压缩。
        std::vector<std::vector<uint8_t>> indexedFrames(totalFrames, std::vector<uint8_t>(pixelCount));
        if (stabTol > 0) {
            unsigned sliceThreads = std::min<unsigned>(numThreads, std::max<unsigned>(1, unsigned(outH / 16)));
            std::vector<std::thread> workers;
            for (unsigned t = 0; t < sliceThreads; ++t) {
                int y0 = (t == 0) ? 0 : (((outH * int(t)) / int(sliceThreads)) & ~3);
                int y1 = (t + 1 == sliceThreads) ? outH : (((outH * int(t + 1)) / int(sliceThreads)) & ~3);
                if (y1 <= y0) continue;
                workers.emplace_back([&, y0, y1]() {
                    std::vector<uint8_t> sliceStab(size_t(y1 - y0) * outW * 3, 0);
                    std::vector<uint8_t> slicePrevIdx(size_t(y1 - y0) * outW, 0);
                    size_t sliceOffsetBytes = size_t(y0) * outW * 3;
                    size_t sliceOffsetPx = size_t(y0) * outW;
                    for (size_t f = 0; f < totalFrames; ++f) {
                        // 复用 mapFrameRowRange 逻辑在切片缓冲上运行。
                        const uint8_t* srcSlice = cachedRgb[f].data() + sliceOffsetBytes;
                        uint8_t* dstSlice = indexedFrames[f].data() + sliceOffsetPx;
                        mapFrameRowRange(
                            srcSlice, sliceStab.data(), slicePrevIdx.data(), dstSlice,
                            outW, 0, y1 - y0, mapper, ditherAmp, stabTol, f == 0
                        );
                    }
                });
            }
            for (auto& w : workers) w.join();
        } else {
            // stabTol == 0：帧间完全独立，多线程并行映射所有帧。
            std::atomic<size_t> nextFrame{0};
            unsigned mapThreads = std::min<unsigned>(numThreads, unsigned(totalFrames));
            std::vector<std::thread> workers;
            for (unsigned t = 0; t < mapThreads; ++t) {
                workers.emplace_back([&]() {
                    for (;;) {
                        size_t f = nextFrame.fetch_add(1, std::memory_order_relaxed);
                        if (f >= totalFrames) break;
                        mapFrameRowRange(
                            cachedRgb[f].data(), nullptr, nullptr, indexedFrames[f].data(),
                            outW, 0, outH, mapper, ditherAmp, 0, true
                        );
                    }
                });
            }
            for (auto& w : workers) w.join();
        }
        std::vector<std::vector<uint8_t>>().swap(cachedRgb);

        std::vector<std::vector<uint8_t>> lzwFrames(totalFrames);
        std::atomic<size_t> nextLzw{0};
        unsigned lzwThreads = std::min<unsigned>(numThreads, unsigned(totalFrames));
        std::vector<std::thread> lzwWorkers;
        for (unsigned t = 0; t < lzwThreads; ++t) {
            lzwWorkers.emplace_back([&]() {
                LzwEncoder lzw;
                for (;;) {
                    size_t f = nextLzw.fetch_add(1, std::memory_order_relaxed);
                    if (f >= totalFrames) break;
                    lzwFrames[f] = lzw.compress(indexedFrames[f].data(), pixelCount, minCodeSize);
                }
            });
        }
        for (auto& w : lzwWorkers) w.join();

        for (size_t f = 0; f < totalFrames; ++f) {
            writeGifFrame(gifOut, outW, outH, kept[f].delayCs, lzwFrames[f]);
        }
    } else {
        // 大视频流式模式：分批重新解码并在有界内存中并行 LZW 写出。
        NativeVideo decoder2(video);
        if (decoder2.error < 0) return false;
        std::vector<uint8_t> stabRgb(stabTol > 0 ? rgbBytes : 0, 0);
        std::vector<uint8_t> prevIdx(stabTol > 0 ? pixelCount : 0, 0);

        size_t batchCap = std::max<size_t>(1, std::min<size_t>(numThreads * 2, (128ull * 1024 * 1024) / std::max<size_t>(1, pixelCount + rgbBytes)));
        std::vector<std::vector<uint8_t>> batchIndexed;
        std::vector<unsigned> batchDelays;
        batchIndexed.reserve(batchCap);
        batchDelays.reserve(batchCap);

        auto flushBatch = [&]() {
            if (batchIndexed.empty()) return;
            std::vector<std::vector<uint8_t>> batchLzw(batchIndexed.size());
            std::atomic<size_t> nextItem{0};
            unsigned bt = std::min<unsigned>(numThreads, unsigned(batchIndexed.size()));
            std::vector<std::thread> workers;
            for (unsigned t = 0; t < bt; ++t) {
                workers.emplace_back([&]() {
                    LzwEncoder lzw;
                    for (;;) {
                        size_t idx = nextItem.fetch_add(1, std::memory_order_relaxed);
                        if (idx >= batchIndexed.size()) break;
                        batchLzw[idx] = lzw.compress(batchIndexed[idx].data(), pixelCount, minCodeSize);
                    }
                });
            }
            for (auto& w : workers) w.join();
            for (size_t i = 0; i < batchLzw.size(); ++i) {
                writeGifFrame(gifOut, outW, outH, batchDelays[i], batchLzw[i]);
            }
            batchIndexed.clear();
            batchDelays.clear();
        };

        std::vector<uint8_t> tmpRgb(rgbBytes);
        size_t targetIdx = 0;
        unsigned read2 = 0;
        while (targetIdx < totalFrames && decoder2.next()) {
            unsigned idx = decoder2.sampleIndex(fps);
            if (read2 && idx < read2) continue;
            read2 = idx + 1;
            if (idx < kept[targetIdx].startSample) continue;
            if (!decoder2.rgb(current, outW, outH)) return false;
            packRgbaToRgb24(current.data(), tmpRgb.data(), pixelCount);
            std::vector<uint8_t> mapped(pixelCount);
            mapFrameRowRange(
                tmpRgb.data(),
                stabTol > 0 ? stabRgb.data() : nullptr,
                stabTol > 0 ? prevIdx.data() : nullptr,
                mapped.data(),
                outW, 0, outH, mapper, ditherAmp, stabTol, targetIdx == 0
            );
            batchIndexed.push_back(std::move(mapped));
            batchDelays.push_back(kept[targetIdx].delayCs);
            ++targetIdx;
            if (batchIndexed.size() >= batchCap) flushBatch();
        }
        flushBatch();
        if (targetIdx != totalFrames) return false;
    }

    uint8_t trailer = 0x3B;
    gifOut.write(reinterpret_cast<const char*>(&trailer), 1);
    gifOut.flush();
    bool writeOk = gifOut.good();
    uint64_t fileBytes = uint64_t(gifOut.tellp());
    gifOut.close();
    encodeMs = GetTickCount64() - eStart;

    std::ofstream stats(log.c_str(), std::ios::app);
    stats << "\nGIF fast width=" << outW << " height=" << outH
          << " src_w=" << width << " src_h=" << height
          << " colors=" << palette.size() << " mode=" << qualityMode
          << " frames_read=" << readFrames << " frames_written=" << totalFrames
          << " file_bytes=" << fileBytes
          << " elapsed_ms=" << (GetTickCount64() - began)
          << " read_ms=" << readMs << " quant_ms=" << quantMs << " encode_ms=" << encodeMs
          << " streaming=" << (streamingPass2 ? 1 : 0) << "\n";

    return writeOk && totalFrames > 0;
}
