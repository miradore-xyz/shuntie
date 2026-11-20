// 使用真实编辑器窗口过程做界面验收，不占用已安装程序的热键，也不关闭其窗口。
#define wWinMain shuntie_product_entry
#include "../src/main.cpp"
#undef wWinMain

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR arguments,int){
    instance=h;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(!initGraphics())return 1;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);mainScale=float(GetDpiForSystem())/96;uiFont=CreateFontW(-int(15*mainScale),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    struct Class{const wchar_t* name;WNDPROC proc;};Class classes[]={{L"ShuntieEditor",overlayProc},{L"ShuntiePin",pinProc},{L"ShuntieRecord",recordProc},{L"ShuntieFrame",frameProc},{L"ShuntieScrollFrame",scrollFrameProc},{L"ShuntieScrollPanel",scrollPanelProc},{L"ShuntieSettings",settingsProc},{L"ShuntieTip",tipProc},{L"ShuntieStroke",strokeProc},{L"ShuntiePalette",paletteProc}};
    for(auto c:classes){WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=c.proc;wc.hInstance=h;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=c.name;if(!RegisterClassExW(&wc))return 1;}
    wchar_t current[32768];GetCurrentDirectoryW(32768,current);root=std::wstring(current)+L"\\dist";outputDir=std::wstring(current)+L"\\artifacts\\ui-check";SHCreateDirectoryExW(nullptr,outputDir.c_str(),nullptr);mainWindow=CreateWindowExW(0,L"STATIC",L"验收剪贴板所有者",WS_POPUP,0,0,0,0,nullptr,nullptr,h,nullptr);
    beginCapture(wcsstr(arguments,L"--record")!=nullptr);if(overlay){SetWindowLongPtrW(overlay,GWL_EXSTYLE,(GetWindowLongPtrW(overlay,GWL_EXSTYLE)&~WS_EX_TOOLWINDOW)|WS_EX_APPWINDOW);SetWindowPos(overlay,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);};MSG msg;while((overlay||recordPanel||recorder.busy())&&GetMessageW(&msg,nullptr,0,0)>0){if(strokeWindow&&msg.message==WM_KEYDOWN){SendMessageW(strokeWindow,msg.message,msg.wParam,msg.lParam);continue;}if(paletteWindow&&msg.message==WM_KEYDOWN){SendMessageW(paletteWindow,msg.message,msg.wParam,msg.lParam);continue;}if(overlay&&msg.message==WM_KEYDOWN){SendMessageW(overlay,msg.message,msg.wParam,msg.lParam);continue;}TranslateMessage(&msg);DispatchMessageW(&msg);if(recordPanel)refreshStatus();}
    DestroyWindow(mainWindow);DeleteObject(uiFont);release(wic);release(writeFactory);release(d2d);CoUninitialize();return 0;
}
