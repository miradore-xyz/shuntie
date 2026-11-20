// SPDX-License-Identifier: AGPL-3.0-or-later
// 高速、低体积、QQ/微信全平台兼容的 GIF89a 编码器：
// 单一全局调色板 + 全画幅不透明帧（无局部调色板、无透明增量帧、无残影），支持有损 LZW 与自适应体积控制。
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

// SSE2 帧间相似判定：tol == 0 为逐字节精确相等；tol > 0 忽略 H.264 编码轻微抖动，但任一高对比像素变化（如鼠标/波纹）立即触发新帧。
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
    int jitterPixels = 0;

    for (; n + 16 <= bytes; n += 16) {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + n));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + n));
        __m128i diff = _mm_or_si128(_mm_subs_epu8(va, vb), _mm_subs_epu8(vb, va));
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_subs_epu8(diff, tv), zero)) == 0xFFFF) {
            continue;
        }
        for (int p = 0; p < 4; ++p) {
            size_t o = n + size_t(p) * 4;
            int d0 = std::abs(int(a[o + 0]) - int(b[o + 0]));
            int d1 = std::abs(int(a[o + 1]) - int(b[o + 1]));
            int d2 = std::abs(int(a[o + 2]) - int(b[o + 2]));
            int md = std::max({d0, d1, d2});
            if (md > 24) return false;
            if (md > tol) {
                ++jitterPixels;
                if (jitterPixels >= 24) return false;
            }
        }
    }
    for (; n + 4 <= bytes; n += 4) {
        int d0 = std::abs(int(a[n + 0]) - int(b[n + 0]));
        int d1 = std::abs(int(a[n + 1]) - int(b[n + 1]));
        int d2 = std::abs(int(a[n + 2]) - int(b[n + 2]));
        int md = std::max({d0, d1, d2});
        if (md > 24) return false;
        if (md > tol && ++jitterPixels >= 24) return false;
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
    std::vector<std::atomic<uint16_t>> lut18;
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

// 邻近调色板缓存表：为有损 LZW 提供视觉最接近的候选替代色，显著拉长字典匹配链。
struct NearPaletteTable {
    static constexpr int MAX_NEAR = 6;
    uint8_t nearIdx[256][MAX_NEAR]{};
    uint16_t nearDist[256][MAX_NEAR]{};
    uint8_t nearCount[256]{};

    void build(const std::vector<Rgb>& pal, int maxSubDistSq) {
        std::memset(nearCount, 0, sizeof(nearCount));
        if (maxSubDistSq <= 0 || pal.size() <= 1) return;
        for (size_t i = 0; i < pal.size(); ++i) {
            std::vector<std::pair<int, uint8_t>> cand;
            cand.reserve(pal.size());
            for (size_t j = 0; j < pal.size(); ++j) {
                if (i == j) continue;
                int dr = int(pal[i].r) - int(pal[j].r);
                int dg = int(pal[i].g) - int(pal[j].g);
                int db = int(pal[i].b) - int(pal[j].b);
                int d2 = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
                if (d2 <= maxSubDistSq) cand.push_back({d2, uint8_t(j)});
            }
            std::sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            uint8_t n = uint8_t(std::min<size_t>(cand.size(), MAX_NEAR));
            nearCount[i] = n;
            for (uint8_t k = 0; k < n; ++k) {
                nearDist[i][k] = uint16_t(cand[k].first);
                nearIdx[i][k] = cand[k].second;
            }
        }
    }
};

// 高速 GIF89a LZW 压缩器（支持无损及可控有损字典匹配，输出 100% 标准兼容的 GIF89a 码流）。
struct LzwEncoder {
    static constexpr uint32_t HASH_SIZE = 16384;
    static constexpr uint32_t HASH_MASK = HASH_SIZE - 1;
    std::vector<uint32_t> keys;
    std::vector<uint16_t> vals;
    std::vector<uint32_t> gens;
    uint32_t gen = 1;

    LzwEncoder() : keys(HASH_SIZE, 0), vals(HASH_SIZE, 0), gens(HASH_SIZE, 0) {}

    std::vector<uint8_t> compress(
        const uint8_t* pixels, size_t count, int minCodeSize,
        int lossyLevel = 0, const NearPaletteTable* nearTable = nullptr
    ) {
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
            if (bitCount > 0) sub[subLen++] = uint8_t(accum & 0xFFu);
            flushSub();
            out.push_back(0);
            return out;
        }

        const int maxRunErr = (lossyLevel == 1) ? 900 : (lossyLevel == 2) ? 2200 : (lossyLevel >= 3) ? 4800 : 0;
        int runErr = 0;

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
            if (!found && lossyLevel > 0 && nearTable && runErr < maxRunErr) {
                uint8_t nc = nearTable->nearCount[k];
                for (uint8_t j = 0; j < nc; ++j) {
                    int d2 = int(nearTable->nearDist[k][j]);
                    if (runErr + d2 > maxRunErr) break;
                    uint8_t alt = nearTable->nearIdx[k][j];
                    uint32_t aKey = (prefix << 8) | alt;
                    uint32_t aSlot = ((aKey * 2654435761u) >> 18) & HASH_MASK;
                    while (gens[aSlot] == gen) {
                        if (keys[aSlot] == aKey) {
                            prefix = vals[aSlot];
                            runErr += d2;
                            found = true;
                            break;
                        }
                        aSlot = (aSlot + 1u) & HASH_MASK;
                    }
                    if (found) break;
                }
            }
            if (!found) {
                emitCode(prefix, codeSize);
                runErr = 0;
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
        if (bitCount > 0) sub[subLen++] = uint8_t(accum & 0xFFu);
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

    static constexpr uint8_t netscape[19] = {
        0x21, 0xFF, 0x0B,
        'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
        0x03, 0x01, 0x00, 0x00, 0x00
    };
    out.write(reinterpret_cast<const char*>(netscape), sizeof(netscape));
}

inline void writeGifFrame(std::ofstream& out, int w, int h, unsigned totalDelayCs, const std::vector<uint8_t>& lzwData) {
    unsigned remaining = std::max(1u, totalDelayCs);
    while (remaining > 0) {
        unsigned d = std::min(remaining, 65535u);
        uint8_t gce[8] = {
            0x21, 0xF9, 0x04,
            0x04, // Disposal = 1 (保留全画幅), TransparencyFlag = 0 (彻底杜绝 QQ 群聊拖影)
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
    int outW, int y0, int y1, PaletteMapper& mapper, int snapDistSq, int stabTol, bool firstFrame
) {
    for (int y = y0; y < y1; ++y) {
        size_t rowBase = size_t(y) * outW;
        uint8_t rowPrevIdx = 0;
        bool hasRowPrev = false;
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
                    uint8_t idx = prevIdx[p];
                    outIdx[p] = idx;
                    rowPrevIdx = idx;
                    hasRowPrev = true;
                    continue;
                }
            }
            uint8_t best = mapper.mapPixel(r, g, b);
            uint8_t chosen = best;
            if (snapDistSq > 0 && hasRowPrev && rowPrevIdx != best) {
                const Rgb& pr = mapper.palette[rowPrevIdx];
                int drp = int(r) - int(pr.r), dgp = int(g) - int(pr.g), dbp = int(b) - int(pr.b);
                int dPrev = 2 * drp * drp + 4 * dgp * dgp + 3 * dbp * dbp;
                if (dPrev <= snapDistSq) {
                    const Rgb& br = mapper.palette[best];
                    int drb = int(r) - int(br.r), dgb = int(g) - int(br.g), dbb = int(b) - int(br.b);
                    int dBest = 2 * drb * drb + 4 * dgb * dgb + 3 * dbb * dbb;
                    if (dPrev <= dBest + snapDistSq) {
                        chosen = rowPrevIdx;
                    }
                }
            }
            outIdx[p] = chosen;
            rowPrevIdx = chosen;
            hasRowPrev = true;
            if (stabTol > 0) {
                prevIdx[p] = chosen;
                stabRgb[o + 0] = r;
                stabRgb[o + 1] = g;
                stabRgb[o + 2] = b;
            }
        }
    }
}

struct GifParams {
    int effFps;
    int effMaxWidth;
    int maxColors;
    int dedupeTol;
    int stabTol;
    int lossyLevel;
    uint64_t targetMaxBytes;
    int qualityMode;
};

struct AttemptOutcome {
    bool ok = false;
    bool needsRetry = false;
    int outW = 0;
    int outH = 0;
    unsigned readFrames = 0;
    size_t framesWritten = 0;
    size_t colors = 0;
    uint64_t fileBytes = 0;
    uint64_t estBytes = 0;
    ULONGLONG readMs = 0;
    ULONGLONG quantMs = 0;
    ULONGLONG encodeMs = 0;
    bool streaming = false;
};

inline AttemptOutcome encodeNativeGifAttempt(
    const std::wstring& tool, const std::wstring& video, const std::wstring& gif,
    int srcW, int srcH, const GifParams& cfg, bool allowProbeAbort
) {
    AttemptOutcome res;
    NativeVideo decoder(video);
    if (decoder.error < 0) return res;

    int outW = srcW, outH = srcH;
    if (cfg.effMaxWidth > 0 && std::max(srcW, srcH) > cfg.effMaxWidth) {
        if (srcW >= srcH) {
            outW = std::max(2, cfg.effMaxWidth & ~1);
            outH = std::max(2, int(std::llround(double(srcH) * outW / srcW)) & ~1);
        } else {
            outH = std::max(2, cfg.effMaxWidth & ~1);
            outW = std::max(2, int(std::llround(double(srcW) * outH / srcH)) & ~1);
        }
    }
    outW = std::clamp(outW, 2, 65535);
    outH = std::clamp(outH, 2, 65535);
    res.outW = outW;
    res.outH = outH;

    const int fps = std::clamp(cfg.effFps, 1, 50);
    const size_t pixelCount = size_t(outW) * size_t(outH);
    const size_t rgbaBytes = pixelCount * 4;
    const size_t rgbBytes = pixelCount * 3;
    const size_t maxRamCacheBytes = 512ull * 1024 * 1024;

    std::vector<BYTE> current(rgbaBytes), previous(rgbaBytes);
    std::vector<std::vector<uint8_t>> cachedRgb;
    std::vector<std::vector<uint8_t>> probeFallbackRgb; // 流式超限时的 6 帧探针缓存
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
                if (uniqueColors.size() > size_t(cfg.maxColors)) {
                    tooManyUnique = true;
                    break;
                }
            }
        }
        size_t uniformBudget = hasPrev ? 4096 : 16384;
        size_t step = std::max<size_t>(1, pixelCount / uniformBudget);
        for (size_t i = 0; i < pixelCount; i += step) samples.push_back(pCurr[i]);
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
            for (size_t read = 0; read < samples.size(); read += 2) samples[write++] = samples[read];
            samples.resize(write);
        }
    };

    auto storeKeptFrame = [&](const BYTE* rgba) {
        if (!streamingPass2 && cachedBytes + rgbBytes <= maxRamCacheBytes) {
            std::vector<uint8_t> rgb(rgbBytes);
            packRgbaToRgb24(rgba, rgb.data(), pixelCount);
            cachedRgb.push_back(std::move(rgb));
            cachedBytes += rgbBytes;
        } else {
            if (!streamingPass2) {
                streamingPass2 = true;
                for (size_t i = 0; i < 6 && i < cachedRgb.size(); ++i) {
                    size_t pick = (i * cachedRgb.size()) / std::min<size_t>(6, cachedRgb.size());
                    probeFallbackRgb.push_back(std::move(cachedRgb[pick]));
                }
                std::vector<std::vector<uint8_t>>().swap(cachedRgb);
                cachedBytes = 0;
            }
            if (probeFallbackRgb.size() < 6) {
                std::vector<uint8_t> rgb(rgbBytes);
                packRgbaToRgb24(rgba, rgb.data(), pixelCount);
                probeFallbackRgb.push_back(std::move(rgb));
            }
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
        res.readMs += GetTickCount64() - step;

        if (!hasPending) {
            previous.swap(current);
            pending = index;
            hasPending = true;
            addFrameSamples(previous.data(), nullptr, false);
            storeKeptFrame(previous.data());
        } else if (!framesSimilar(current.data(), previous.data(), rgbaBytes, cfg.dedupeTol)) {
            unsigned dur = unsigned((uint64_t(index) * 100 + fps / 2) / fps - (uint64_t(pending) * 100 + fps / 2) / fps);
            kept.push_back({pending, index, std::max(1u, dur)});
            addFrameSamples(current.data(), previous.data(), true);
            previous.swap(current);
            pending = index;
            storeKeptFrame(previous.data());
        }
        readFrames = index + 1;
    }

    if (!decodeOk || decoder.error < 0 || !hasPending || readFrames == 0) {
        return res;
    }
    unsigned lastEndIdx = std::max(readFrames, decoder.endIndex(fps));
    unsigned lastDur = unsigned((uint64_t(lastEndIdx) * 100 + fps / 2) / fps - (uint64_t(pending) * 100 + fps / 2) / fps);
    kept.push_back({pending, lastEndIdx, std::max(1u, lastDur)});

    res.readFrames = readFrames;
    res.framesWritten = kept.size();
    res.streaming = streamingPass2;

    // 全局调色板生成（仅执行一次）。
    ULONGLONG qStart = GetTickCount64();
    std::vector<Rgb> palette;
    bool exactPalette = !tooManyUnique && !uniqueColors.empty() && uniqueColors.size() <= size_t(cfg.maxColors);

    if (exactPalette) {
        palette.reserve(uniqueColors.size());
        for (uint32_t c : uniqueColors) {
            palette.push_back({uint8_t(c & 0xFFu), uint8_t((c >> 8) & 0xFFu), uint8_t((c >> 16) & 0xFFu)});
        }
    } else {
        bool usedGifski = false;
        if (cfg.maxColors >= 255 && !samples.empty()) {
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
            palette = wu.quantize(cfg.maxColors);
        }
    }

    int gctBits = 2;
    while ((1 << gctBits) < int(palette.size()) && gctBits < 8) ++gctBits;
    const int minCodeSize = gctBits;
    res.colors = palette.size();

    PaletteMapper mapper;
    mapper.init(palette, exactPalette);

    const int snapDistSq = (cfg.lossyLevel == 1) ? 120 : (cfg.lossyLevel == 2) ? 260 : (cfg.lossyLevel >= 3) ? 480 : 0;
    const int maxSubDistSq = (cfg.lossyLevel == 1) ? 450 : (cfg.lossyLevel == 2) ? 1100 : (cfg.lossyLevel >= 3) ? 2400 : 0;
    NearPaletteTable nearTable;
    if (cfg.lossyLevel > 0 && !exactPalette) {
        nearTable.build(mapper.palette, maxSubDistSq);
    }
    res.quantMs = GetTickCount64() - qStart;

    const size_t totalFrames = kept.size();

    // 6 帧快速体积探针：在完整编码前预估总体积，超限则提前重采样而无需浪费编码时间。
    if (allowProbeAbort && cfg.targetMaxBytes > 0 && totalFrames > 6) {
        std::vector<const uint8_t*> probePtrs;
        if (!streamingPass2 && !cachedRgb.empty()) {
            size_t cnt = std::min<size_t>(6, cachedRgb.size());
            for (size_t i = 0; i < cnt; ++i) {
                size_t pick = (i * (cachedRgb.size() - 1)) / std::max<size_t>(1, cnt - 1);
                probePtrs.push_back(cachedRgb[pick].data());
            }
        } else if (!probeFallbackRgb.empty()) {
            for (const auto& fr : probeFallbackRgb) probePtrs.push_back(fr.data());
        }
        if (!probePtrs.empty()) {
            uint64_t probeBytesSum = 0;
            std::vector<uint8_t> tmpIdx(pixelCount);
            LzwEncoder probeLzw;
            for (const uint8_t* rgbPtr : probePtrs) {
                mapFrameRowRange(rgbPtr, nullptr, nullptr, tmpIdx.data(), outW, 0, outH, mapper, snapDistSq, 0, true);
                auto lzwOut = probeLzw.compress(tmpIdx.data(), pixelCount, minCodeSize, cfg.lossyLevel, &nearTable);
                probeBytesSum += uint64_t(18 + lzwOut.size());
            }
            uint64_t est = uint64_t((double(probeBytesSum) / double(probePtrs.size())) * double(totalFrames)) + 2048;
            res.estBytes = est;
            if (est > cfg.targetMaxBytes) {
                res.needsRetry = true;
                return res;
            }
        }
    }
    std::vector<std::vector<uint8_t>>().swap(probeFallbackRgb);

    ULONGLONG eStart = GetTickCount64();
    std::ofstream gifOut(gif.c_str(), std::ios::binary);
    if (!gifOut) return res;
    writeGifHeader(gifOut, outW, outH, palette, gctBits);

    const unsigned numThreads = std::clamp(std::thread::hardware_concurrency(), 1u, 16u);

    if (!streamingPass2 && cachedRgb.size() == totalFrames) {
        std::vector<std::vector<uint8_t>> indexedFrames(totalFrames, std::vector<uint8_t>(pixelCount));
        if (cfg.stabTol > 0) {
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
                        mapFrameRowRange(
                            cachedRgb[f].data() + sliceOffsetBytes,
                            sliceStab.data(), slicePrevIdx.data(),
                            indexedFrames[f].data() + sliceOffsetPx,
                            outW, 0, y1 - y0, mapper, snapDistSq, cfg.stabTol, f == 0
                        );
                    }
                });
            }
            for (auto& w : workers) w.join();
        } else {
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
                            outW, 0, outH, mapper, snapDistSq, 0, true
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
                    lzwFrames[f] = lzw.compress(indexedFrames[f].data(), pixelCount, minCodeSize, cfg.lossyLevel, &nearTable);
                }
            });
        }
        for (auto& w : lzwWorkers) w.join();

        for (size_t f = 0; f < totalFrames; ++f) {
            writeGifFrame(gifOut, outW, outH, kept[f].delayCs, lzwFrames[f]);
        }
    } else {
        NativeVideo decoder2(video);
        if (decoder2.error < 0) return res;
        std::vector<uint8_t> stabRgb(cfg.stabTol > 0 ? rgbBytes : 0, 0);
        std::vector<uint8_t> prevIdx(cfg.stabTol > 0 ? pixelCount : 0, 0);

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
                        batchLzw[idx] = lzw.compress(batchIndexed[idx].data(), pixelCount, minCodeSize, cfg.lossyLevel, &nearTable);
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
            if (!decoder2.rgb(current, outW, outH)) return res;
            packRgbaToRgb24(current.data(), tmpRgb.data(), pixelCount);
            std::vector<uint8_t> mapped(pixelCount);
            mapFrameRowRange(
                tmpRgb.data(),
                cfg.stabTol > 0 ? stabRgb.data() : nullptr,
                cfg.stabTol > 0 ? prevIdx.data() : nullptr,
                mapped.data(),
                outW, 0, outH, mapper, snapDistSq, cfg.stabTol, targetIdx == 0
            );
            batchIndexed.push_back(std::move(mapped));
            batchDelays.push_back(kept[targetIdx].delayCs);
            ++targetIdx;
            if (batchIndexed.size() >= batchCap) flushBatch();
        }
        flushBatch();
        if (targetIdx != totalFrames) return res;
    }

    uint8_t trailer = 0x3B;
    gifOut.write(reinterpret_cast<const char*>(&trailer), 1);
    gifOut.flush();
    bool writeOk = gifOut.good();
    res.fileBytes = uint64_t(gifOut.tellp());
    gifOut.close();
    res.encodeMs = GetTickCount64() - eStart;
    res.ok = writeOk && totalFrames > 0;
    if (res.ok && allowProbeAbort && cfg.targetMaxBytes > 0 && res.fileBytes > uint64_t(double(cfg.targetMaxBytes) * 1.12)) {
        res.needsRetry = true;
    }
    return res;
}

inline void adaptParamsForBudget(GifParams& cfg, int curW, int curH, size_t curFrames, uint64_t observedBytes) {
    double ratio = (double(cfg.targetMaxBytes) * 0.78) / double(std::max<uint64_t>(1, observedBytes));
    int oldMaxDim = std::max(curW, curH);
    int oldFps = cfg.effFps;
    int oldLossy = cfg.lossyLevel;
    int oldColors = cfg.maxColors;

    cfg.lossyLevel = std::min(3, cfg.lossyLevel + 1);
    double lossyGain = (cfg.lossyLevel > oldLossy) ? 1.28 : 1.0;

    if (cfg.maxColors > 64) cfg.maxColors = 64;
    else if (ratio < 0.55 && cfg.maxColors > 48) cfg.maxColors = 48;
    double colorGain = (cfg.maxColors < oldColors) ? 1.10 : 1.0;

    double fpsGain = 1.0;
    if (curFrames > 32 && ratio < 0.72) {
        double fpsFactor = std::clamp(std::pow(ratio, 0.28), 0.55, 0.85);
        int newFps = std::max(8, int(std::floor(double(oldFps) * fpsFactor)));
        if (newFps < oldFps) {
            cfg.effFps = newFps;
            fpsGain = double(oldFps) / double(newFps);
        }
    }

    double areaRatio = ratio * lossyGain * colorGain * fpsGain;
    double dimScale = std::clamp(std::sqrt(areaRatio), 0.28, 0.90);
    int newMaxW = std::max(360, int(std::lround(double(oldMaxDim) * dimScale)) & ~1);
    if (newMaxW >= oldMaxDim && cfg.effFps >= oldFps && cfg.lossyLevel <= oldLossy) {
        newMaxW = std::max(360, int(std::lround(double(oldMaxDim) * 0.78)) & ~1);
    }
    cfg.effMaxWidth = newMaxW;
}

} // namespace native_gif_detail

// qualityMode:
//   0 = 小体积 (≤6MB), 1 = 均衡 (≤12MB), 2 = 高清 (≤25MB), 3 = 原尺寸精确回归模式（不设上限、不降采样、无损容差）
inline bool encodeNativeGif(
    const std::wstring& tool, const std::wstring& video, const std::wstring& gif, const std::wstring& log,
    int fps, int width, int height, int maxWidth = 0, int qualityMode = 3
) {
    using namespace native_gif_detail;
    ULONGLONG began = GetTickCount64();

    double estDurationSec = 0.0;
    {
        NativeVideo probe(video);
        if (probe.error < 0) return false;
        if (width <= 0 || height <= 0) {
            width = probe.sourceWidth();
            height = probe.sourceHeight();
        }
        if (probe.container && probe.container->duration > 0) {
            estDurationSec = double(probe.container->duration) / double(AV_TIME_BASE);
        }
    }
    if (width < 2 || height < 2) return false;

    GifParams cfg{};
    cfg.qualityMode = std::clamp(qualityMode, 0, 3);
    cfg.effFps = std::clamp(fps, 1, 50);

    if (cfg.qualityMode == 0) {
        cfg.effFps = std::min(cfg.effFps, 12);
        cfg.effMaxWidth = maxWidth > 0 ? maxWidth : 720;
        cfg.maxColors = 64;
        cfg.dedupeTol = 6;
        cfg.stabTol = 6;
        cfg.lossyLevel = 2;
        cfg.targetMaxBytes = 6ull * 1024 * 1024;
    } else if (cfg.qualityMode == 1) {
        cfg.effFps = std::min(cfg.effFps, 15);
        cfg.effMaxWidth = maxWidth > 0 ? maxWidth : 1080;
        cfg.maxColors = 96;
        cfg.dedupeTol = 5;
        cfg.stabTol = 5;
        cfg.lossyLevel = 1;
        cfg.targetMaxBytes = 12ull * 1024 * 1024;
    } else if (cfg.qualityMode == 2) {
        if (estDurationSec > 6.0) cfg.effFps = std::min(cfg.effFps, 20);
        cfg.effMaxWidth = maxWidth;
        cfg.maxColors = 128;
        cfg.dedupeTol = 4;
        cfg.stabTol = 3;
        cfg.lossyLevel = 1;
        cfg.targetMaxBytes = 25ull * 1024 * 1024;
        // 超高分辨率长视频首轮预缩放，避免首轮扫描耗时过长
        if (cfg.effMaxWidth == 0 && estDurationSec > 4.0) {
            double pixelBudget = double(width) * double(height) * double(cfg.effFps) * estDurationSec;
            if (pixelBudget > 700e6) cfg.effMaxWidth = 1280;
            else if (pixelBudget > 350e6) cfg.effMaxWidth = 1600;
        }
    } else {
        cfg.effMaxWidth = maxWidth;
        cfg.maxColors = 256;
        cfg.dedupeTol = 0;
        cfg.stabTol = 0;
        cfg.lossyLevel = 0;
        cfg.targetMaxBytes = 0;
    }

    AttemptOutcome outcome;
    int attempts = 0;
    const int maxAttempts = cfg.targetMaxBytes > 0 ? 3 : 1;
    while (attempts < maxAttempts) {
        bool allowAbort = (attempts + 1 < maxAttempts);
        outcome = encodeNativeGifAttempt(tool, video, gif, width, height, cfg, allowAbort);
        ++attempts;
        if (!outcome.needsRetry || !allowAbort) break;
        uint64_t observed = outcome.fileBytes > 0 ? outcome.fileBytes : outcome.estBytes;
        adaptParamsForBudget(cfg, outcome.outW, outcome.outH, outcome.framesWritten, observed);
    }

    std::ofstream stats(log.c_str(), std::ios::app);
    stats << "\nGIF fast width=" << outcome.outW << " height=" << outcome.outH
          << " src_w=" << width << " src_h=" << height
          << " fps=" << cfg.effFps << " colors=" << outcome.colors
          << " mode=" << cfg.qualityMode << " lossy=" << cfg.lossyLevel
          << " attempts=" << attempts
          << " frames_read=" << outcome.readFrames << " frames_written=" << outcome.framesWritten
          << " file_bytes=" << outcome.fileBytes
          << " elapsed_ms=" << (GetTickCount64() - began)
          << " read_ms=" << outcome.readMs << " quant_ms=" << outcome.quantMs << " encode_ms=" << outcome.encodeMs
          << " streaming=" << (outcome.streaming ? 1 : 0) << "\n";

    return outcome.ok;
}
