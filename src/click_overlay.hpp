#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// 录屏鼠标点击波纹与按住高亮指示器（允许被录屏捕获，绝不设置 WDA_EXCLUDEFROMCAPTURE）。
class ClickOverlay {
    static constexpr int MAX_RIPPLES = 4;
    static constexpr UINT_PTR ANIM_TIMER = 0xC101;
    static constexpr UINT_PTR POLL_TIMER = 0xC102;
    static constexpr ULONGLONG RIPPLE_MS = 420;

    struct Ripple {
        bool active = false;
        POINT pt{};
        int button = 0; // 0=左键(珊瑚红), 1=右键(天蓝), 2=中键(琥珀黄)
        ULONGLONG startTick = 0;
        HWND hwnd = nullptr;
    };

    inline static ClickOverlay* activeInstance = nullptr;
    inline static bool classesRegistered = false;

    HINSTANCE hInst = nullptr;
    HWND hostHwnd = nullptr;
    HWND panelHwnd = nullptr;
    HWND holdHwnd = nullptr;
    Ripple ripples[MAX_RIPPLES]{};
    HHOOK mouseHook = nullptr;

    RECT area{};
    float scale = 1.0f;
    int boxSize = 64;
    bool enabled = true;
    bool recordingActive = false;
    bool animTimerOn = false;

    int holdButton = -1;
    POINT holdPt{};
    bool holdVisible = false;
    bool holdDirty = false;
    ULONGLONG lastHookDownTick[3]{};
    bool prevAsyncDown[3]{};

    HDC memDc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ oldBmp = nullptr;
    uint32_t* dibPixels = nullptr;

    static void buttonRgb(int btn, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (btn == 1) { r = 0x3B; g = 0x82; b = 0xF6; }      // 右键：天蓝
        else if (btn == 2) { r = 0xF5; g = 0x9E; b = 0x0B; } // 中键：琥珀黄
        else { r = 0xFF; g = 0x5A; b = 0x4E; }               // 左键：珊瑚红
    }

    static float smoothstep(float e0, float e1, float x) {
        float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static uint32_t premul(uint8_t r, uint8_t g, uint8_t b, float a) {
        float alpha = std::clamp(a, 0.0f, 1.0f);
        uint8_t aa = uint8_t(std::lround(alpha * 255.0f));
        uint8_t rr = uint8_t(std::lround(r * alpha));
        uint8_t gg = uint8_t(std::lround(g * alpha));
        uint8_t bb = uint8_t(std::lround(b * alpha));
        return (uint32_t(aa) << 24) | (uint32_t(rr) << 16) | (uint32_t(gg) << 8) | uint32_t(bb);
    }

    void ensureSurface() {
        int needed = std::max(48, int(std::ceil(68.0f * scale)));
        if (dib && boxSize == needed) return;
        releaseSurface();
        boxSize = needed;
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = boxSize;
        bi.bmiHeader.biHeight = -boxSize;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HDC screen = GetDC(nullptr);
        memDc = CreateCompatibleDC(screen);
        dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, reinterpret_cast<void**>(&dibPixels), nullptr, 0);
        if (memDc && dib) oldBmp = SelectObject(memDc, dib);
        ReleaseDC(nullptr, screen);
    }

    void releaseSurface() {
        if (memDc && oldBmp) { SelectObject(memDc, oldBmp); oldBmp = nullptr; }
        if (dib) { DeleteObject(dib); dib = nullptr; dibPixels = nullptr; }
        if (memDc) { DeleteDC(memDc); memDc = nullptr; }
    }

    bool pointAllowed(POINT pt) const {
        if (!PtInRect(&area, pt)) return false;
        if (panelHwnd && IsWindow(panelHwnd) && IsWindowVisible(panelHwnd)) {
            RECT pr{};
            if (GetWindowRect(panelHwnd, &pr) && PtInRect(&pr, pt)) return false;
        }
        return true;
    }

    void renderRingWindow(HWND hwnd, POINT center, int btn, float radius, float strokeHalf, float ringAlpha, float fillAlpha) {
        if (!hwnd || !dibPixels || !memDc) return;
        uint8_t cr, cg, cb;
        buttonRgb(btn, cr, cg, cb);

        int half = boxSize / 2;
        int ox = center.x - half;
        int oy = center.y - half;
        float cx = float(half) - 0.5f;
        float cy = float(half) - 0.5f;

        for (int py = 0; py < boxSize; ++py) {
            int sy = oy + py;
            if (sy < area.top || sy >= area.bottom) {
                std::memset(dibPixels + size_t(py) * boxSize, 0, size_t(boxSize) * 4);
                continue;
            }
            float dy = float(py) - cy;
            for (int px = 0; px < boxSize; ++px) {
                int sx = ox + px;
                if (sx < area.left || sx >= area.right) {
                    dibPixels[size_t(py) * boxSize + size_t(px)] = 0;
                    continue;
                }
                float dx = float(px) - cx;
                float dist = std::hypot(dx, dy);
                float dRing = std::abs(dist - radius);

                // 外圈微弱高对比描边，保证红/蓝/黄圆环在深浅背景下都清晰可见。
                float haloMask = 1.0f - smoothstep(strokeHalf, strokeHalf + 1.4f * scale, dRing);
                float ringMask = 1.0f - smoothstep(std::max(0.4f, strokeHalf - 0.6f), strokeHalf + 0.6f, dRing);
                float fillMask = fillAlpha > 0.001f ? (1.0f - smoothstep(std::max(0.0f, radius - 1.0f), radius + 0.5f, dist)) : 0.0f;

                float aFill = fillMask * fillAlpha;
                float aHalo = haloMask * ringAlpha * 0.45f;
                float aRing = ringMask * ringAlpha;

                // Porter-Duff 顺序叠加：中心填充 -> 白/亮描边 -> 彩色主环
                float outA = aFill;
                float outR = cr * aFill;
                float outG = cg * aFill;
                float outB = cb * aFill;

                // 白色对比描边
                outR = 255.0f * aHalo + outR * (1.0f - aHalo);
                outG = 255.0f * aHalo + outG * (1.0f - aHalo);
                outB = 255.0f * aHalo + outB * (1.0f - aHalo);
                outA = aHalo + outA * (1.0f - aHalo);

                // 主彩色环
                outR = cr * aRing + outR * (1.0f - aRing);
                outG = cg * aRing + outG * (1.0f - aRing);
                outB = cb * aRing + outB * (1.0f - aRing);
                outA = aRing + outA * (1.0f - aRing);

                outA = std::clamp(outA, 0.0f, 1.0f);
                uint8_t aa = uint8_t(std::lround(outA * 255.0f));
                uint8_t rr = std::min(aa, uint8_t(std::clamp(int(std::lround(outR)), 0, 255)));
                uint8_t gg = std::min(aa, uint8_t(std::clamp(int(std::lround(outG)), 0, 255)));
                uint8_t bb = std::min(aa, uint8_t(std::clamp(int(std::lround(outB)), 0, 255)));
                dibPixels[size_t(py) * boxSize + size_t(px)] =
                    (uint32_t(aa) << 24) | (uint32_t(rr) << 16) | (uint32_t(gg) << 8) | uint32_t(bb);
            }
        }

        POINT dstPos{ox, oy};
        SIZE sz{boxSize, boxSize};
        POINT srcPos{0, 0};
        BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOWNA);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        UpdateLayeredWindow(hwnd, nullptr, &dstPos, &sz, memDc, &srcPos, 0, &bf, ULW_ALPHA);
    }

    void hideAllVisuals() {
        for (auto& r : ripples) {
            r.active = false;
            if (r.hwnd && IsWindowVisible(r.hwnd)) ShowWindow(r.hwnd, SW_HIDE);
        }
        holdButton = -1;
        holdVisible = false;
        holdDirty = false;
        if (holdHwnd && IsWindowVisible(holdHwnd)) ShowWindow(holdHwnd, SW_HIDE);
        if (animTimerOn && hostHwnd) {
            KillTimer(hostHwnd, ANIM_TIMER);
            animTimerOn = false;
        }
    }

    void ensureAnimTimer() {
        if (!animTimerOn && hostHwnd) {
            SetTimer(hostHwnd, ANIM_TIMER, 16, nullptr);
            animTimerOn = true;
        }
    }

    void spawnRipple(POINT pt, int btn) {
        if (!enabled || !recordingActive || !pointAllowed(pt)) return;
        int slot = 0;
        ULONGLONG oldest = UINT64_MAX;
        for (int i = 0; i < MAX_RIPPLES; ++i) {
            if (!ripples[i].active) { slot = i; break; }
            if (ripples[i].startTick < oldest) { oldest = ripples[i].startTick; slot = i; }
        }
        ripples[slot].active = true;
        ripples[slot].pt = pt;
        ripples[slot].button = btn;
        ripples[slot].startTick = GetTickCount64();
        ensureAnimTimer();
        tickAnimation();
    }

    void tickAnimation() {
        if (!enabled || !recordingActive) {
            hideAllVisuals();
            return;
        }
        ULONGLONG now = GetTickCount64();
        bool anyActive = false;

        for (auto& r : ripples) {
            if (!r.active) continue;
            ULONGLONG elapsed = now >= r.startTick ? now - r.startTick : 0;
            if (elapsed >= RIPPLE_MS) {
                r.active = false;
                if (r.hwnd && IsWindowVisible(r.hwnd)) ShowWindow(r.hwnd, SW_HIDE);
                continue;
            }
            anyActive = true;
            float t = float(elapsed) / float(RIPPLE_MS);
            float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            float radius = (8.0f + 18.0f * ease) * scale;
            float fade = 1.0f - t * t;
            float fillAlpha = t < 0.55f ? (1.0f - t / 0.55f) * 0.28f : 0.0f;
            renderRingWindow(r.hwnd, r.pt, r.button, radius, 1.6f * scale, 0.92f * fade, fillAlpha);
        }

        if (holdButton >= 0 && pointAllowed(holdPt)) {
            anyActive = true;
            if (!holdVisible || holdDirty) {
                renderRingWindow(holdHwnd, holdPt, holdButton, 13.0f * scale, 1.3f * scale, 0.72f, 0.14f);
                holdVisible = true;
                holdDirty = false;
            }
        } else if (holdVisible) {
            holdVisible = false;
            if (holdHwnd && IsWindowVisible(holdHwnd)) ShowWindow(holdHwnd, SW_HIDE);
        }

        if (!anyActive && animTimerOn && hostHwnd) {
            KillTimer(hostHwnd, ANIM_TIMER);
            animTimerOn = false;
        }
    }

    void pollAsyncMouseFallback() {
        if (!enabled || !recordingActive) return;
        static constexpr int vks[3] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
        ULONGLONG now = GetTickCount64();
        POINT pt{};
        GetCursorPos(&pt);
        for (int b = 0; b < 3; ++b) {
            bool down = (GetAsyncKeyState(vks[b]) & 0x8000) != 0;
            if (down && !prevAsyncDown[b]) {
                // 若 WH_MOUSE_LL 在 120ms 内已处理同一按键，则跳过（避免重复）；否则兜底管理员权限窗口点击。
                if (now - lastHookDownTick[b] > 120 && pointAllowed(pt)) {
                    holdButton = b;
                    holdPt = pt;
                    holdDirty = true;
                    spawnRipple(pt, b);
                }
            } else if (!down && prevAsyncDown[b]) {
                if (holdButton == b) {
                    holdButton = -1;
                    holdDirty = true;
                    if (holdVisible && holdHwnd) {
                        holdVisible = false;
                        ShowWindow(holdHwnd, SW_HIDE);
                    }
                }
            }
            prevAsyncDown[b] = down;
        }
        if (holdButton >= 0 && (pt.x != holdPt.x || pt.y != holdPt.y)) {
            holdPt = pt;
            holdDirty = true;
            ensureAnimTimer();
            tickAnimation();
        }
    }

    static LRESULT CALLBACK hookProc(int code, WPARAM wp, LPARAM lp) {
        ClickOverlay* self = activeInstance;
        if (code == HC_ACTION && self && self->enabled && self->recordingActive) {
            auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
            int btn = -1;
            bool isDown = false, isUp = false;
            switch (wp) {
            case WM_LBUTTONDOWN: btn = 0; isDown = true; break;
            case WM_LBUTTONUP:   btn = 0; isUp = true; break;
            case WM_RBUTTONDOWN: btn = 1; isDown = true; break;
            case WM_RBUTTONUP:   btn = 1; isUp = true; break;
            case WM_MBUTTONDOWN: btn = 2; isDown = true; break;
            case WM_MBUTTONUP:   btn = 2; isUp = true; break;
            case WM_MOUSEMOVE:
                if (self->holdButton >= 0) {
                    self->holdPt = ms->pt;
                    self->holdDirty = true;
                    self->ensureAnimTimer();
                }
                break;
            default: break;
            }
            if (isDown && btn >= 0) {
                self->lastHookDownTick[btn] = GetTickCount64();
                self->prevAsyncDown[btn] = true;
                if (self->pointAllowed(ms->pt)) {
                    self->holdButton = btn;
                    self->holdPt = ms->pt;
                    self->holdDirty = true;
                    self->spawnRipple(ms->pt, btn);
                }
            } else if (isUp && btn >= 0) {
                self->prevAsyncDown[btn] = false;
                if (self->holdButton == btn) {
                    self->holdButton = -1;
                    if (self->holdVisible && self->holdHwnd) {
                        self->holdVisible = false;
                        ShowWindow(self->holdHwnd, SW_HIDE);
                    }
                }
            }
        }
        return CallNextHookEx(nullptr, code, wp, lp);
    }

    static LRESULT CALLBACK hostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        ClickOverlay* self = activeInstance;
        if (msg == WM_TIMER && self) {
            if (wp == ANIM_TIMER) self->tickAnimation();
            else if (wp == POLL_TIMER) self->pollAsyncMouseFallback();
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK layerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_NCHITTEST) return HTTRANSPARENT;
        if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    void ensureClasses(HINSTANCE inst) {
        if (classesRegistered) return;
        WNDCLASSEXW hc{};
        hc.cbSize = sizeof(hc);
        hc.lpfnWndProc = hostProc;
        hc.hInstance = inst;
        hc.lpszClassName = L"ShuntieClickHost";
        RegisterClassExW(&hc);

        WNDCLASSEXW lc{};
        lc.cbSize = sizeof(lc);
        lc.lpfnWndProc = layerProc;
        lc.hInstance = inst;
        lc.lpszClassName = L"ShuntieClickRipple";
        RegisterClassExW(&lc);
        classesRegistered = true;
    }

public:
    ~ClickOverlay() { stop(); }

    void start(HINSTANCE inst, RECT recordArea, HWND panel, float dpiScale, bool showClicks) {
        stop();
        hInst = inst;
        area = recordArea;
        panelHwnd = panel;
        scale = std::max(1.0f, dpiScale);
        enabled = showClicks;
        recordingActive = false;
        std::memset(lastHookDownTick, 0, sizeof(lastHookDownTick));
        std::memset(prevAsyncDown, 0, sizeof(prevAsyncDown));

        ensureClasses(inst);
        ensureSurface();

        activeInstance = this;
        hostHwnd = CreateWindowExW(0, L"ShuntieClickHost", L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
        DWORD ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED;
        for (int i = 0; i < MAX_RIPPLES; ++i) {
            ripples[i] = {};
            ripples[i].hwnd = CreateWindowExW(ex, L"ShuntieClickRipple", L"", WS_POPUP, 0, 0, boxSize, boxSize, nullptr, nullptr, inst, nullptr);
        }
        holdHwnd = CreateWindowExW(ex, L"ShuntieClickRipple", L"", WS_POPUP, 0, 0, boxSize, boxSize, nullptr, nullptr, inst, nullptr);

        mouseHook = SetWindowsHookExW(WH_MOUSE_LL, hookProc, inst, 0);
        if (hostHwnd) SetTimer(hostHwnd, POLL_TIMER, 45, nullptr);
    }

    void setPanel(HWND panel) { panelHwnd = panel; }

    void setEnabled(bool showClicks) {
        if (enabled == showClicks) return;
        enabled = showClicks;
        if (!enabled) hideAllVisuals();
    }

    void setRecordingActive(bool active) {
        if (recordingActive == active) return;
        recordingActive = active;
        if (!recordingActive) {
            hideAllVisuals();
        } else {
            static constexpr int vks[3] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
            for (int b = 0; b < 3; ++b) prevAsyncDown[b] = (GetAsyncKeyState(vks[b]) & 0x8000) != 0;
        }
    }

    void stop() {
        if (mouseHook) { UnhookWindowsHookEx(mouseHook); mouseHook = nullptr; }
        if (hostHwnd) {
            KillTimer(hostHwnd, ANIM_TIMER);
            KillTimer(hostHwnd, POLL_TIMER);
            DestroyWindow(hostHwnd);
            hostHwnd = nullptr;
        }
        animTimerOn = false;
        for (int i = 0; i < MAX_RIPPLES; ++i) {
            if (ripples[i].hwnd) { DestroyWindow(ripples[i].hwnd); ripples[i].hwnd = nullptr; }
            ripples[i].active = false;
        }
        if (holdHwnd) { DestroyWindow(holdHwnd); holdHwnd = nullptr; }
        holdButton = -1;
        holdVisible = false;
        holdDirty = false;
        recordingActive = false;
        releaseSurface();
        if (activeInstance == this) activeInstance = nullptr;
    }
};
