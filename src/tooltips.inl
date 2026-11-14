// 自绘提示：统一圆角和居中文本，120 毫秒后显示，不抢输入焦点。
LRESULT CALLBACK tipProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_NCHITTEST)return HTTRANSPARENT;
    if(msg==WM_PAINT){PAINTSTRUCT p;BeginPaint(hwnd,&p);auto rt=createTarget(hwnd);if(rt){rt->BeginDraw();rt->Clear(rgb(0x303841));RECT r;GetClientRect(hwnd,&r);IDWriteTextFormat* format=nullptr;ID2D1SolidColorBrush* brush=nullptr;writeFactory->CreateTextFormat(L"Microsoft YaHei UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13*tipScale,L"zh-cn",&format);rt->CreateSolidColorBrush(rgb(INK),&brush);if(format&&brush){format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);rt->DrawText(customTipText.c_str(),UINT(customTipText.size()),format,D2D1::RectF(0,0,float(r.right),float(r.bottom)),brush);}release(brush);release(format);rt->EndDraw();release(rt);}EndPaint(hwnd,&p);return 0;}
    if(msg==WM_NCDESTROY)customTip=nullptr;
    return DefWindowProcW(hwnd,msg,wp,lp);
}
void hideTip(){if(customTip)ShowWindow(customTip,SW_HIDE);}
void showTip(HWND source){
    wchar_t title[180]{};GetWindowTextW(source,title,180);if(!title[0])return;customTipText=title;tipScale=float(GetDpiForWindow(source))/96;
    IDWriteTextFormat* format=nullptr;IDWriteTextLayout* layout=nullptr;DWRITE_TEXT_METRICS metrics{};writeFactory->CreateTextFormat(L"Microsoft YaHei UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13*tipScale,L"zh-cn",&format);if(format&&SUCCEEDED(writeFactory->CreateTextLayout(title,UINT(wcslen(title)),format,800,80,&layout)))layout->GetMetrics(&metrics);release(layout);release(format);
    int width=int(std::ceil(metrics.widthIncludingTrailingWhitespace)+28*tipScale),height=int(34*tipScale);RECT r;GetWindowRect(source,&r);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(source,MONITOR_DEFAULTTONEAREST),&monitor);int x=std::clamp((r.left+r.right-width)/2,monitor.rcWork.left+4,monitor.rcWork.right-width-4),y=r.top-height-int(8*tipScale);if(y<monitor.rcWork.top)y=r.bottom+int(8*tipScale);
    if(!customTip)customTip=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT,L"ShuntieTip",L"工具提示",WS_POPUP,x,y,width,height,GetAncestor(source,GA_ROOT),nullptr,instance,nullptr);
    SetWindowPos(customTip,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);roundWindow(customTip);InvalidateRect(customTip,nullptr,FALSE);ShowWindow(customTip,SW_SHOWNOACTIVATE);
}
LRESULT CALLBACK hoverProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    constexpr UINT_PTR timer=0xB001;
    if(msg==WM_MOUSEMOVE&&hoveredButton!=hwnd){if(hoveredButton)KillTimer(hoveredButton,timer);hideTip();HWND previous=hoveredButton;hoveredButton=hwnd;if(previous)InvalidateRect(previous,nullptr,FALSE);InvalidateRect(hwnd,nullptr,FALSE);TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0};TrackMouseEvent(&track);SetTimer(hwnd,timer,120,nullptr);}
    if(msg==WM_TIMER&&wp==timer){KillTimer(hwnd,timer);if(hoveredButton==hwnd&&GetAsyncKeyState(VK_LBUTTON)>=0)showTip(hwnd);return 0;}
    if(msg==WM_MOUSELEAVE){KillTimer(hwnd,timer);if(hoveredButton==hwnd){hoveredButton=nullptr;hideTip();InvalidateRect(hwnd,nullptr,FALSE);}}
    if(msg==WM_LBUTTONDOWN){KillTimer(hwnd,timer);hideTip();}
    if(msg==WM_NCDESTROY){KillTimer(hwnd,timer);if(hoveredButton==hwnd){hoveredButton=nullptr;hideTip();}RemoveWindowSubclass(hwnd,hoverProc,1);}
    return DefSubclassProc(hwnd,msg,wp,lp);
}
