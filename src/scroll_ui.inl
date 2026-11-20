// 长截图控制面板与选区边框：支持手动滚动实时拼接及自动滚动捕获。
#include "scroll_stitch.hpp"

RECT scrollArea{};
ScrollStitcher scrollStitcher;
bool scrollPreviewDirty = true;
std::wstring scrollHint = L"向下滚动自动拼接";
int scrollNoChangeStreak = 0;
ULONGLONG scrollLastWheelTick = 0, scrollNextCaptureTick = 0;
POINT scrollAutoAnchor{};
HHOOK scrollMouseHook = nullptr;
ID2D1HwndRenderTarget* scrollRt = nullptr;
ID2D1Bitmap* scrollPreviewBmp = nullptr;

void setScrollAuto(bool enable);

LRESULT CALLBACK scrollFrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT p;
        HDC dc = BeginPaint(hwnd, &p);
        RECT r;
        GetClientRect(hwnd, &r);
        HBRUSH brush = CreateSolidBrush(scrollAutoMode ? RGB(64, 223, 203) : RGB(124, 230, 197));
        FillRect(dc, &r, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &p);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK scrollHookProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && scrollPanel) {
        auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
        bool injected = (ms->flags & LLMHF_INJECTED) != 0;
        if (!injected) {
            if (wp == WM_MOUSEWHEEL) {
                scrollLastWheelTick = GetTickCount64();
                scrollNextCaptureTick = scrollLastWheelTick + 95;
                if (scrollAutoMode) setScrollAuto(false);
            } else if (scrollAutoMode) {
                if (wp == WM_LBUTTONDOWN || wp == WM_RBUTTONDOWN || wp == WM_MBUTTONDOWN) {
                    RECT pr{};
                    GetWindowRect(scrollPanel, &pr);
                    if (!PtInRect(&pr, ms->pt)) setScrollAuto(false);
                } else if (wp == WM_MOUSEMOVE) {
                    if (std::hypot(double(ms->pt.x - scrollAutoAnchor.x), double(ms->pt.y - scrollAutoAnchor.y)) > 28.0) {
                        setScrollAuto(false);
                    }
                }
            }
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

void setScrollAuto(bool enable) {
    if (scrollAutoMode == enable) return;
    scrollAutoMode = enable;
    scrollNoChangeStreak = 0;
    if (scrollPanel) {
        if (scrollAutoMode) {
            scrollHint = L"自动滚动中…";
            int cx = (scrollArea.left + scrollArea.right) / 2;
            int cy = (scrollArea.top + scrollArea.bottom) / 2;
            SetCursorPos(cx, cy);
            scrollAutoAnchor = {cx, cy};
            SetTimer(scrollPanel, 2, 320, nullptr);
        } else {
            KillTimer(scrollPanel, 2);
            if (scrollHint == L"自动滚动中…") scrollHint = L"向下滚动自动拼接";
        }
        HWND autoBtn = GetDlgItem(scrollPanel, ScrollAuto);
        if (autoBtn) {
            setLabel(autoBtn, scrollAutoMode ? L"暂停自动滚动" : L"自动向下滚动");
            InvalidateRect(autoBtn, nullptr, FALSE);
        }
        InvalidateRect(scrollPanel, nullptr, FALSE);
    }
}

void scrollCaptureStep() {
    if (!scrollPanel) return;
    ULONGLONG now = GetTickCount64();
    if (scrollNextCaptureTick && now < scrollNextCaptureTick) return;
    scrollNextCaptureTick = 0;

    Image frame = captureDesktop(scrollArea);
    if (!frame.valid()) return;
    auto res = scrollStitcher.append(frame);
    if (res == StitchResult::Appended) {
        scrollNoChangeStreak = 0;
        scrollPreviewDirty = true;
        scrollHint = scrollAutoMode ? L"自动滚动中…" : L"向下滚动自动拼接";
        InvalidateRect(scrollPanel, nullptr, FALSE);
    } else if (res == StitchResult::NoChange) {
        if (scrollAutoMode) {
            ++scrollNoChangeStreak;
            if (scrollNoChangeStreak >= 3) {
                setScrollAuto(false);
                scrollHint = L"已到达底部";
                InvalidateRect(scrollPanel, nullptr, FALSE);
            }
        }
    } else if (res == StitchResult::TooFast) {
        if (scrollAutoMode) setScrollAuto(false);
        scrollHint = L"滚动过快，请稍微回滚";
        InvalidateRect(scrollPanel, nullptr, FALSE);
    } else if (res == StitchResult::MaxHeightReached) {
        scrollPreviewDirty = true;
        if (scrollAutoMode) setScrollAuto(false);
        scrollHint = L"已达到最大拼接高度";
        InvalidateRect(scrollPanel, nullptr, FALSE);
    }
}

void finishScrollAction(int action) {
    setScrollAuto(false);
    Image result = scrollStitcher.finalize();
    if (action == ScrollCancel || !result.valid()) {
        endScrollUI();
        return;
    }
    if (action == ScrollCopy) {
        if (!copyImage(mainWindow, result)) {
            showError(scrollPanel, L"剪贴板暂时被其他程序占用，请再试一次。");
            return;
        }
        lastImage = std::move(result);
        endScrollUI();
        message(L"长截图已复制到剪贴板。");
        return;
    }
    if (action == ScrollSave) {
        std::wstring initial = timestamp(L"_长截图.png");
        wchar_t file[32768]{};
        wcsncpy(file, initial.c_str(), 32767);
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = scrollPanel;
        dialog.lpstrFilter = english ? L"PNG image\0*.png\0\0" : L"PNG 图片\0*.png\0\0";
        dialog.lpstrFile = file;
        dialog.nMaxFile = 32768;
        dialog.lpstrDefExt = L"png";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&dialog)) return;
        if (!savePng(result, file)) {
            showError(scrollPanel, L"保存失败，请检查目标目录。");
            return;
        }
        lastImage = std::move(result);
        endScrollUI();
        message(L"长截图已保存为 PNG。");
        return;
    }
    if (action == ScrollPin) {
        copyImage(mainWindow, result);
        lastImage = result;
        endScrollUI();
        pinImage(std::move(result));
        message(L"贴图已置顶 · 滚轮缩放 · Alt+滚轮调透明度 · 右键关闭");
    }
}

LRESULT CALLBACK scrollPanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        float s = mainScale;
        const std::pair<int, const wchar_t*> btns[] = {
            {ScrollAuto, L"自动向下滚动"},
            {ScrollCopy, L"复制长截图（Enter）"},
            {ScrollSave, L"保存长截图（Ctrl+S）"},
            {ScrollPin, L"贴图"},
            {ScrollCancel, L"取消长截图（Esc）"}
        };
        int x = 14;
        for (auto e : btns) {
            button(hwnd, e.first, e.second, int(x * s), int(308 * s), int(38 * s), int(38 * s));
            x += 42;
        }
        SetTimer(hwnd, 1, 100, nullptr);
        return 0;
    }
    case WM_DRAWITEM:
        drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
        return TRUE;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ScrollAuto) setScrollAuto(!scrollAutoMode);
        else finishScrollAction(id);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1) {
            scrollCaptureStep();
        } else if (wp == 2 && scrollAutoMode) {
            INPUT in{};
            in.type = INPUT_MOUSE;
            in.mi.dwFlags = MOUSEEVENTF_WHEEL;
            in.mi.mouseData = DWORD(-3 * WHEEL_DELTA);
            SendInput(1, &in, sizeof(in));
            scrollNextCaptureTick = GetTickCount64() + 120;
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { finishScrollAction(ScrollCancel); return 0; }
        if (wp == VK_RETURN) { finishScrollAction(ScrollCopy); return 0; }
        if (GetKeyState(VK_CONTROL) < 0 && wp == 'S') { finishScrollAction(ScrollSave); return 0; }
        break;
    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        roundWindow(hwnd);
        if (scrollRt) scrollRt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (!scrollRt) scrollRt = createTarget(hwnd);
        if (scrollRt) {
            if (scrollPreviewDirty) {
                release(scrollPreviewBmp);
                int maxTailRows = std::max(64, int(std::lround(double(std::max(1, scrollStitcher.width)) * 224.0 / 208.0)));
                Image tail = scrollStitcher.tailPreview(maxTailRows);
                if (tail.valid()) scrollPreviewBmp = bitmapFor(scrollRt, tail);
                scrollPreviewDirty = false;
            }
            scrollRt->BeginDraw();
            scrollRt->SetTransform(D2D1::Matrix3x2F::Scale(mainScale, mainScale));
            scrollRt->Clear(rgb(0x20262E));

            std::wstring dims = std::to_wstring(scrollStitcher.width) + L" × " + std::to_wstring(scrollStitcher.currentHeight()) + L" px";
            text(scrollRt, L"长截图", {14, 10, 120, 34}, 15, INK, true);
            text(scrollRt, dims, {104, 12, 222, 34}, 12, ACCENT);
            unsigned hintColor = (scrollHint == L"滚动过快，请稍微回滚" || scrollHint == L"已达到最大拼接高度") ? 0xF0B879 : MUTED;
            text(scrollRt, scrollHint, {14, 38, 222, 66}, 12, hintColor);

            snip::Box box{14, 74, 222, 298};
            fill(scrollRt, box, 0x161A1F);
            if (scrollPreviewBmp) {
                auto sz = scrollPreviewBmp->GetSize();
                if (sz.width > 0 && sz.height > 0) {
                    float bw = box.width(), bh = box.height();
                    float scaleRatio = std::min(bw / sz.width, bh / sz.height);
                    float dw = sz.width * scaleRatio, dh = sz.height * scaleRatio;
                    float dx = box.l + (bw - dw) * 0.5f;
                    float dy = box.t + (bh - dh) * 0.5f;
                    scrollRt->DrawBitmap(scrollPreviewBmp, D2D1::RectF(dx, dy, dx + dw, dy + dh), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                }
            }
            ID2D1SolidColorBrush* border = nullptr;
            scrollRt->CreateSolidColorBrush(rgb(0x36404C), &border);
            if (border) { scrollRt->DrawRectangle(rect(box), border, 1.0f); release(border); }

            if (FAILED(scrollRt->EndDraw())) {
                release(scrollPreviewBmp);
                release(scrollRt);
                scrollPreviewDirty = true;
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        scrollAutoMode = false;
        if (scrollMouseHook) { UnhookWindowsHookEx(scrollMouseHook); scrollMouseHook = nullptr; }
        release(scrollPreviewBmp);
        release(scrollRt);
        scrollPanel = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void endScrollUI() {
    hideTip();
    if (scrollMouseHook) { UnhookWindowsHookEx(scrollMouseHook); scrollMouseHook = nullptr; }
    if (scrollPanel) { DestroyWindow(scrollPanel); scrollPanel = nullptr; }
    if (scrollFrame) { DestroyWindow(scrollFrame); scrollFrame = nullptr; }
    scrollStitcher.reset({});
    scrollPreviewDirty = true;
}

void beginScrollUI(RECT area) {
    endScrollUI();
    scrollArea = area;
    scrollAutoMode = false;
    scrollNoChangeStreak = 0;
    scrollLastWheelTick = 0;
    scrollNextCaptureTick = 0;
    scrollHint = L"向下滚动自动拼接";

    Sleep(50);
    DwmFlush();
    Image initial = captureDesktop(scrollArea);
    if (!initial.valid()) {
        showMain();
        showError(mainWindow, L"无法读取屏幕。请确认桌面没有锁定。");
        return;
    }
    scrollStitcher.reset(initial);
    scrollPreviewDirty = true;

    int border = 3;
    scrollFrame = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"ShuntieScrollFrame", L"瞬贴 · 长截图范围", WS_POPUP,
        area.left - border, area.top - border,
        area.right - area.left + border * 2, area.bottom - area.top + border * 2,
        nullptr, nullptr, instance, nullptr
    );
    if (scrollFrame) {
        RECT r;
        GetClientRect(scrollFrame, &r);
        HRGN outer = CreateRectRgn(0, 0, r.right, r.bottom);
        HRGN inner = CreateRectRgn(border, border, r.right - border, r.bottom - border);
        CombineRgn(outer, outer, inner, RGN_DIFF);
        DeleteObject(inner);
        SetWindowRgn(scrollFrame, outer, FALSE);
        SetLayeredWindowAttributes(scrollFrame, 0, 255, LWA_ALPHA);
        SetWindowDisplayAffinity(scrollFrame, 0x11);
        ShowWindow(scrollFrame, SW_SHOWNOACTIVATE);
    }

    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&area, MONITOR_DEFAULTTONEAREST), &mi);
    RECT work = mi.rcWork;
    int pw = int(236 * mainScale), ph = int(360 * mainScale);
    int px = 0, py = 0;
    if (area.right + 12 + pw <= work.right) {
        px = area.right + 12;
        py = std::clamp(int(area.top), int(work.top), int(work.bottom) - ph);
    } else if (area.left - 12 - pw >= work.left) {
        px = area.left - 12 - pw;
        py = std::clamp(int(area.top), int(work.top), int(work.bottom) - ph);
    } else if (area.bottom + 12 + ph <= work.bottom) {
        px = std::clamp(int(area.right) - pw, int(work.left), int(work.right) - pw);
        py = area.bottom + 12;
    } else if (area.top - 12 - ph >= work.top) {
        px = std::clamp(int(area.right) - pw, int(work.left), int(work.right) - pw);
        py = area.top - 12 - ph;
    } else {
        px = work.right - pw - 16;
        py = work.bottom - ph - 16;
    }

    scrollPanel = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"ShuntieScrollPanel", L"瞬贴 · 长截图", WS_POPUP | WS_CLIPCHILDREN,
        px, py, pw, ph,
        nullptr, nullptr, instance, nullptr
    );
    if (scrollPanel) {
        roundWindow(scrollPanel);
        SetWindowDisplayAffinity(scrollPanel, 0x11);
        scrollMouseHook = SetWindowsHookExW(WH_MOUSE_LL, scrollHookProc, instance, 0);
        ShowWindow(scrollPanel, SW_SHOWNOACTIVATE);
    } else {
        endScrollUI();
    }
}
