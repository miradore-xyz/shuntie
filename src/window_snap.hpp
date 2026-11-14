#pragma once
#include <windows.h>
#include <dwmapi.h>
#include "model.hpp"
#include <vector>

// 抓屏前记录可见顶层窗口，避免全屏选区覆盖层挡住窗口识别。
struct SnapWindow { HWND hwnd;RECT bounds; };
inline std::vector<SnapWindow> captureSnapWindows(){
    std::vector<SnapWindow> windows;
    EnumWindows([](HWND hwnd,LPARAM value)->BOOL{
        if(!IsWindowVisible(hwnd)||IsIconic(hwnd))return TRUE;
        DWORD cloaked=0;DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)return TRUE;
        wchar_t name[80]{};GetClassNameW(hwnd,name,80);if(wcscmp(name,L"Progman")==0||wcscmp(name,L"WorkerW")==0)return TRUE;
        RECT r{};if(FAILED(DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))&&!GetWindowRect(hwnd,&r))return TRUE;
        if(r.right-r.left>=32&&r.bottom-r.top>=24)reinterpret_cast<std::vector<SnapWindow>*>(value)->push_back({hwnd,r});return TRUE;
    },reinterpret_cast<LPARAM>(&windows));return windows;
}
inline snip::Box snapAt(const std::vector<SnapWindow>& windows,POINT point,RECT desktop){
    RECT region=desktop;
    for(const auto& candidate:windows){if(!PtInRect(&candidate.bounds,point))continue;region=candidate.bounds;HWND parent=candidate.hwnd;
        // Win32 原生子窗口也可被识别，网页内部非原生元素不在此识别范围。
        for(int depth=0;depth<12;++depth){POINT local=point;ScreenToClient(parent,&local);HWND child=RealChildWindowFromPoint(parent,local);if(!child||child==parent||!IsWindowVisible(child))break;RECT bounds{};if(!GetWindowRect(child,&bounds)||bounds.right-bounds.left<80||bounds.bottom-bounds.top<40)break;RECT clipped{};if(!IntersectRect(&clipped,&region,&bounds)||!PtInRect(&clipped,point))break;region=clipped;parent=child;}
        break;
    }
    RECT clipped{};if(!IntersectRect(&clipped,&region,&desktop))clipped=desktop;
    return {float(clipped.left-desktop.left),float(clipped.top-desktop.top),float(clipped.right-desktop.left),float(clipped.bottom-desktop.top)};
}
