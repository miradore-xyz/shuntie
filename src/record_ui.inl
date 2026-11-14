LRESULT CALLBACK frameProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_NCHITTEST)return HTTRANSPARENT;
    if(msg==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(hwnd,&p);RECT r;GetClientRect(hwnd,&r);bool paused=recorder.status().phase==RecordingStatus::Paused;HBRUSH brush=CreateSolidBrush(paused?RGB(240,184,121):RGB(124,230,197));FillRect(dc,&r,brush);DeleteObject(brush);EndPaint(hwnd,&p);return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
void beginRecordingUI(RECT area){
    int border=3;recordFrame=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_LAYERED,L"ShuntieFrame",L"瞬贴 · 录制范围",WS_POPUP,area.left-border,area.top-border,area.right-area.left+border*2,area.bottom-area.top+border*2,nullptr,nullptr,instance,nullptr);
    if(recordFrame){RECT r;GetClientRect(recordFrame,&r);HRGN outer=CreateRectRgn(0,0,r.right,r.bottom),inner=CreateRectRgn(border,border,r.right-border,r.bottom-border);CombineRgn(outer,outer,inner,RGN_DIFF);DeleteObject(inner);SetWindowRgn(recordFrame,outer,FALSE);SetLayeredWindowAttributes(recordFrame,0,255,LWA_ALPHA);SetWindowDisplayAffinity(recordFrame,0x11);ShowWindow(recordFrame,SW_SHOWNOACTIVATE);}
    RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int width=int(280*mainScale),height=int(52*mainScale);int x=std::clamp(area.right-width,work.left,work.right-width),y=area.top-height-12;if(y<work.top)y=std::min(work.bottom-height,area.bottom+12);
    recordPanel=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"ShuntieRecord",L"瞬贴 · 录制控制",WS_POPUP|WS_CLIPCHILDREN,x,y,width,height,nullptr,nullptr,instance,nullptr);
    if(recordPanel){roundWindow(recordPanel);SetWindowDisplayAffinity(recordPanel,0x11);ShowWindow(recordPanel,SW_SHOWNOACTIVATE);}
}
LRESULT CALLBACK recordProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    static HWND tips=nullptr;
    switch(msg){
    case WM_CREATE:{int x=132;for(auto entry:{std::pair<int,const wchar_t*>{PauseRecord,L"暂停 / 继续"},{CopyRecording,L"复制已录 GIF（会暂停）"},{StopRecord,L"结束录制"}}){HWND b=button(hwnd,entry.first,entry.second,int(x*mainScale),int(7*mainScale),int(38*mainScale),int(38*mainScale));x+=44;}
        SetTimer(hwnd,1,250,nullptr);return 0;}
    case WM_NOTIFY:if(reinterpret_cast<NMHDR*>(lp)->code==TTN_GETDISPINFOW){auto tip=reinterpret_cast<NMTTDISPINFOW*>(lp);static wchar_t title[160];GetWindowTextW(reinterpret_cast<HWND>(tip->hdr.idFrom),title,160);tip->lpszText=title;return 0;}break;
    case WM_DRAWITEM:drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_COMMAND:if(LOWORD(wp)==StopRecord)recorder.stop();if(LOWORD(wp)==CopyRecording)recorder.copySnapshot();if(LOWORD(wp)==PauseRecord){if(recorder.status().phase==RecordingStatus::Paused)recorder.resume();else recorder.pause();}return 0;
    case WM_TIMER:{auto phase=recorder.status().phase;EnableWindow(GetDlgItem(hwnd,PauseRecord),phase==RecordingStatus::Recording||phase==RecordingStatus::Paused);EnableWindow(GetDlgItem(hwnd,CopyRecording),phase==RecordingStatus::Recording||phase==RecordingStatus::Paused);for(int id:{PauseRecord,CopyRecording,int(StopRecord)})InvalidateRect(GetDlgItem(hwnd,id),nullptr,FALSE);InvalidateRect(hwnd,nullptr,FALSE);return 0;}
    case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT p;BeginPaint(hwnd,&p);auto rt=createTarget(hwnd);if(rt){rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Scale(mainScale,mainScale));rt->Clear(rgb(0x20262E));auto s=recorder.status();ULONGLONG ms=s.elapsedMs+(s.phase==RecordingStatus::Recording?GetTickCount64()-s.started:0),secs=ms/1000;std::wstring label=std::to_wstring(secs/60)+L":"+(secs%60<10?L"0":L"")+std::to_wstring(secs%60);if(s.phase==RecordingStatus::Paused)label=L"已暂停 "+label;else if(s.phase==RecordingStatus::Exporting)label=L"导出中…";else if(s.phase==RecordingStatus::Starting)label=L"准备中…";text(rt,label,{14,15,130,43},14,s.phase==RecordingStatus::Paused?0xF0B879:INK);rt->EndDraw();release(rt);}EndPaint(hwnd,&p);return 0;}
    case WM_DESTROY:KillTimer(hwnd,1);tips=nullptr;return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
