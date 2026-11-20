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
    static constexpr int MAX_NEAR = 12;
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

// 高速 GIF89a LZW 压缩器（支持无损及 Kornel 一维误差扩散有损字典匹配，绝不将透明与不透明像素互相替换）。
struct LzwEncoder {
    static constexpr uint32_t HASH_SIZE = 16384;
    static constexpr uint32_t HASH_MASK = HASH_SIZE - 1;
    std::vector<uint32_t> keys;
    std::vector<uint16_t> vals;
    std::vector<uint32_t> gens;
    uint32_t gen = 1;

    LzwEncoder() : keys(HASH_SIZE, 0), vals(HASH_SIZE, 0), gens(HASH_SIZE, 0) {}

    std::vector<uint8_t> compress(
        const uint8_t* pixels, const uint8_t* origRgb, int w, int h,
        int minCodeSize, int transIdx, const std::vector<Rgb>& palette,
        int lossyStrength = 0, const NearPaletteTable* nearTable = nullptr
    ) {
        const size_t count = size_t(w) * size_t(h);
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

        const bool lossy = (lossyStrength > 0 && origRgb != nullptr && nearTable != nullptr && !palette.empty());
        const int maxDistSq = lossyStrength * lossyStrength * 3;
        const int maxRunErr = maxDistSq * 3;
        int runErr = 0;
        int errR = 0, errG = 0, errB = 0;

        uint32_t prefix = pixels[0];
        if (lossy && int(prefix) != transIdx && prefix < palette.size()) {
            errR = std::clamp(int(origRgb[0]) - int(palette[prefix].r), -24, 24);
            errG = std::clamp(int(origRgb[1]) - int(palette[prefix].g), -24, 24);
            errB = std::clamp(int(origRgb[2]) - int(palette[prefix].b), -24, 24);
        }

        for (int y = 0; y < h; ++y) {
            errR = errG = errB = 0;
            const int xStart = (y == 0) ? 1 : 0;
            for (int x = xStart; x < w; ++x) {
                const size_t i = size_t(y) * size_t(w) + size_t(x);
                const uint8_t k = pixels[i];
                const uint32_t key = (prefix << 8) | k;
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
                if (found) {
                    if (!lossy || int(k) == transIdx || k >= palette.size()) {
                        errR = errG = errB = 0;
                    } else {
                        int tR = std::clamp(int(origRgb[i * 3 + 0]) + (errR * 3) / 4, 0, 255);
                        int tG = std::clamp(int(origRgb[i * 3 + 1]) + (errG * 3) / 4, 0, 255);
                        int tB = std::clamp(int(origRgb[i * 3 + 2]) + (errB * 3) / 4, 0, 255);
                        errR = std::clamp(tR - int(palette[k].r), -24, 24);
                        errG = std::clamp(tG - int(palette[k].g), -24, 24);
                        errB = std::clamp(tB - int(palette[k].b), -24, 24);
                    }
                    continue;
                }

                if (lossy && int(k) != transIdx && k < palette.size() && runErr < maxRunErr) {
                    int tR = std::clamp(int(origRgb[i * 3 + 0]) + (errR * 3) / 4, 0, 255);
                    int tG = std::clamp(int(origRgb[i * 3 + 1]) + (errG * 3) / 4, 0, 255);
                    int tB = std::clamp(int(origRgb[i * 3 + 2]) + (errB * 3) / 4, 0, 255);
                    int bestD2 = maxDistSq + 1;
                    uint8_t bestAlt = 0;
                    uint16_t bestVal = 0xFFFFu;
                    const uint8_t nc = nearTable->nearCount[k];
                    for (uint8_t j = 0; j < nc; ++j) {
                        const uint8_t alt = nearTable->nearIdx[k][j];
                        int dr = tR - int(palette[alt].r);
                        int dg = tG - int(palette[alt].g);
                        int db = tB - int(palette[alt].b);
                        int d2 = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
                        if (d2 >= bestD2 || runErr + d2 > maxRunErr) continue;
                        uint32_t aKey = (prefix << 8) | alt;
                        uint32_t aSlot = ((aKey * 2654435761u) >> 18) & HASH_MASK;
                        while (gens[aSlot] == gen) {
                            if (keys[aSlot] == aKey) {
                                bestD2 = d2;
                                bestAlt = alt;
                                bestVal = vals[aSlot];
                                break;
                            }
                            aSlot = (aSlot + 1u) & HASH_MASK;
                        }
                    }
                    if (bestVal != 0xFFFFu) {
                        prefix = bestVal;
                        runErr += bestD2;
                        errR = std::clamp(tR - int(palette[bestAlt].r), -32, 32);
                        errG = std::clamp(tG - int(palette[bestAlt].g), -32, 32);
                        errB = std::clamp(tB - int(palette[bestAlt].b), -32, 32);
                        continue;
                    }
                }

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
                if (!lossy || int(k) == transIdx || k >= palette.size()) {
                    errR = errG = errB = 0;
                } else {
                    errR = std::clamp(int(origRgb[i * 3 + 0]) - int(palette[k].r), -24, 24);
                    errG = std::clamp(int(origRgb[i * 3 + 1]) - int(palette[k].g), -24, 24);
                    errB = std::clamp(int(origRgb[i * 3 + 2]) - int(palette[k].b), -24, 24);
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

// 始终写单全局调色板 (LCTF=0) 且 Disposal=1 (不擦除)，局部差异透明复用大幅减体积且 100% 兼容 QQ。
inline void writeGifFrame(
    std::ofstream& out, int left, int top, int w, int h,
    unsigned totalDelayCs, bool useTrans, uint8_t transIdx, const std::vector<uint8_t>& lzwData
) {
    unsigned remaining = std::max(1u, totalDelayCs);
    while (remaining > 0) {
        unsigned d = std::min(remaining, 65535u);
        uint8_t gce[8] = {
            0x21, 0xF9, 0x04,
            uint8_t(useTrans ? 0x05 : 0x04), // Disposal = 1 (Do Not Dispose), TransparencyFlag = useTrans ? 1 : 0
            uint8_t(d & 0xFFu), uint8_t((d >> 8) & 0xFFu),
            uint8_t(useTrans ? transIdx : 0x00), 0x00
        };
        uint8_t desc[10] = {
            0x2C,
            uint8_t(left & 0xFF), uint8_t((left >> 8) & 0xFF),
            uint8_t(top & 0xFF), uint8_t((top >> 8) & 0xFF),
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

struct RowBox {
    int minX = INT32_MAX;
    int maxX = -1;
    bool hasOpaque = false;
    bool hasTrans = false;
};

struct DeltaSubFrame {
    int left = 0, top = 0, w = 0, h = 0;
    bool useTrans = false;
    std::vector<uint8_t> indices;
    std::vector<uint8_t> rgb;
};

inline void mapOpaqueRows(
    const uint8_t* currRgb, uint8_t* canvasRgb, uint8_t* canvasIdx, uint8_t* outIdx,
    int outW, int y0, int y1, PaletteMapper& mapper, int snapDistSq
) {
    for (int y = y0; y < y1; ++y) {
        size_t rowBase = size_t(y) * size_t(outW);
        uint8_t rowPrevIdx = 0;
        bool hasRowPrev = false;
        for (int x = 0; x < outW; ++x) {
            size_t p = rowBase + size_t(x);
            size_t o = p * 3;
            uint8_t r = currRgb[o + 0];
            uint8_t g = currRgb[o + 1];
            uint8_t b = currRgb[o + 2];
            uint8_t chosen = mapper.mapPixel(r, g, b);
            if (snapDistSq > 0 && hasRowPrev && rowPrevIdx != chosen) {
                const Rgb& pr = mapper.palette[rowPrevIdx];
                int drp = int(r) - int(pr.r), dgp = int(g) - int(pr.g), dbp = int(b) - int(pr.b);
                int dPrev = 2 * drp * drp + 4 * dgp * dgp + 3 * dbp * dbp;
                if (dPrev <= snapDistSq) {
                    const Rgb& br = mapper.palette[chosen];
                    int drb = int(r) - int(br.r), dgb = int(g) - int(br.g), dbb = int(b) - int(br.b);
                    int dBest = 2 * drb * drb + 4 * dgb * dgb + 3 * dbb * dbb;
                    if (dPrev <= dBest + snapDistSq) chosen = rowPrevIdx;
                }
            }
            outIdx[p] = chosen;
            rowPrevIdx = chosen;
            hasRowPrev = true;
            if (canvasIdx) canvasIdx[p] = chosen;
            if (canvasRgb) {
                const Rgb& qr = mapper.palette[chosen];
                canvasRgb[o + 0] = qr.r;
                canvasRgb[o + 1] = qr.g;
                canvasRgb[o + 2] = qr.b;
            }
        }
    }
}

inline void mapDeltaRows(
    const uint8_t* currRgb, uint8_t* canvasRgb, uint8_t* canvasIdx, uint8_t* fullOutIdx,
    RowBox* rowBoxes, int outW, int y0, int y1, PaletteMapper& mapper,
    int snapDistSq, int stabTol, bool hasTrans, uint8_t transIdx
) {
    const int stabTolSq6 = stabTol * stabTol * 6;
    for (int y = y0; y < y1; ++y) {
        size_t rowBase = size_t(y) * size_t(outW);
        uint8_t rowPrevIdx = 0;
        bool hasRowPrev = false;
        RowBox rb;
        for (int x = 0; x < outW; ++x) {
            size_t p = rowBase + size_t(x);
            size_t o = p * 3;
            uint8_t r = currRgb[o + 0];
            uint8_t g = currRgb[o + 1];
            uint8_t b = currRgb[o + 2];

            if (hasTrans && stabTol > 0) {
                int dr = int(r) - int(canvasRgb[o + 0]);
                int dg = int(g) - int(canvasRgb[o + 1]);
                int db = int(b) - int(canvasRgb[o + 2]);
                if (std::abs(dr) <= stabTol && std::abs(dg) <= stabTol && std::abs(db) <= stabTol &&
                    (2 * dr * dr + 4 * dg * dg + 3 * db * db) <= stabTolSq6) {
                    fullOutIdx[p] = transIdx;
                    rb.hasTrans = true;
                    hasRowPrev = false;
                    continue;
                }
            }

            uint8_t chosen = mapper.mapPixel(r, g, b);
            if (hasTrans && chosen == canvasIdx[p]) {
                fullOutIdx[p] = transIdx;
                rb.hasTrans = true;
                hasRowPrev = false;
                continue;
            }

            if (snapDistSq > 0 && hasRowPrev && rowPrevIdx != chosen) {
                const Rgb& pr = mapper.palette[rowPrevIdx];
                int drp = int(r) - int(pr.r), dgp = int(g) - int(pr.g), dbp = int(b) - int(pr.b);
                int dPrev = 2 * drp * drp + 4 * dgp * dgp + 3 * dbp * dbp;
                if (dPrev <= snapDistSq) {
                    const Rgb& br = mapper.palette[chosen];
                    int drb = int(r) - int(br.r), dgb = int(g) - int(br.g), dbb = int(b) - int(br.b);
                    int dBest = 2 * drb * drb + 4 * dgb * dgb + 3 * dbb * dbb;
                    if (dPrev <= dBest + snapDistSq && (!hasTrans || rowPrevIdx != canvasIdx[p])) {
                        chosen = rowPrevIdx;
                    }
                }
            }

            fullOutIdx[p] = chosen;
            canvasIdx[p] = chosen;
            const Rgb& qr = mapper.palette[chosen];
            canvasRgb[o + 0] = qr.r;
            canvasRgb[o + 1] = qr.g;
            canvasRgb[o + 2] = qr.b;
            rowPrevIdx = chosen;
            hasRowPrev = true;
            if (!rb.hasOpaque) {
                rb.minX = x;
                rb.maxX = x;
                rb.hasOpaque = true;
            } else {
                rb.maxX = x;
            }
        }
        rowBoxes[y] = rb;
    }
}

inline DeltaSubFrame extractDirtySubFrame(
    const uint8_t* fullIdx, const uint8_t* currRgb, const std::vector<RowBox>& rowBoxes,
    int outW, int outH, bool hasTrans, uint8_t transIdx, bool keepRgb
) {
    DeltaSubFrame sub;
    int minX = outW, maxX = -1, minY = outH, maxY = -1;
    for (int y = 0; y < outH; ++y) {
        if (!rowBoxes[y].hasOpaque) continue;
        minX = std::min(minX, rowBoxes[y].minX);
        maxX = std::max(maxX, rowBoxes[y].maxX);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    if (maxX < minX || maxY < minY) {
        return sub; // 整帧无变化，w=0, h=0 -> 合并延迟
    }
    sub.left = minX;
    sub.top = minY;
    sub.w = maxX - minX + 1;
    sub.h = maxY - minY + 1;
    const size_t subPixels = size_t(sub.w) * size_t(sub.h);
    sub.indices.resize(subPixels);
    if (keepRgb) sub.rgb.resize(subPixels * 3);

    bool anyTrans = false;
    for (int r = 0; r < sub.h; ++r) {
        int sy = sub.top + r;
        const uint8_t* srcRow = fullIdx + size_t(sy) * size_t(outW) + size_t(sub.left);
        uint8_t* dstRow = sub.indices.data() + size_t(r) * size_t(sub.w);
        std::memcpy(dstRow, srcRow, size_t(sub.w));
        if (hasTrans && !anyTrans && rowBoxes[sy].hasTrans) {
            for (int c = 0; c < sub.w; ++c) {
                if (dstRow[c] == transIdx) {
                    anyTrans = true;
                    break;
                }
            }
        }
        if (keepRgb) {
            const uint8_t* srcRgbRow = currRgb + (size_t(sy) * size_t(outW) + size_t(sub.left)) * 3;
            uint8_t* dstRgbRow = sub.rgb.data() + size_t(r) * size_t(sub.w) * 3;
            std::memcpy(dstRgbRow, srcRgbRow, size_t(sub.w) * 3);
        }
    }
    sub.useTrans = hasTrans && anyTrans;
    return sub;
}

struct GifParams {
    int effFps;
    int effMaxWidth;
    int maxColors;
    int dedupeTol;
    int stabTol;
    int lossyStrength;
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
    std::vector<uint8_t> probeFrame0Rgb;
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> probePairs;
    std::vector<uint8_t> streamLastKeptRgb;
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
            std::vector<uint8_t> rgb(rgbBytes);
            packRgbaToRgb24(rgba, rgb.data(), pixelCount);
            if (!streamingPass2) {
                streamingPass2 = true;
                if (!cachedRgb.empty()) {
                    probeFrame0Rgb = cachedRgb[0];
                    size_t n = cachedRgb.size();
                    if (n >= 2) {
                        size_t want = std::min<size_t>(4, n - 1);
                        for (size_t i = 0; i < want; ++i) {
                            size_t pick = 1 + (i * (n - 1)) / want;
                            probePairs.emplace_back(cachedRgb[pick - 1], cachedRgb[pick]);
                        }
                    }
                    streamLastKeptRgb = cachedRgb.back();
                }
                std::vector<std::vector<uint8_t>>().swap(cachedRgb);
                cachedBytes = 0;
            }
            if (probePairs.size() < 5 && !streamLastKeptRgb.empty()) {
                if (kept.size() % 12 == 0) {
                    probePairs.emplace_back(streamLastKeptRgb, rgb);
                }
            }
            streamLastKeptRgb = std::move(rgb);
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
    std::vector<uint8_t>().swap(streamLastKeptRgb);

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

    const bool hasTrans = palette.size() <= 255;
    const uint8_t transIdx = hasTrans ? uint8_t(palette.size()) : 0;
    const size_t totalPaletteEntries = palette.size() + (hasTrans ? 1u : 0u);
    int gctBits = 2;
    while ((1 << gctBits) < int(totalPaletteEntries) && gctBits < 8) ++gctBits;
    const int minCodeSize = gctBits;
    res.colors = palette.size();

    PaletteMapper mapper;
    mapper.init(palette, exactPalette);

    const int snapDistSq = (cfg.lossyStrength > 0) ? std::clamp(cfg.lossyStrength * 3, 60, 420) : 0;
    const int maxSubDistSq = (cfg.lossyStrength > 0) ? (cfg.lossyStrength * cfg.lossyStrength * 3) : 0;
    NearPaletteTable nearTable;
    if (cfg.lossyStrength > 0 && !exactPalette) {
        nearTable.build(mapper.palette, maxSubDistSq);
    }
    res.quantMs = GetTickCount64() - qStart;

    const size_t totalFrames = kept.size();

    // 基于增量差分透明块与有损 LZW 的快速体积探针（在完整编码前预估大小）
    if (allowProbeAbort && cfg.targetMaxBytes > 0 && totalFrames > 4) {
        const uint8_t* f0Ptr = !streamingPass2 && !cachedRgb.empty() ? cachedRgb[0].data()
                              : (!probeFrame0Rgb.empty() ? probeFrame0Rgb.data() : nullptr);
        std::vector<std::pair<const uint8_t*, const uint8_t*>> pairPtrs;
        if (!streamingPass2 && cachedRgb.size() >= 2) {
            size_t cnt = std::min<size_t>(5, cachedRgb.size() - 1);
            for (size_t i = 0; i < cnt; ++i) {
                size_t pick = 1 + (i * (cachedRgb.size() - 2)) / std::max<size_t>(1, cnt - 1);
                pick = std::clamp<size_t>(pick, 1, cachedRgb.size() - 1);
                pairPtrs.emplace_back(cachedRgb[pick - 1].data(), cachedRgb[pick].data());
            }
        } else {
            for (const auto& pr : probePairs) {
                pairPtrs.emplace_back(pr.first.data(), pr.second.data());
            }
        }
        if (f0Ptr && !pairPtrs.empty()) {
            LzwEncoder probeLzw;
            std::vector<uint8_t> tmpIdx(pixelCount);
            mapOpaqueRows(f0Ptr, nullptr, nullptr, tmpIdx.data(), outW, 0, outH, mapper, snapDistSq);
            auto lzw0 = probeLzw.compress(
                tmpIdx.data(), cfg.lossyStrength > 0 ? f0Ptr : nullptr,
                outW, outH, minCodeSize, hasTrans ? int(transIdx) : -1,
                mapper.palette, cfg.lossyStrength, &nearTable
            );
            uint64_t size0 = uint64_t(18 + lzw0.size());

            uint64_t deltaSum = 0;
            std::vector<uint8_t> pairCanvasRgb(rgbBytes);
            std::vector<uint8_t> pairCanvasIdx(pixelCount);
            std::vector<RowBox> pairRows(outH);
            for (const auto& pr : pairPtrs) {
                mapOpaqueRows(pr.first, pairCanvasRgb.data(), pairCanvasIdx.data(), tmpIdx.data(), outW, 0, outH, mapper, snapDistSq);
                mapDeltaRows(
                    pr.second, pairCanvasRgb.data(), pairCanvasIdx.data(), tmpIdx.data(),
                    pairRows.data(), outW, 0, outH, mapper, snapDistSq, cfg.stabTol, hasTrans, transIdx
                );
                auto dSub = extractDirtySubFrame(
                    tmpIdx.data(), pr.second, pairRows, outW, outH,
                    hasTrans, transIdx, cfg.lossyStrength > 0
                );
                if (dSub.w > 0 && dSub.h > 0) {
                    auto lzwD = probeLzw.compress(
                        dSub.indices.data(), cfg.lossyStrength > 0 ? dSub.rgb.data() : nullptr,
                        dSub.w, dSub.h, minCodeSize, dSub.useTrans ? int(transIdx) : -1,
                        mapper.palette, cfg.lossyStrength, &nearTable
                    );
                    deltaSum += uint64_t(18 + lzwD.size());
                } else {
                    deltaSum += 18;
                }
            }
            uint64_t avgDelta = deltaSum / pairPtrs.size();
            uint64_t est = size0 + avgDelta * uint64_t(totalFrames - 1) + 2048;
            res.estBytes = est;
            if (est > cfg.targetMaxBytes) {
                res.needsRetry = true;
                return res;
            }
        }
    }
    std::vector<uint8_t>().swap(probeFrame0Rgb);
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>().swap(probePairs);

    ULONGLONG eStart = GetTickCount64();
    std::ofstream gifOut(gif.c_str(), std::ios::binary);
    if (!gifOut) return res;
    writeGifHeader(gifOut, outW, outH, palette, gctBits);

    const unsigned numThreads = std::clamp(std::thread::hardware_concurrency(), 1u, 16u);
    const unsigned sliceThreads = std::min<unsigned>(numThreads, std::max<unsigned>(1, unsigned(outH / 16)));
    std::vector<std::pair<int, int>> bands;
    for (unsigned t = 0; t < sliceThreads; ++t) {
        int y0 = (t == 0) ? 0 : ((outH * int(t)) / int(sliceThreads));
        int y1 = (t + 1 == sliceThreads) ? outH : ((outH * int(t + 1)) / int(sliceThreads));
        if (y1 > y0) bands.emplace_back(y0, y1);
    }

    struct PendingOut {
        int left = 0, top = 0, w = 0, h = 0;
        bool useTrans = false;
        unsigned delayCs = 0;
        std::vector<uint8_t> lzw;
    };
    bool hasPendingOut = false;
    PendingOut pendingOut{};
    size_t emittedCount = 0;

    auto emitOrFold = [&](int left, int top, int w, int h, bool useTrans, unsigned delayCs, std::vector<uint8_t>&& lzw) {
        if (w <= 0 || h <= 0) {
            if (hasPendingOut) pendingOut.delayCs += delayCs;
            return;
        }
        if (hasPendingOut) {
            writeGifFrame(gifOut, pendingOut.left, pendingOut.top, pendingOut.w, pendingOut.h,
                          pendingOut.delayCs, pendingOut.useTrans, transIdx, pendingOut.lzw);
            ++emittedCount;
        }
        pendingOut = {left, top, w, h, useTrans, delayCs, std::move(lzw)};
        hasPendingOut = true;
    };

    std::vector<uint8_t> canvasRgb(rgbBytes);
    std::vector<uint8_t> canvasIdx(pixelCount);
    std::vector<uint8_t> fullOutIdx(pixelCount);
    std::vector<RowBox> rowBoxes(outH);

    auto encodeAndWriteBatch = [&](std::vector<DeltaSubFrame>& batch, std::vector<unsigned>& batchDelays) {
        if (batch.empty()) return;
        std::vector<std::vector<uint8_t>> batchLzw(batch.size());
        std::atomic<size_t> nextItem{0};
        unsigned bt = std::min<unsigned>(numThreads, unsigned(batch.size()));
        std::vector<std::thread> workers;
        for (unsigned t = 0; t < bt; ++t) {
            workers.emplace_back([&]() {
                LzwEncoder lzw;
                for (;;) {
                    size_t idx = nextItem.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= batch.size()) break;
                    auto& sf = batch[idx];
                    if (sf.w > 0 && sf.h > 0) {
                        batchLzw[idx] = lzw.compress(
                            sf.indices.data(),
                            cfg.lossyStrength > 0 && !sf.rgb.empty() ? sf.rgb.data() : nullptr,
                            sf.w, sf.h, minCodeSize, sf.useTrans ? int(transIdx) : -1,
                            mapper.palette, cfg.lossyStrength, &nearTable
                        );
                    }
                }
            });
        }
        for (auto& w : workers) w.join();
        for (size_t i = 0; i < batch.size(); ++i) {
            emitOrFold(batch[i].left, batch[i].top, batch[i].w, batch[i].h,
                       batch[i].useTrans, batchDelays[i], std::move(batchLzw[i]));
        }
        batch.clear();
        batchDelays.clear();
    };

    auto processFrameToSub = [&](size_t frameNo, const uint8_t* currRgbPtr) -> DeltaSubFrame {
        if (frameNo == 0) {
            if (bands.size() <= 1) {
                mapOpaqueRows(currRgbPtr, canvasRgb.data(), canvasIdx.data(), fullOutIdx.data(), outW, 0, outH, mapper, snapDistSq);
            } else {
                std::vector<std::thread> workers;
                for (auto b : bands) {
                    workers.emplace_back([&, b]() {
                        mapOpaqueRows(currRgbPtr, canvasRgb.data(), canvasIdx.data(), fullOutIdx.data(),
                                      outW, b.first, b.second, mapper, snapDistSq);
                    });
                }
                for (auto& w : workers) w.join();
            }
            DeltaSubFrame f0;
            f0.left = 0; f0.top = 0; f0.w = outW; f0.h = outH;
            f0.useTrans = false;
            f0.indices = fullOutIdx;
            if (cfg.lossyStrength > 0) {
                f0.rgb.assign(currRgbPtr, currRgbPtr + rgbBytes);
            }
            return f0;
        }
        if (bands.size() <= 1) {
            mapDeltaRows(currRgbPtr, canvasRgb.data(), canvasIdx.data(), fullOutIdx.data(),
                         rowBoxes.data(), outW, 0, outH, mapper, snapDistSq, cfg.stabTol, hasTrans, transIdx);
        } else {
            std::vector<std::thread> workers;
            for (auto b : bands) {
                workers.emplace_back([&, b]() {
                    mapDeltaRows(currRgbPtr, canvasRgb.data(), canvasIdx.data(), fullOutIdx.data(),
                                 rowBoxes.data(), outW, b.first, b.second, mapper, snapDistSq, cfg.stabTol, hasTrans, transIdx);
                });
            }
            for (auto& w : workers) w.join();
        }
        return extractDirtySubFrame(fullOutIdx.data(), currRgbPtr, rowBoxes, outW, outH,
                                    hasTrans, transIdx, cfg.lossyStrength > 0);
    };

    std::vector<DeltaSubFrame> batch;
    std::vector<unsigned> batchDelays;
    size_t batchBytes = 0;
    constexpr size_t MAX_BATCH_BYTES = 64ull * 1024 * 1024;
    const size_t batchMaxCount = std::max<size_t>(4, size_t(numThreads) * 2);

    auto pushDeltaToBatch = [&](DeltaSubFrame&& sub, unsigned delayCs) {
        if (sub.w <= 0 || sub.h <= 0) {
            if (!batchDelays.empty()) {
                batchDelays.back() += delayCs;
            } else {
                encodeAndWriteBatch(batch, batchDelays);
                std::vector<uint8_t> emptyLzw;
                emitOrFold(0, 0, 0, 0, false, delayCs, std::move(emptyLzw));
            }
            return;
        }
        batchBytes += sub.indices.size() + sub.rgb.size();
        batch.push_back(std::move(sub));
        batchDelays.push_back(delayCs);
        if (batch.size() >= batchMaxCount || batchBytes >= MAX_BATCH_BYTES) {
            encodeAndWriteBatch(batch, batchDelays);
            batchBytes = 0;
        }
    };

    if (!streamingPass2 && cachedRgb.size() == totalFrames) {
        for (size_t f = 0; f < totalFrames; ++f) {
            DeltaSubFrame sub = processFrameToSub(f, cachedRgb[f].data());
            std::vector<uint8_t>().swap(cachedRgb[f]);
            pushDeltaToBatch(std::move(sub), kept[f].delayCs);
        }
        std::vector<std::vector<uint8_t>>().swap(cachedRgb);
        encodeAndWriteBatch(batch, batchDelays);
    } else {
        NativeVideo decoder2(video);
        if (decoder2.error < 0) return res;
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
            DeltaSubFrame sub = processFrameToSub(targetIdx, tmpRgb.data());
            pushDeltaToBatch(std::move(sub), kept[targetIdx].delayCs);
            ++targetIdx;
        }
        encodeAndWriteBatch(batch, batchDelays);
        if (targetIdx != totalFrames) return res;
    }

    if (hasPendingOut) {
        writeGifFrame(gifOut, pendingOut.left, pendingOut.top, pendingOut.w, pendingOut.h,
                      pendingOut.delayCs, pendingOut.useTrans, transIdx, pendingOut.lzw);
        ++emittedCount;
    }

    uint8_t trailer = 0x3B;
    gifOut.write(reinterpret_cast<const char*>(&trailer), 1);
    gifOut.flush();
    bool writeOk = gifOut.good();
    res.fileBytes = uint64_t(gifOut.tellp());
    gifOut.close();
    res.encodeMs = GetTickCount64() - eStart;
    res.ok = writeOk && totalFrames > 0 && emittedCount > 0;
    if (res.ok && allowProbeAbort && cfg.targetMaxBytes > 0 && res.fileBytes > uint64_t(double(cfg.targetMaxBytes) * 1.12)) {
        res.needsRetry = true;
    }
    return res;
}

inline void adaptParamsForBudget(GifParams& cfg, int curW, int curH, size_t curFrames, uint64_t observedBytes) {
    double ratio = (double(cfg.targetMaxBytes) * 0.80) / double(std::max<uint64_t>(1, observedBytes));
    int oldMaxDim = std::max(curW, curH);
    int oldFps = cfg.effFps;

    // 优先保证原生 2K 分辨率清晰度：先增强 Kornel 有损 LZW、颜色量化挡位、帧间静默容差与帧率，仅在极端超标时最后缩尺寸。
    cfg.lossyStrength = std::min(145, cfg.lossyStrength + 35);
    cfg.stabTol = std::min(16, cfg.stabTol + 4);
    cfg.dedupeTol = std::min(12, cfg.dedupeTol + 2);

    if (cfg.maxColors > 127) cfg.maxColors = 127;
    else if (cfg.maxColors > 63) cfg.maxColors = 63;
    else if (ratio < 0.60 && cfg.maxColors > 31) cfg.maxColors = 31;

    if (curFrames > 24 && ratio < 0.85) {
        if (cfg.effFps > 15) cfg.effFps = 15;
        else if (cfg.effFps > 12) cfg.effFps = 12;
        else if (cfg.effFps > 10) cfg.effFps = 10;
        else if (cfg.effFps > 8 && ratio < 0.60) cfg.effFps = 8;
    }

    bool atFloor = (cfg.lossyStrength >= 115 && cfg.maxColors <= 63 && cfg.effFps <= 10);
    bool extremeOverflow = (double(observedBytes) > double(cfg.targetMaxBytes) * 4.0);
    if (atFloor || extremeOverflow) {
        double dimScale = std::clamp(std::sqrt(ratio * 1.4), 0.50, 0.85);
        int baseDim = cfg.effMaxWidth > 0 ? std::min(cfg.effMaxWidth, oldMaxDim) : oldMaxDim;
        int newMaxW = std::max(640, int(std::lround(double(baseDim) * dimScale)) & ~1);
        if (newMaxW >= baseDim) newMaxW = std::max(640, int(std::lround(double(baseDim) * 0.80)) & ~1);
        cfg.effMaxWidth = newMaxW;
    } else if (cfg.effFps == oldFps && cfg.lossyStrength >= 145) {
        cfg.effMaxWidth = std::max(640, int(std::lround(double(oldMaxDim) * 0.85)) & ~1);
    }
}

} // namespace native_gif_detail

// qualityMode:
//   0 = 2K 小体积 (≤8MB), 1 = 2K 均衡 (≤15MB), 2 = 2K 原画 (≤25MB), 3 = 原尺寸精确回归模式（无损容差）
inline bool encodeNativeGif(
    const std::wstring& tool, const std::wstring& video, const std::wstring& gif, const std::wstring& log,
    int fps, int width, int height, int maxWidth = 0, int qualityMode = 3
) {
    using namespace native_gif_detail;
    ULONGLONG began = GetTickCount64();

    if (width <= 0 || height <= 0) {
        NativeVideo probe(video);
        if (probe.error < 0) return false;
        width = probe.sourceWidth();
        height = probe.sourceHeight();
    }
    if (width < 2 || height < 2) return false;

    GifParams cfg{};
    cfg.qualityMode = std::clamp(qualityMode, 0, 3);
    cfg.effFps = std::clamp(fps, 1, 50);

    if (cfg.qualityMode == 0) {
        cfg.effFps = std::min(cfg.effFps, 10);
        cfg.effMaxWidth = maxWidth > 0 ? maxWidth : 2560;
        cfg.maxColors = 63;
        cfg.dedupeTol = 7;
        cfg.stabTol = 10;
        cfg.lossyStrength = 80;
        cfg.targetMaxBytes = 8ull * 1024 * 1024;
    } else if (cfg.qualityMode == 1) {
        cfg.effFps = std::min(cfg.effFps, 15);
        cfg.effMaxWidth = maxWidth > 0 ? maxWidth : 2560;
        cfg.maxColors = 127;
        cfg.dedupeTol = 5;
        cfg.stabTol = 7;
        cfg.lossyStrength = 45;
        cfg.targetMaxBytes = 15ull * 1024 * 1024;
    } else if (cfg.qualityMode == 2) {
        cfg.effFps = std::min(cfg.effFps, 20);
        cfg.effMaxWidth = maxWidth > 0 ? maxWidth : 2560;
        cfg.maxColors = 255;
        cfg.dedupeTol = 4;
        cfg.stabTol = 4;
        cfg.lossyStrength = 25;
        cfg.targetMaxBytes = 25ull * 1024 * 1024;
    } else {
        cfg.effMaxWidth = maxWidth;
        cfg.maxColors = 256;
        cfg.dedupeTol = 0;
        cfg.stabTol = 0;
        cfg.lossyStrength = 0;
        cfg.targetMaxBytes = 0;
    }

    AttemptOutcome outcome;
    int attempts = 0;
    const int maxAttempts = cfg.targetMaxBytes > 0 ? 4 : 1;
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
          << " mode=" << cfg.qualityMode << " lossy=" << cfg.lossyStrength
          << " attempts=" << attempts
          << " frames_read=" << outcome.readFrames << " frames_written=" << outcome.framesWritten
          << " file_bytes=" << outcome.fileBytes
          << " elapsed_ms=" << (GetTickCount64() - began)
          << " read_ms=" << outcome.readMs << " quant_ms=" << outcome.quantMs << " encode_ms=" << outcome.encodeMs
          << " streaming=" << (outcome.streaming ? 1 : 0) << "\n";

    return outcome.ok;
}
