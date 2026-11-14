#define NOMINMAX
#define UNICODE
#include "../src/window_snap.hpp"
#include <cassert>
#include <iostream>
int main(){SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);RECT desktop{x,y,x+GetSystemMetrics(SM_CXVIRTUALSCREEN),y+GetSystemMetrics(SM_CYVIRTUALSCREEN)};auto windows=captureSnapWindows();assert(!windows.empty());int checked=0;for(auto& window:windows){RECT r;if(!IntersectRect(&r,&window.bounds,&desktop))continue;POINT p{(r.left+r.right)/2,(r.top+r.bottom)/2};auto box=snapAt(windows,p,desktop);assert(box.width()>0&&box.height()>0&&box.l>=0&&box.t>=0&&box.r<=desktop.right-desktop.left&&box.b<=desktop.bottom-desktop.top);assert(box.contains({float(p.x-desktop.left),float(p.y-desktop.top)}));++checked;}assert(checked>0);std::cout<<"Native window detection passed: "<<checked<<". / 原生窗口识别及范围检查通过。\n";}
