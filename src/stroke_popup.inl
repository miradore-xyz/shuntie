HWND strokeWindow=nullptr;
bool strokeChanged=false;
void setStroke(float value){
    value=std::clamp(std::round(value),1.f,24.f);if(value==editor.stroke)return;
    if(!strokeChanged&&editor.active>=0)editor.checkpoint();strokeChanged=true;editor.stroke=value;
    if(editor.active>=0)editor.shapes[editor.active].stroke=value;
    setLabel(GetDlgItem(overlay,Thickness),(L"线宽："+std::to_wstring(int(value))+L" 像素").c_str());InvalidateRect(strokeWindow,nullptr,FALSE);InvalidateRect(overlay,nullptr,FALSE);
}
LRESULT CALLBACK strokeProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_PAINT){PAINTSTRUCT p;BeginPaint(hwnd,&p);auto rt=createTarget(hwnd);if(rt){rt->BeginDraw();rt->Clear(rgb(0x20262E));rt->SetTransform(D2D1::Matrix3x2F::Scale(mainScale,mainScale));auto label=localize(L"线宽 "+std::to_wstring(int(editor.stroke))+L" 像素");IDWriteTextFormat* format=nullptr;ID2D1SolidColorBrush* brush=nullptr;writeFactory->CreateTextFormat(L"Microsoft YaHei UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,14,L"zh-cn",&format);rt->CreateSolidColorBrush(rgb(INK),&brush);if(format&&brush){format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);rt->DrawText(label.c_str(),UINT(label.size()),format,D2D1::RectF(0,8,260,39),brush);brush->SetColor(rgb(0x56616D));rt->DrawLine({22,65},{238,65},brush,3);brush->SetColor(rgb(ACCENT));float x=22+(editor.stroke-1)/23*216;rt->DrawLine({22,65},{x,65},brush,3);brush->SetColor(rgb(INK));rt->FillEllipse(D2D1::Ellipse({x,65},6,6),brush);}release(brush);release(format);rt->EndDraw();release(rt);}EndPaint(hwnd,&p);return 0;}
    if(msg==WM_LBUTTONDOWN){SetCapture(hwnd);float x=float(GET_X_LPARAM(lp))/mainScale;setStroke(1+(x-22)/216*23);return 0;}
    if(msg==WM_MOUSEMOVE&&GetCapture()==hwnd){float x=float(GET_X_LPARAM(lp))/mainScale;setStroke(1+(x-22)/216*23);return 0;}
    if(msg==WM_LBUTTONUP){ReleaseCapture();return 0;}
    if(msg==WM_KEYDOWN){if(wp==VK_LEFT||wp==VK_DOWN)setStroke(editor.stroke-1);if(wp==VK_RIGHT||wp==VK_UP)setStroke(editor.stroke+1);if(wp==VK_ESCAPE){DestroyWindow(hwnd);SetFocus(overlay);}return 0;}
    if(msg==WM_ACTIVATE&&LOWORD(wp)==WA_INACTIVE){DestroyWindow(hwnd);return 0;}
    if(msg==WM_DESTROY){strokeWindow=nullptr;return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
void showStroke(){
    if(strokeWindow){DestroyWindow(strokeWindow);return;}strokeChanged=false;RECT r;GetWindowRect(GetDlgItem(overlay,Thickness),&r);int width=int(260*mainScale),height=int(100*mainScale);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(overlay,MONITOR_DEFAULTTONEAREST),&monitor);int y=r.top-height-10;if(y<monitor.rcWork.top)y=r.bottom+10;
    strokeWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"ShuntieStroke",L"瞬贴 · 线宽",WS_POPUP,std::clamp(r.left,monitor.rcWork.left,monitor.rcWork.right-width),y,width,height,overlay,nullptr,instance,nullptr);roundWindow(strokeWindow);ShowWindow(strokeWindow,SW_SHOW);SetForegroundWindow(strokeWindow);
}
