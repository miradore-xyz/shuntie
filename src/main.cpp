#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <uxtheme.h>
#include "graphics.hpp"
#include "recorder.hpp"
#include "icons.hpp"
#include <fstream>

namespace {
constexpr unsigned BG=0x161A1F, INK=0xECF0F3, MUTED=0xA6AFB9, ACCENT=0x7CE6C5;
constexpr UINT TRAY_MESSAGE=WM_APP+1;
HINSTANCE instance;
HWND mainWindow=nullptr,overlay=nullptr,recordPanel=nullptr,recordFrame=nullptr,settingsWindow=nullptr;
std::wstring root,outputDir,notice;
Recorder recorder;
HFONT uiFont=nullptr;
HICON appIcon=nullptr;
HICON smallIcon=nullptr;
NOTIFYICONDATAW tray{};
bool captureHotkey=false,pinHotkey=false,recordHotkey=false;
Image lastImage;
Image brandImage;
float mainScale=1;
int gifFps=30,waitingHotkey=0;
UINT keyModifiers[]={0,0,MOD_ALT},keyCodes[]={VK_F1,VK_F3,'G'};
enum Command {Capture=101,PinClipboard,Record,Folder,Hide,Exit,CopyGif,OpenVideo,FrameRate,
    Select=201,Rectangle,Arrow,Pen,Mosaic,Color,Thickness,Undo,Redo,Copy,Save,Pin,Reselect,Cancel,StartRecord,StopRecord,
    Settings=301,Back,FpsToggle,AutoStart,KeyCapture,KeyPin,KeyRecord};
constexpr int ClearCapture=311,ClearPin=312,ClearRecord=313,PauseRecord=320,CopyRecording=321;
constexpr int Language=330;
void showMain();void beginCapture(bool recording);void pinImage(Image image);void finishCapture(int action);void refreshStatus();
void showSettings();void registerKeys();void mainCommand(int id);
void beginRecordingUI(RECT area);
bool isToolSelected(int id);
bool isEditorStrip(HWND button);
LRESULT CALLBACK hoverProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
void roundWindow(HWND hwnd);
HWND hoveredButton=nullptr,customTip=nullptr;
std::wstring customTipText;
float tipScale=1;

std::wstring timestamp(const wchar_t* suffix){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[128];swprintf(b,128,L"\\截图_%04d%02d%02d_%02d%02d%02d_%03d%s",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,suffix);return outputDir+b;}
void message(const std::wstring& value){notice=value;InvalidateRect(mainWindow,nullptr,FALSE);}
void showError(HWND owner,const std::wstring& value){auto translated=localize(value);MessageBoxW(owner,translated.c_str(),english?L"Shuntie":L"瞬贴",MB_OK|MB_ICONWARNING);}
HWND button(HWND parent,int id,const wchar_t* label,int x,int y,int w,int h){
    auto translated=localize(label);HWND b=CreateWindowExW(0,L"BUTTON",translated.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,x,y,w,h,parent,reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);
    SendMessageW(b,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont),TRUE);SetWindowSubclass(b,hoverProc,1,0);return b;
}
void drawButton(DRAWITEMSTRUCT* d){
    bool disabled=(d->itemState&ODS_DISABLED)!=0,down=(d->itemState&ODS_SELECTED)!=0;
    bool inStrip=isEditorStrip(d->hwndItem);
    bool primary=d->CtlID==CopyGif||(!inStrip&&(d->CtlID==Copy||d->CtlID==StartRecord));
    COLORREF bg=disabled?RGB(28,33,39):primary?(down?RGB(87,184,153):RGB(124,230,197)):(down?RGB(53,61,70):RGB(36,42,50));
    HBRUSH brush=CreateSolidBrush(bg);HPEN pen=CreatePen(PS_SOLID,1,bg);HGDIOBJ oldBrush=SelectObject(d->hDC,brush),oldPen=SelectObject(d->hDC,pen);
    RoundRect(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom,10,10);
    SelectObject(d->hDC,oldBrush);SelectObject(d->hDC,oldPen);DeleteObject(brush);DeleteObject(pen);
    Icon icon=Icon::scan_line;bool hasIcon=true,iconOnly=d->CtlID==Settings||d->CtlID==Back||inStrip||(d->CtlID>=ClearCapture&&d->CtlID<=ClearRecord)||GetParent(d->hwndItem)==recordPanel||(GetParent(d->hwndItem)==overlay&&d->CtlID!=StartRecord);
    switch(d->CtlID){case Settings:case FpsToggle:case Thickness:icon=Icon::sliders_horizontal;break;case Back:case Cancel:case Exit:icon=Icon::x;break;case Capture:case KeyCapture:case Reselect:icon=Icon::scan_line;break;case PinClipboard:case KeyPin:icon=Icon::clipboard_paste;break;case Record:case KeyRecord:case OpenVideo:case StartRecord:icon=Icon::video;break;case Copy:case CopyGif:icon=Icon::copy;break;case Folder:icon=Icon::folder_open;break;case Hide:icon=Icon::minus;break;case Select:icon=Icon::mouse_pointer_2;break;case Rectangle:icon=Icon::square;break;case Arrow:icon=Icon::move_up_right;break;case Pen:icon=Icon::pencil;break;case Mosaic:icon=Icon::grid_2x2;break;case Color:icon=Icon::palette;break;case Undo:icon=Icon::undo_2;break;case Redo:icon=Icon::redo_2;break;case Save:icon=Icon::save;break;case Pin:icon=Icon::pin;break;case StopRecord:icon=Icon::circle_stop;break;default:hasIcon=false;}
    if(d->CtlID>=ClearCapture&&d->CtlID<=ClearRecord){icon=Icon::x;hasIcon=true;}if(d->CtlID==CopyRecording){icon=Icon::copy;hasIcon=true;}if(d->CtlID==PauseRecord){icon=recorder.status().phase==RecordingStatus::Paused?Icon::play:Icon::pause;hasIcon=true;}
    float scale=float(GetDpiForWindow(d->hwndItem))/96,iw=(iconOnly?21:18)*scale,h=float(d->rcItem.bottom-d->rcItem.top),w=float(d->rcItem.right-d->rcItem.left);
    ID2D1DCRenderTarget* rt=nullptr;auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);d2d->CreateDCRenderTarget(&properties,&rt);
    if(rt){rt->BindDC(d->hDC,&d->rcItem);rt->BeginDraw();rt->Clear(rgb(inStrip?0x20262E:BG));unsigned fg=disabled?0x73808D:primary?0x11251E:inStrip&&(isToolSelected(d->CtlID)||d->CtlID==Copy)?ACCENT:INK;
        ID2D1SolidColorBrush* surface=nullptr;bool hover=hoveredButton==d->hwndItem;unsigned background=inStrip?(down?0x353D46:hover?0x2D3641:0x20262E):disabled?0x1C2127:primary?(down?0x57B899:ACCENT):(down?0x353D46:hover?0x303943:0x242A32);rt->CreateSolidColorBrush(rgb(background),&surface);if(surface){rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0,0,w,h),7*scale,7*scale),surface);release(surface);}
        if(hasIcon)drawIcon(rt,icon,iconOnly?(w-iw)/2:12*scale,(h-iw)/2,iw,fg);
        if(!iconOnly){wchar_t label[128];GetWindowTextW(d->hwndItem,label,128);IDWriteTextFormat* format=nullptr;ID2D1SolidColorBrush* b=nullptr;writeFactory->CreateTextFormat(L"Microsoft YaHei UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13*scale,L"zh-cn",&format);rt->CreateSolidColorBrush(rgb(fg),&b);if(format&&b){format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);rt->DrawText(label,UINT(wcslen(label)),format,D2D1::RectF(0,0,w,h),b);}release(format);release(b);}
        if((d->itemState&ODS_FOCUS)&&!(d->itemState&ODS_NOFOCUSRECT)){rt->CreateSolidColorBrush(rgb(ACCENT),&surface);if(surface){rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(2,2,w-2,h-2),6,6),surface,1);release(surface);}}
        rt->EndDraw();release(rt);
    }
}
ID2D1HwndRenderTarget* createTarget(HWND hwnd){RECT r;GetClientRect(hwnd,&r);ID2D1HwndRenderTarget* rt=nullptr;
    d2d->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,D2D1_ALPHA_MODE_UNKNOWN),96,96),D2D1::HwndRenderTargetProperties(hwnd,D2D1::SizeU(r.right,r.bottom)),&rt);return rt;}
#include "tooltips.inl"

HICON makeIcon(){
    HICON embedded=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,64,64,0));if(embedded)return embedded;
    HICON loaded=static_cast<HICON>(LoadImageW(nullptr,(root+L"\\app.ico").c_str(),IMAGE_ICON,64,64,LR_LOADFROMFILE));if(loaded)return loaded;
    HDC dc=GetDC(nullptr),mem=CreateCompatibleDC(dc);HBITMAP bm=CreateCompatibleBitmap(dc,32,32),mask=CreateBitmap(32,32,1,1,nullptr);HGDIOBJ old=SelectObject(mem,bm);
    RECT r{0,0,32,32};HBRUSH brush=CreateSolidBrush(RGB(0,110,102));FillRect(mem,&r,brush);DeleteObject(brush);
    HPEN pen=CreatePen(PS_SOLID,2,RGB(255,255,255));HGDIOBJ p=SelectObject(mem,pen),b=SelectObject(mem,GetStockObject(NULL_BRUSH));::Rectangle(mem,7,7,22,21);::Rectangle(mem,13,13,27,27);
    SelectObject(mem,p);SelectObject(mem,b);DeleteObject(pen);SelectObject(mem,old);DeleteDC(mem);ReleaseDC(nullptr,dc);
    ICONINFO info{TRUE,0,0,mask,bm};HICON icon=CreateIconIndirect(&info);DeleteObject(bm);DeleteObject(mask);return icon;
}

// 贴图窗口各自持有图像、缩放和透明度，彼此独立。
struct PinState {Image image;ID2D1HwndRenderTarget* rt=nullptr;ID2D1Bitmap* bitmap=nullptr;float scale=1;BYTE alpha=255;~PinState(){release(bitmap);release(rt);}};
LRESULT CALLBACK pinProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    auto p=reinterpret_cast<PinState*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){p=static_cast<PinState*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
    switch(msg){
    case WM_PAINT:{PAINTSTRUCT paint;BeginPaint(hwnd,&paint);if(p){if(!p->rt){p->rt=createTarget(hwnd);if(p->rt)p->bitmap=bitmapFor(p->rt,p->image);}if(p->rt){RECT r;GetClientRect(hwnd,&r);p->rt->BeginDraw();p->rt->Clear(rgb(0x161A1F));if(p->bitmap)p->rt->DrawBitmap(p->bitmap,D2D1::RectF(4,4,float(r.right-4),float(r.bottom-4)));ID2D1SolidColorBrush* border=nullptr;p->rt->CreateSolidColorBrush(rgb(ACCENT,.85f),&border);if(border){p->rt->DrawRectangle(D2D1::RectF(1.5f,1.5f,float(r.right)-1.5f,float(r.bottom)-1.5f),border,1.5f);release(border);}if(FAILED(p->rt->EndDraw())){release(p->bitmap);release(p->rt);}}}EndPaint(hwnd,&paint);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:if(p&&p->rt)p->rt->Resize(D2D1::SizeU(LOWORD(lp),HIWORD(lp)));InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;
    case WM_MOUSEWHEEL:if(p){int delta=GET_WHEEL_DELTA_WPARAM(wp);if(GetKeyState(VK_MENU)<0){p->alpha=BYTE(std::clamp(int(p->alpha)+(delta>0?16:-16),40,255));SetLayeredWindowAttributes(hwnd,0,p->alpha,LWA_ALPHA);}else{
        RECT r;GetWindowRect(hwnd,&r);float old=p->scale;p->scale=std::clamp(p->scale*std::pow(1.1f,float(delta)/120),0.1f,5.f);
        int w=std::max(20,int(p->image.width*p->scale))+8,h=std::max(20,int(p->image.height*p->scale))+8;
        int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);float ratio=p->scale/old;SetWindowPos(hwnd,nullptr,x-int((x-r.left)*ratio),y-int((y-r.top)*ratio),w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    }}return 0;
    case WM_KEYDOWN:if(wp==VK_ESCAPE){DestroyWindow(hwnd);return 0;}if(wp=='C'&&GetKeyState(VK_CONTROL)<0&&p){copyImage(hwnd,p->image);return 0;}if(wp=='0'&&p){p->scale=1;SetWindowPos(hwnd,nullptr,0,0,p->image.width+8,p->image.height+8,SWP_NOMOVE|SWP_NOZORDER);return 0;}break;
    case WM_RBUTTONUP:case WM_NCRBUTTONUP:DestroyWindow(hwnd);return 0;
    case WM_NCDESTROY:delete p;SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);break;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
void pinImage(Image image){
    if(!image.valid()){showError(mainWindow,L"剪贴板中没有可用的图片。请先截图，或复制一张图片。");return;}
    auto p=std::make_unique<PinState>();p->image=std::move(image);RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    p->scale=std::min(1.f,std::min(float(work.right-work.left)*.75f/p->image.width,float(work.bottom-work.top)*.75f/p->image.height));int w=int(p->image.width*p->scale)+8,h=int(p->image.height*p->scale)+8;
    HWND hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TOOLWINDOW,L"ShuntiePin",L"瞬贴 · 滚轮缩放 / Alt+滚轮透明度 / 右键关闭",WS_POPUP,(work.right-w)/2,(work.bottom-h)/2,w,h,nullptr,nullptr,instance,p.get());
    if(hwnd){p.release();SetLayeredWindowAttributes(hwnd,0,255,LWA_ALPHA);ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);}else showError(mainWindow,L"无法创建贴图窗口。");
}

// 截图编辑器：对象列表与撤销历史和屏幕背景分开保存。
struct Editor {
    Image screen;RECT desktop{};snip::Box selection{};snip::Tool tool=snip::Tool::Rectangle;
    std::vector<snip::Shape> shapes;std::vector<std::vector<snip::Shape>> past,future;
    bool selected=false,recording=false,dragging=false,selecting=false,moving=false,resizing=false,rotating=false;
    float initialRotation=0,initialPolar=0;
    int active=-1,colorIndex=0,widthIndex=0;snip::Point start,last;float stroke=3;
    ID2D1HwndRenderTarget* rt=nullptr;ID2D1Bitmap* background=nullptr;
    std::vector<HWND> toolbar;HWND hint=nullptr,tips=nullptr;std::vector<std::wstring> tipText;RECT toolbarBounds{};
    ~Editor(){release(background);release(rt);}
    void checkpoint(){past.push_back(shapes);if(past.size()>60)past.erase(past.begin());future.clear();}
    void history(bool redo){auto& from=redo?future:past;auto& to=redo?past:future;if(from.empty())return;to.push_back(shapes);shapes=from.back();from.pop_back();active=-1;}
} editor;
bool isEditorStrip(HWND b){return GetParent(b)==overlay&&!editor.recording;}
bool isToolSelected(int id){return overlay&&!editor.recording&&((id==Select&&editor.tool==snip::Tool::Select)||(id==Rectangle&&editor.tool==snip::Tool::Rectangle)||(id==Arrow&&editor.tool==snip::Tool::Arrow)||(id==Pen&&editor.tool==snip::Tool::Pen)||(id==Mosaic&&editor.tool==snip::Tool::Mosaic));}
const unsigned colors[]={0xFF6554,0xFF9D57,0xFFCF4A,0x7CE6C5,0x3478F6,0xA58BFA,0xF472B6,0xFFFFFF,0xAFB7C1,0x64748B,0x007F78,0x182B35};
HWND paletteWindow=nullptr;
LRESULT CALLBACK paletteProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_PAINT:{PAINTSTRUCT paint;BeginPaint(hwnd,&paint);auto rt=createTarget(hwnd);if(rt){rt->BeginDraw();rt->Clear(rgb(0x20262E));rt->SetTransform(D2D1::Matrix3x2F::Scale(mainScale,mainScale));text(rt,L"选择颜色",{14,8,246,33},13,INK);for(int i=0;i<12;++i){float x=14+(i%6)*38.f,y=42+(i/6)*38.f;fill(rt,{x,y,x+28,y+28},colors[i]);if(i==editor.colorIndex){ID2D1SolidColorBrush* border=nullptr;rt->CreateSolidColorBrush(rgb(ACCENT),&border);if(border){rt->DrawRectangle(rect({x-3,y-3,x+31,y+31}),border,2);release(border);}}}rt->EndDraw();release(rt);}EndPaint(hwnd,&paint);return 0;}
    case WM_LBUTTONUP:{float x=float(GET_X_LPARAM(lp))/mainScale,y=float(GET_Y_LPARAM(lp))/mainScale;int col=int((x-14)/38),row=int((y-42)/38);if(x>=14&&y>=42&&col<6&&row<2&&col>=0&&row>=0){int i=row*6+col;editor.colorIndex=i;if(editor.active>=0){editor.checkpoint();editor.shapes[editor.active].color=colors[i];}wchar_t label[64];swprintf(label,64,L"颜色：#%06X",colors[i]);setLabel(GetDlgItem(overlay,Color),label);DestroyWindow(hwnd);SetFocus(overlay);InvalidateRect(overlay,nullptr,FALSE);}return 0;}
    case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE)DestroyWindow(hwnd);return 0;
    case WM_KEYDOWN:if(wp==VK_ESCAPE){DestroyWindow(hwnd);SetFocus(overlay);return 0;}break;
    case WM_DESTROY:paletteWindow=nullptr;return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
void showPalette(){if(paletteWindow){DestroyWindow(paletteWindow);return;}RECT r;GetWindowRect(GetDlgItem(overlay,Color),&r);int width=int(256*mainScale),height=int(126*mainScale);RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int y=r.bottom+8;if(y+height>work.bottom)y=r.top-height-8;paletteWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"ShuntiePalette",L"瞬贴 · 选色卡",WS_POPUP,std::clamp(r.left,work.left,work.right-width),y,width,height,overlay,nullptr,instance,nullptr);roundWindow(paletteWindow);ShowWindow(paletteWindow,SW_SHOW);SetForegroundWindow(paletteWindow);}
#include "stroke_popup.inl"

void layoutToolbar(){
    int total=editor.recording?3:int(editor.toolbar.size()),gap=int(4*mainScale),h=int(40*mainScale),w=editor.recording?int(120*mainScale):int(40*mainScale);
    int separator=editor.recording?0:int(8*mainScale),panelW=total*(w+gap)+gap+4*separator;int left=std::clamp(int((editor.selection.l+editor.selection.r)/2)-panelW/2,10,std::max(10,editor.screen.width-panelW-10));
    int top=int(editor.selection.b)+12;if(top+h+32>editor.screen.height)top=std::max(8,int(editor.selection.t)-h-40);
    editor.toolbarBounds={left-gap,top-gap,left+panelW+gap,top+h+gap};
    int offset=gap;for(int i=0;i<int(editor.toolbar.size());++i){SetWindowPos(editor.toolbar[i],nullptr,left+offset,top,w,h,SWP_NOZORDER|SWP_SHOWWINDOW);offset+=w+gap;if(i==4||i==6||i==8||i==12)offset+=separator;}
    if(editor.hint)ShowWindow(editor.hint,SW_HIDE);
}
void toolHint(){
    if(!editor.hint)return;std::wstring value;
    if(editor.recording)value=L"框选单个显示器内的区域 · 录制不包含声音";
    else if(editor.tool==snip::Tool::Select)value=L"拖动对象或右下角控制点 · Delete 删除 · Ctrl+Z 撤销";
    else value=L"当前工具可连续绘制 · V 选择编辑 · Ctrl+Z 撤销 · Enter 复制 · Esc 取消";
    setLabel(editor.hint,value.c_str());
}
void createToolbar(){
    if(!editor.toolbar.empty()){layoutToolbar();return;}
    if(editor.recording){editor.toolbar.push_back(button(overlay,StartRecord,L"开始录制",0,0,0,0));editor.toolbar.push_back(button(overlay,Reselect,L"重新框选",0,0,0,0));editor.toolbar.push_back(button(overlay,Cancel,L"取消",0,0,0,0));}
    else{
        const std::pair<int,const wchar_t*> entries[]={{Select,L"选择（V）"},{Rectangle,L"方框（R）"},{Arrow,L"箭头（A）"},{Pen,L"画笔（P）"},{Mosaic,L"马赛克（M）"},{Color,L"颜色：红色"},{Thickness,L"线宽：3"},{Undo,L"撤销（Ctrl+Z）"},{Redo,L"重做（Ctrl+Y）"},{Copy,L"复制截图（Enter）"},{Save,L"保存（Ctrl+S）"},{Pin,L"贴图"},{StartRecord,L"录制选区"},{Reselect,L"重新框选"},{Cancel,L"取消（Esc）"}};
        for(auto e:entries)editor.toolbar.push_back(button(overlay,e.first,e.second,0,0,0,0));
    }
    editor.hint=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|SS_LEFT,0,0,0,0,overlay,nullptr,instance,nullptr);SendMessageW(editor.hint,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont),TRUE);
    layoutToolbar();toolHint();
}
void paintEditor(HWND hwnd){
    PAINTSTRUCT paint;BeginPaint(hwnd,&paint);
    if(!editor.rt){editor.rt=createTarget(hwnd);if(editor.rt)editor.background=bitmapFor(editor.rt,editor.screen);}
    auto rt=editor.rt;if(rt){rt->BeginDraw();rt->Clear(rgb(INK));if(editor.background)rt->DrawBitmap(editor.background);
        auto s=editor.selection;float sw=float(editor.screen.width),sh=float(editor.screen.height);
        if(editor.selected||editor.dragging){
            fill(rt,{0,0,sw,s.t},0,0.52f);fill(rt,{0,s.t,s.l,s.b},0,.52f);fill(rt,{s.r,s.t,sw,s.b},0,.52f);fill(rt,{0,s.b,sw,sh},0,.52f);
            rt->PushAxisAlignedClip(rect(s),D2D1_ANTIALIAS_MODE_ALIASED);for(auto& shape:editor.shapes)renderShape(rt,shape,editor.screen);rt->PopAxisAlignedClip();
            ID2D1SolidColorBrush* brush=nullptr;rt->CreateSolidColorBrush(rgb(0x40DFCB),&brush);if(brush){rt->DrawRectangle(rect(s),brush,2);release(brush);}
            std::wstring size=std::to_wstring(int(s.width()))+L" × "+std::to_wstring(int(s.height()));
            float labelY=s.t>=34?s.t-32:s.t+6;fill(rt,{s.l,labelY,s.l+150,labelY+28},0x20262E,.98f);text(rt,size,{s.l+8,labelY+2,s.l+148,labelY+28},17,0xECF0F3);
            if(editor.active>=0&&editor.active<int(editor.shapes.size())&&(!editor.dragging||editor.moving||editor.resizing||editor.rotating)){auto& shape=editor.shapes[editor.active];auto b=shape.rawBounds();auto resize=shape.resizeHandle(),rotate=shape.rotateHandle(28*mainScale),top=shape.toWorld({(b.l+b.r)/2,b.t});
                rt->CreateSolidColorBrush(rgb(0xB4C4CC),&brush);if(brush){rt->DrawLine(pt(top),pt(rotate),brush,1);brush->SetColor(rgb(ACCENT));rt->FillEllipse(D2D1::Ellipse(pt(resize),3.5f*mainScale,3.5f*mainScale),brush);rt->DrawEllipse(D2D1::Ellipse(pt(rotate),4*mainScale,4*mainScale),brush,1.5f);release(brush);}}
        }else{fill(rt,{0,0,sw,sh},0,.42f);fill(rt,{sw/2-280,70,sw/2+280,160},0x20262E,.98f);text(rt,editor.recording?L"框选录制区域":L"拖动鼠标，截取一块屏幕",{sw/2-255,84,sw/2+255,125},25,0xFFFFFF,true);text(rt,L"按 Esc 取消 · 支持跨屏截图",{sw/2-255,127,sw/2+255,158},16,0xBDD4D8);}
        if(editor.selected&&!editor.toolbar.empty()){auto b=editor.toolbarBounds;ID2D1SolidColorBrush* bar=nullptr;rt->CreateSolidColorBrush(rgb(0x20262E),&bar);if(bar){rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(float(b.left),float(b.top),float(b.right),float(b.bottom)),10*mainScale,10*mainScale),bar);if(!editor.recording){bar->SetColor(rgb(0x46515E));for(int index:{4,6,8,12}){RECT r;GetWindowRect(editor.toolbar[index],&r);int x=r.right-editor.desktop.left+int(6*mainScale);rt->DrawLine({float(x),float(b.top)+12*mainScale},{float(x),float(b.bottom)-12*mainScale},bar,1);}}release(bar);}}
        if(FAILED(rt->EndDraw())){release(editor.background);release(editor.rt);}
    }EndPaint(hwnd,&paint);
}
void selectTool(snip::Tool tool){editor.tool=tool;editor.active=-1;toolHint();for(HWND b:editor.toolbar)InvalidateRect(b,nullptr,FALSE);InvalidateRect(overlay,nullptr,FALSE);}
void editorCommand(int id){
    switch(id){
    case Select:selectTool(snip::Tool::Select);break;case Rectangle:selectTool(snip::Tool::Rectangle);break;case Arrow:selectTool(snip::Tool::Arrow);break;case Pen:selectTool(snip::Tool::Pen);break;case Mosaic:selectTool(snip::Tool::Mosaic);break;
    case Color:showPalette();return;
    case Thickness:showStroke();return;
    case Undo:editor.history(false);break;case Redo:editor.history(true);break;
    case Copy:case Save:case Pin:case StartRecord:finishCapture(id);return;
    case Reselect:editor.selected=false;editor.shapes.clear();editor.past.clear();editor.future.clear();editor.active=-1;for(HWND b:editor.toolbar)ShowWindow(b,SW_HIDE);ShowWindow(editor.hint,SW_HIDE);break;
    case Cancel:DestroyWindow(overlay);return;
    }
    if(overlay){SetFocus(overlay);InvalidateRect(overlay,nullptr,FALSE);}
}
LRESULT CALLBACK overlayProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    snip::Point p{float(GET_X_LPARAM(lp)),float(GET_Y_LPARAM(lp))};
    switch(msg){
    case WM_PAINT:paintEditor(hwnd);return 0;case WM_ERASEBKGND:return 1;
    case WM_DRAWITEM:drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_NOTIFY:if(reinterpret_cast<NMHDR*>(lp)->code==TTN_GETDISPINFOW){auto tip=reinterpret_cast<NMTTDISPINFOW*>(lp);static wchar_t title[160];GetWindowTextW(reinterpret_cast<HWND>(tip->hdr.idFrom),title,160);tip->lpszText=title;return 0;}break;
    case WM_CTLCOLORSTATIC:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,RGB(255,255,255));SetBkColor(dc,RGB(24,43,53));static HBRUSH brush=CreateSolidBrush(RGB(24,43,53));return reinterpret_cast<LRESULT>(brush);}
    case WM_COMMAND:editorCommand(LOWORD(wp));return 0;
    case WM_LBUTTONDOWN:{SetFocus(hwnd);
        editor.last=p;editor.start=p;editor.dragging=true;editor.moving=false;editor.resizing=false;editor.rotating=false;editor.selecting=!editor.selected;
        if(editor.selecting)editor.selection=snip::normalized(p,p);
        else if(editor.selection.contains(p)&&!editor.recording){
            if(editor.tool==snip::Tool::Select){
                bool handle=false,rotationHandle=false;if(editor.active>=0){auto& shape=editor.shapes[editor.active];auto resize=shape.resizeHandle(),rotate=shape.rotateHandle(28*mainScale);handle=std::hypot(p.x-resize.x,p.y-resize.y)<12*mainScale;rotationHandle=std::hypot(p.x-rotate.x,p.y-rotate.y)<12*mainScale;}
                if(rotationHandle){editor.checkpoint();editor.rotating=true;auto& shape=editor.shapes[editor.active];auto c=shape.center();editor.initialRotation=shape.rotation;editor.initialPolar=std::atan2(p.y-c.y,p.x-c.x);}
                else if(handle){editor.checkpoint();editor.resizing=true;}
                else{editor.active=-1;for(int i=int(editor.shapes.size())-1;i>=0;--i)if(snip::hit(editor.shapes[i],p)){editor.active=i;break;}
                    if(editor.active>=0){editor.checkpoint();editor.moving=true;}}
            }else{editor.checkpoint();snip::Shape s;s.tool=editor.tool;s.a=p;s.b=p;s.color=colors[editor.colorIndex];s.stroke=editor.stroke;if(s.tool==snip::Tool::Pen)s.points.push_back(p);editor.shapes.push_back(s);editor.active=int(editor.shapes.size())-1;}
        }else editor.dragging=false;
        if(editor.dragging)SetCapture(hwnd);InvalidateRect(hwnd,nullptr,FALSE);return 0;}
    case WM_MOUSEMOVE:if(editor.dragging){
        p.x=std::clamp(p.x,0.f,float(editor.screen.width));p.y=std::clamp(p.y,0.f,float(editor.screen.height));
        if(editor.selecting)editor.selection=snip::normalized(editor.start,p);
        else if(editor.active>=0){p.x=std::clamp(p.x,editor.selection.l,editor.selection.r);p.y=std::clamp(p.y,editor.selection.t,editor.selection.b);auto& s=editor.shapes[editor.active];
            if(editor.moving){auto b=s.bounds();float dx=b.width()<=editor.selection.width()?std::clamp(p.x-editor.last.x,editor.selection.l-b.l,editor.selection.r-b.r):0,dy=b.height()<=editor.selection.height()?std::clamp(p.y-editor.last.y,editor.selection.t-b.t,editor.selection.b-b.b):0;s.move(dx,dy);}
            else if(editor.rotating){auto c=s.center();s.rotation=editor.initialRotation+(std::atan2(p.y-c.y,p.x-c.x)-editor.initialPolar)*57.29577951f;if(GetKeyState(VK_SHIFT)<0)s.rotation=std::round(s.rotation/15)*15;}
            else if(editor.resizing){s.resizeTo(p);}else if(editor.tool!=snip::Tool::Select){s.b=p;if(s.tool==snip::Tool::Pen)s.points.push_back(p);}
        }editor.last=p;InvalidateRect(hwnd,nullptr,FALSE);
    }return 0;
    case WM_LBUTTONUP:if(editor.dragging){ReleaseCapture();editor.dragging=false;
        if(editor.selecting){if(editor.selection.width()<=6&&editor.selection.height()<=6){editor.selection={0,0,float(editor.screen.width),float(editor.screen.height)};}if(editor.selection.width()>6&&editor.selection.height()>6){editor.selected=true;createToolbar();}else editor.selected=false;}
        else if(editor.active>=0&&!editor.moving&&!editor.resizing&&editor.tool!=snip::Tool::Select){auto b=editor.shapes[editor.active].bounds();if(b.width()<2&&b.height()<2){editor.shapes.pop_back();editor.active=-1;}else{editor.tool=snip::Tool::Select;toolHint();for(HWND button:editor.toolbar)InvalidateRect(button,nullptr,FALSE);}}
        InvalidateRect(hwnd,nullptr,FALSE);
    }return 0;
    case WM_RBUTTONUP:editorCommand(Cancel);return 0;
    case WM_SETCURSOR:if(editor.selected){POINT pos;GetCursorPos(&pos);ScreenToClient(hwnd,&pos);snip::Point point{float(pos.x),float(pos.y)};if(editor.active>=0){auto& shape=editor.shapes[editor.active];auto resize=shape.resizeHandle(),rotate=shape.rotateHandle(28*mainScale);if(std::hypot(point.x-rotate.x,point.y-rotate.y)<12*mainScale){SetCursor(LoadCursorW(nullptr,IDC_CROSS));return TRUE;}if(std::hypot(point.x-resize.x,point.y-resize.y)<12*mainScale){SetCursor(LoadCursorW(nullptr,IDC_SIZENWSE));return TRUE;}}if(editor.tool==snip::Tool::Select){for(auto& s:editor.shapes)if(snip::hit(s,point)){SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));return TRUE;}}}break;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE){editorCommand(Cancel);return 0;}
        if(wp==VK_RETURN&&editor.selected){finishCapture(editor.recording?StartRecord:Copy);return 0;}
        if(GetKeyState(VK_CONTROL)<0){if(wp=='Z'){editor.history(GetKeyState(VK_SHIFT)<0);InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp=='Y'){editorCommand(Redo);return 0;}if(wp=='C'&&editor.selected){finishCapture(Copy);return 0;}if(wp=='S'&&editor.selected){finishCapture(Save);return 0;}}
        if(wp==VK_DELETE&&editor.active>=0){editor.checkpoint();editor.shapes.erase(editor.shapes.begin()+editor.active);editor.active=-1;InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        if(!editor.recording){if(wp=='V')selectTool(snip::Tool::Select);if(wp=='R')selectTool(snip::Tool::Rectangle);if(wp=='A')selectTool(snip::Tool::Arrow);if(wp=='P')selectTool(snip::Tool::Pen);if(wp=='M')selectTool(snip::Tool::Mosaic);}return 0;
    case WM_DESTROY:overlay=nullptr;release(editor.background);release(editor.rt);editor.toolbar.clear();editor.hint=nullptr;editor.tips=nullptr;editor.tipText.clear();return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
void beginCapture(bool recording){
    if(overlay){SetForegroundWindow(overlay);return;}if(recording&&recorder.busy()){showMain();return;}
    ShowWindow(mainWindow,SW_HIDE);Sleep(160);DwmFlush();
    editor.screen={};release(editor.background);release(editor.rt);editor.shapes.clear();editor.past.clear();editor.future.clear();editor.active=-1;editor.selected=false;editor.recording=recording;editor.dragging=false;editor.tool=snip::Tool::Rectangle;editor.toolbar.clear();editor.hint=nullptr;
    int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);editor.desktop={x,y,x+w,y+h};editor.screen=captureDesktop(editor.desktop);
    if(!editor.screen.valid()){showMain();showError(mainWindow,L"无法读取屏幕。请确认桌面没有锁定。");return;}
    overlay=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"ShuntieEditor",recording?L"瞬贴 · 框选录屏":L"瞬贴 · 截图标注",WS_POPUP|WS_CLIPCHILDREN,x,y,w,h,nullptr,nullptr,instance,nullptr);
    if(!overlay){showMain();return;}ShowWindow(overlay,SW_SHOW);SetForegroundWindow(overlay);SetFocus(overlay);
}
bool sameMonitor(RECT area){POINT a{area.left,area.top},b{area.right-1,area.bottom-1};return MonitorFromPoint(a,MONITOR_DEFAULTTONULL)==MonitorFromPoint(b,MONITOR_DEFAULTTONULL);}
void finishCapture(int action){
    if(!editor.selected||!overlay)return;
    if(action==StartRecord){
        RECT area{LONG(editor.selection.l)+editor.desktop.left,LONG(editor.selection.t)+editor.desktop.top,LONG(editor.selection.r)+editor.desktop.left,LONG(editor.selection.b)+editor.desktop.top};
        if(!sameMonitor(area)){showError(overlay,L"录屏首版需要在单个显示器内框选；截图可以跨屏。");return;}
        int fps=gifFps;
        DestroyWindow(overlay);ShowWindow(mainWindow,SW_HIDE);recorder.start(area,root,outputDir,fps);beginRecordingUI(area);return;
    }
    Image im=flatten(editor.screen,editor.selection,editor.shapes);if(!im.valid()){showError(overlay,L"截图生成失败，请重试。");return;}
    if(action==Copy){if(!copyImage(overlay,im)){showError(overlay,L"剪贴板暂时被其他程序占用，请再试一次。");return;}message(L"截图已复制到剪贴板。");}
    if(action==Save){
        std::wstring initial=timestamp(L".png");wchar_t file[32768]{};wcsncpy(file,initial.c_str(),32767);OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=overlay;dialog.lpstrFilter=english?L"PNG image\0*.png\0\0":L"PNG 图片\0*.png\0\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrDefExt=L"png";dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetSaveFileNameW(&dialog))return;if(!savePng(im,file)){showError(overlay,L"保存失败，请检查目标目录。");return;}message(L"截图已保存为 PNG。");
    }
    lastImage=im;DestroyWindow(overlay);if(action==Pin){copyImage(mainWindow,im);pinImage(std::move(im));message(L"贴图已置顶 · 滚轮缩放 · Alt+滚轮调透明度 · 右键关闭");}
}

void showMain(){ShowWindow(mainWindow,SW_SHOW);SetForegroundWindow(mainWindow);InvalidateRect(mainWindow,nullptr,FALSE);}
void roundWindow(HWND hwnd){RECT r;GetClientRect(hwnd,&r);int radius=int(22*float(GetDpiForWindow(hwnd))/96);SetWindowRgn(hwnd,CreateRoundRectRgn(0,0,r.right+1,r.bottom+1,radius,radius),TRUE);}
std::wstring keyLabel(int i){
    if(!keyCodes[i])return L"未设置";
    std::wstring label;if(keyModifiers[i]&MOD_CONTROL)label+=L"Ctrl+";if(keyModifiers[i]&MOD_ALT)label+=L"Alt+";if(keyModifiers[i]&MOD_SHIFT)label+=L"Shift+";
    wchar_t key[64]{};UINT scan=MapVirtualKeyW(keyCodes[i],MAPVK_VK_TO_VSC);GetKeyNameTextW(LONG(scan<<16),key,64);return label+key;
}
void registerKeys(){
    bool* flags[]={&captureHotkey,&pinHotkey,&recordHotkey};std::wstring ini=root+L"\\settings.ini";
    for(int i=0;i<3;++i){UnregisterHotKey(mainWindow,i+1);*flags[i]=!keyCodes[i]||RegisterHotKey(mainWindow,i+1,keyModifiers[i]|MOD_NOREPEAT,keyCodes[i]);
        WritePrivateProfileStringW(L"shortcuts",(L"key"+std::to_wstring(i)).c_str(),std::to_wstring(keyCodes[i]).c_str(),ini.c_str());WritePrivateProfileStringW(L"shortcuts",(L"mod"+std::to_wstring(i)).c_str(),std::to_wstring(keyModifiers[i]).c_str(),ini.c_str());}
}
bool autoStartEnabled(){HKEY key=nullptr;bool enabled=false;if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_READ,&key)==ERROR_SUCCESS){enabled=RegQueryValueExW(key,L"Shuntie",nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;RegCloseKey(key);}return enabled;}
bool setAutoStart(bool enabled){HKEY key=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;LONG status;
    if(enabled){std::wstring command=L"\""+root+L"\\瞬贴.exe\" --tray";status=RegSetValueExW(key,L"Shuntie",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t)));}else{status=RegDeleteValueW(key,L"Shuntie");if(status==ERROR_FILE_NOT_FOUND)status=ERROR_SUCCESS;}RegCloseKey(key);return status==ERROR_SUCCESS;}
void settingsLabels(){
    if(!settingsWindow)return;for(int i=0;i<3;++i){const wchar_t* labels[]={L"截图  ",L"贴图  ",L"录屏  "};bool available=i==0?captureHotkey:i==1?pinHotkey:recordHotkey;setLabel(GetDlgItem(settingsWindow,KeyCapture+i),(labels[i]+(waitingHotkey==i+1?L"请按新的快捷键…":keyLabel(i)+(available?L"":L"（占用）"))).c_str());}
    setLabel(GetDlgItem(settingsWindow,FpsToggle),(L"GIF 帧率   "+std::to_wstring(gifFps)+L" 帧/秒").c_str());setLabel(GetDlgItem(settingsWindow,AutoStart),autoStartEnabled()?L"开机启动   已开启":L"开机启动   已关闭");InvalidateRect(settingsWindow,nullptr,FALSE);
}
LRESULT CALLBACK settingsProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    static ID2D1HwndRenderTarget* rt=nullptr;float s=mainScale;
    switch(msg){
    case WM_CREATE:{auto add=[&](int id,const wchar_t* label,int x,int y,int w,int h){button(hwnd,id,label,int(x*s),int(y*s),int(w*s),int(h*s));};add(Back,L"关闭设置",286,18,34,34);add(Language,english?L"简体中文":L"English",200,60,120,25);add(KeyCapture,L"截图",24,92,252,40);add(KeyPin,L"贴图",24,140,252,40);add(KeyRecord,L"录屏",24,188,252,40);add(ClearCapture,L"清除截图快捷键",284,92,36,40);add(ClearPin,L"清除贴图快捷键",284,140,36,40);add(ClearRecord,L"清除录屏快捷键",284,188,36,40);add(FpsToggle,L"GIF 帧率",24,252,296,40);add(AutoStart,L"开机启动",24,300,296,40);add(Folder,L"输出目录",24,364,140,38);add(Hide,L"收起",180,364,140,38);add(Exit,L"退出软件",24,418,296,38);return 0;}
    case WM_DRAWITEM:drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_COMMAND:{int id=LOWORD(wp);if(id==Language){english=!english;WritePrivateProfileStringW(L"ui",L"english",english?L"1":L"0",(root+L"\\settings.ini").c_str());EnumChildWindows(mainWindow,[](HWND control,LPARAM)->BOOL{wchar_t label[256]{};GetWindowTextW(control,label,256);setLabel(control,label);return TRUE;},0);setLabel(GetDlgItem(hwnd,Language),english?L"简体中文":L"English");settingsLabels();setLabel(mainWindow,L"瞬贴 · 截图与录屏");RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);return 0;}if(id>=ClearCapture&&id<=ClearRecord){int i=id-ClearCapture;keyCodes[i]=0;keyModifiers[i]=0;waitingHotkey=0;registerKeys();settingsLabels();return 0;}if(id>=KeyCapture&&id<=KeyRecord){waitingHotkey=id-KeyCapture+1;SetFocus(hwnd);settingsLabels();return 0;}if(id==Back){DestroyWindow(hwnd);return 0;}if(id==FpsToggle){gifFps=gifFps==30?60:30;WritePrivateProfileStringW(L"export",L"fps",std::to_wstring(gifFps).c_str(),(root+L"\\settings.ini").c_str());settingsLabels();return 0;}if(id==AutoStart){if(!setAutoStart(!autoStartEnabled()))showError(hwnd,L"开机启动设置未能保存。");settingsLabels();return 0;}if(id==Hide){DestroyWindow(hwnd);ShowWindow(mainWindow,SW_HIDE);return 0;}if(id==Exit){DestroyWindow(hwnd);SendMessageW(mainWindow,WM_CLOSE,0,0);return 0;}mainCommand(id);return 0;}
    case WM_SYSKEYDOWN:case WM_KEYDOWN:
        if(waitingHotkey){if(wp==VK_ESCAPE){waitingHotkey=0;settingsLabels();return 0;}if(wp==VK_CONTROL||wp==VK_SHIFT||wp==VK_MENU||wp==VK_LWIN||wp==VK_RWIN)return 0;
        int i=waitingHotkey-1;UINT mod=(GetKeyState(VK_CONTROL)<0?MOD_CONTROL:0)|(GetKeyState(VK_MENU)<0?MOD_ALT:0)|(GetKeyState(VK_SHIFT)<0?MOD_SHIFT:0);UINT oldKey=keyCodes[i],oldMod=keyModifiers[i];UnregisterHotKey(mainWindow,i+1);
            if(RegisterHotKey(mainWindow,i+1,mod|MOD_NOREPEAT,UINT(wp))){keyCodes[i]=UINT(wp);keyModifiers[i]=mod;waitingHotkey=0;registerKeys();settingsLabels();InvalidateRect(mainWindow,nullptr,FALSE);}
            else{RegisterHotKey(mainWindow,i+1,oldMod|MOD_NOREPEAT,oldKey);showError(hwnd,L"该快捷键已被其他程序占用，请换一个组合。");}return 0;}
        if(wp==VK_ESCAPE){DestroyWindow(hwnd);return 0;}break;
    case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(GetAncestor(hwnd,GA_ROOT),WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;
    case WM_WINDOWPOSCHANGING:if(GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_CHILD){auto p=reinterpret_cast<WINDOWPOS*>(lp);p->x=0;p->y=0;}return 0;
    case WM_SIZE:roundWindow(hwnd);if(rt)rt->Resize(D2D1::SizeU(LOWORD(lp),HIWORD(lp)));return 0;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd,&ps);if(!rt)rt=createTarget(hwnd);if(rt){rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Scale(s,s));rt->Clear(rgb(BG));text(rt,L"设置",{24,22,250,58},21,INK,true);text(rt,L"点击修改快捷键",{24,62,190,85},12,MUTED);if(FAILED(rt->EndDraw()))release(rt);}EndPaint(hwnd,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_DESTROY:settingsWindow=nullptr;waitingHotkey=0;release(rt);ShowWindow(GetDlgItem(mainWindow,Settings),SW_SHOW);refreshStatus();RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
void showSettings(){if(settingsWindow){SetFocus(settingsWindow);return;}SendMessageW(mainWindow,WM_SETREDRAW,FALSE,0);ShowWindow(GetDlgItem(mainWindow,Settings),SW_HIDE);ShowWindow(GetDlgItem(mainWindow,CopyGif),SW_HIDE);SetWindowPos(mainWindow,nullptr,0,0,int(344*mainScale),int(480*mainScale),SWP_NOMOVE|SWP_NOZORDER);settingsWindow=CreateWindowExW(0,L"ShuntieSettings",L"瞬贴 · 设置",WS_CHILD|WS_CLIPCHILDREN,0,0,int(344*mainScale),int(480*mainScale),mainWindow,nullptr,instance,nullptr);settingsLabels();ShowWindow(settingsWindow,SW_SHOW);SendMessageW(mainWindow,WM_SETREDRAW,TRUE,0);RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);SetFocus(settingsWindow);}
void layoutMain(HWND hwnd){
    float s=mainScale;MoveWindow(GetDlgItem(hwnd,Settings),int(286*s),int(20*s),int(34*s),int(34*s),TRUE);
    MoveWindow(GetDlgItem(hwnd,CopyGif),int(24*s),int(214*s),int(296*s),int(42*s),TRUE);
}
void refreshStatus(){
    static unsigned copiedVersion=0;auto latest=recorder.status();if(!latest.copyVersion)copiedVersion=0;if(latest.copyVersion>copiedVersion){if(copyFileToClipboard(mainWindow,latest.gif)){copiedVersion=latest.copyVersion;notice=L"GIF 已复制";}else notice=L"GIF 已生成，剪贴板繁忙，请重试复制";}
    auto s=recorder.status();bool completed=!s.gif.empty()&&s.phase==RecordingStatus::Complete;
    if(!settingsWindow)ShowWindow(GetDlgItem(mainWindow,CopyGif),completed?SW_SHOW:SW_HIDE);
    int height=completed?280:(s.phase==RecordingStatus::Failed?250:180);RECT current;GetWindowRect(mainWindow,&current);if(!settingsWindow&&current.bottom-current.top!=int(height*mainScale))SetWindowPos(mainWindow,nullptr,0,0,int(344*mainScale),int(height*mainScale),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    if(recordPanel){InvalidateRect(recordPanel,nullptr,FALSE);if(recordFrame)InvalidateRect(recordFrame,nullptr,FALSE);if(s.phase==RecordingStatus::Complete||s.phase==RecordingStatus::Failed||s.phase==RecordingStatus::Idle){DestroyWindow(recordPanel);recordPanel=nullptr;if(recordFrame){DestroyWindow(recordFrame);recordFrame=nullptr;}if(s.phase!=RecordingStatus::Idle)showMain();}}
    InvalidateRect(mainWindow,nullptr,FALSE);
}
void mainCommand(int id){
    switch(id){
    case Settings:showSettings();break;
    case Capture:beginCapture(false);break;case PinClipboard:pinImage(clipboardImage(mainWindow));break;
    case Record:if(recorder.busy())recorder.stop();else beginCapture(true);break;
    case CopyGif:{auto s=recorder.status();if(copyFileToClipboard(mainWindow,s.gif))message(L"GIF 动图文件已复制，可粘贴到支持文件的聊天或编辑软件。");else showError(mainWindow,L"复制失败，请稍后重试。");break;}
    case OpenVideo:{auto s=recorder.status();if(!s.video.empty())ShellExecuteW(mainWindow,L"open",s.video.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;}
    case Folder:ShellExecuteW(mainWindow,L"open",outputDir.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
    case Hide:ShowWindow(mainWindow,SW_HIDE);break;case Exit:SendMessageW(mainWindow,WM_CLOSE,0,0);break;
    }
}
LRESULT CALLBACK mainProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    static ID2D1HwndRenderTarget* rt=nullptr;
    static ID2D1Bitmap* logo=nullptr;
    switch(msg){
    case WM_CREATE:{
        button(hwnd,Settings,L"设置",0,0,0,0);button(hwnd,CopyGif,L"复制 GIF",0,0,0,0);ShowWindow(GetDlgItem(hwnd,CopyGif),SW_HIDE);SetTimer(hwnd,1,400,nullptr);layoutMain(hwnd);return 0;}
    case WM_SIZE:roundWindow(hwnd);if(rt)rt->Resize(D2D1::SizeU(LOWORD(lp),HIWORD(lp)));return 0;
    case WM_DPICHANGED:mainScale=float(HIWORD(wp))/96;{RECT* r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);layoutMain(hwnd);}return 0;
    case WM_PAINT:{PAINTSTRUCT paint;BeginPaint(hwnd,&paint);if(!rt)rt=createTarget(hwnd);if(rt){rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Scale(mainScale,mainScale));rt->Clear(rgb(BG));
        // 统一使用嵌入的高清 Logo，图标资源包含 16 到 256 像素尺寸。
        if(!logo)logo=bitmapFor(rt,brandImage,true);if(logo)rt->DrawBitmap(logo,D2D1::RectF(20,17,70,67),1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        text(rt,L"瞬贴",{82,18,240,50},22,INK,true);text(rt,L"版本 0.1.3",{83,50,240,69},10,MUTED);
        text(rt,L"截图、标注、贴图。",{24,87,320,116},19,INK);
        text(rt,L"录下操作，一键复制 GIF。",{24,118,320,148},14,MUTED);
        auto s=recorder.status();if(s.phase==RecordingStatus::Complete){fill(rt,{24,166,320,167},0x2C333C);text(rt,notice.empty()?L"GIF 已导出":notice,{24,180,320,208},12,ACCENT);}
        else if(s.phase==RecordingStatus::Failed)text(rt,s.message,{24,176,320,238},12,0xF0B879);
        if(FAILED(rt->EndDraw())){release(logo);release(rt);}
    }EndPaint(hwnd,&paint);return 0;}
    case WM_ERASEBKGND:return 1;case WM_DRAWITEM:drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_COMMAND:mainCommand(LOWORD(wp));return 0;
    case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;
    case WM_TIMER:refreshStatus();return 0;
    case WM_HOTKEY:if(wp==1)beginCapture(false);if(wp==2)pinImage(clipboardImage(hwnd));if(wp==3){auto s=recorder.status();if(s.phase==RecordingStatus::Starting||s.phase==RecordingStatus::Recording)recorder.stop();else if(!recorder.busy())beginCapture(true);else showMain();}return 0;
    case TRAY_MESSAGE:if(lp==WM_LBUTTONUP||lp==WM_LBUTTONDBLCLK)showMain();if(lp==WM_RBUTTONUP){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,Capture,localize(L"截图").c_str());AppendMenuW(menu,MF_STRING,Record,localize(L"录屏 / 停止").c_str());AppendMenuW(menu,MF_STRING,Hide,localize(L"显示主窗口").c_str());AppendMenuW(menu,MF_STRING,Exit,localize(L"退出").c_str());POINT p;GetCursorPos(&p);SetForegroundWindow(hwnd);int cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);DestroyMenu(menu);if(cmd==Hide)showMain();else if(cmd)mainCommand(cmd);}return 0;
    case WM_CLOSE:if(recorder.busy()){recorder.stop();showMain();message(L"正在完成当前录屏，导出后即可退出。");return 0;}if(settingsWindow)DestroyWindow(settingsWindow);DestroyWindow(hwnd);return 0;
    case WM_DESTROY:KillTimer(hwnd,1);Shell_NotifyIconW(NIM_DELETE,&tray);UnregisterHotKey(hwnd,1);UnregisterHotKey(hwnd,2);UnregisterHotKey(hwnd,3);release(logo);release(rt);PostQuitMessage(0);return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
#include "record_ui.inl"

// 集成冒烟测试：真实桌面、Direct2D 导出、录屏、GIF 文件剪贴板。
int smokeTest(){
    RECT area{160,160,800,640};auto screen=captureDesktop(area);std::vector<snip::Shape> shapes;
    snip::Shape box;box.a={20,20};box.b={260,160};shapes.push_back(box);snip::Shape arrow;arrow.tool=snip::Tool::Arrow;arrow.a={40,200};arrow.b={420,280};shapes.push_back(arrow);
    auto image=flatten(screen,{0,0,float(screen.width),float(screen.height)},shapes);bool png=image.valid()&&savePng(image,outputDir+L"\\smoke-annotated.png");
    recorder.start(area,root,outputDir,60);ULONGLONG deadline=GetTickCount64()+20000;while(GetTickCount64()<deadline){auto phase=recorder.status().phase;if(phase==RecordingStatus::Recording||phase==RecordingStatus::Failed)break;Sleep(100);}
    bool paused=false,resumed=false,snapshot=false,timerPaused=false;
    if(recorder.status().phase==RecordingStatus::Recording){Sleep(700);recorder.pause();deadline=GetTickCount64()+10000;while(GetTickCount64()<deadline&&recorder.status().phase!=RecordingStatus::Paused)Sleep(50);paused=recorder.status().phase==RecordingStatus::Paused;
        if(paused){auto before=recorder.status().elapsedMs;Sleep(1000);timerPaused=recorder.status().elapsedMs==before;recorder.copySnapshot();deadline=GetTickCount64()+30000;while(GetTickCount64()<deadline&&!(recorder.status().phase==RecordingStatus::Paused&&recorder.status().copyVersion>0))Sleep(100);snapshot=recorder.status().copyVersion>0;recorder.resume();deadline=GetTickCount64()+10000;while(GetTickCount64()<deadline&&recorder.status().phase!=RecordingStatus::Recording)Sleep(50);resumed=recorder.status().phase==RecordingStatus::Recording;if(resumed)Sleep(700);}}
    recorder.stop();while(recorder.busy())Sleep(100);
    auto status=recorder.status();bool copied=false,drop=false;HWND owner=CreateWindowExW(0,L"STATIC",L"剪贴板验证",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
    if(status.phase==RecordingStatus::Complete){copied=copyFileToClipboard(owner,status.gif);if(copied&&OpenClipboard(owner)){auto h=static_cast<HDROP>(GetClipboardData(CF_HDROP));wchar_t path[32768]{};drop=h&&DragQueryFileW(h,0,path,32768)>0&&status.gif==path;CloseClipboard();}}DestroyWindow(owner);
    std::ofstream report((outputDir+L"\\smoke-result.json").c_str());report<<"{\"screenshot_png\":"<<(png?"true":"false")<<",\"recording_gif\":"<<(status.phase==RecordingStatus::Complete?"true":"false")<<",\"copy_gif\":"<<(copied?"true":"false")<<",\"clipboard_file_path\":"<<(drop?"true":"false")<<",\"pause\":"<<(paused?"true":"false")<<",\"pause_excludes_wait_time\":"<<(timerPaused?"true":"false")<<",\"copy_while_paused\":"<<(snapshot?"true":"false")<<",\"resume\":"<<(resumed?"true":"false")<<"}";return png&&copied&&drop&&paused&&resumed&&snapshot&&timerPaused?0:1;
}
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR args,int){
    instance=h;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);root=module;root=root.substr(0,root.find_last_of(L"\\"));outputDir=root+L"\\输出";SHCreateDirectoryExW(nullptr,outputDir.c_str(),nullptr);
    english=GetPrivateProfileIntW(L"ui",L"english",0,(root+L"\\settings.ini").c_str())!=0;
    if(!initGraphics()){MessageBoxW(nullptr,L"无法初始化 Windows 图形组件。",L"瞬贴",MB_OK|MB_ICONERROR);return 1;}
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);brandImage=loadImageResource(2);
    if(wcsstr(args,L"--smoke-test")){int result=smokeTest();release(wic);release(writeFactory);release(d2d);CoUninitialize();return result;}
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\ShuntieStudio-FirstRelease");if(GetLastError()==ERROR_ALREADY_EXISTS){HWND other=FindWindowW(L"ShuntieMain",nullptr);if(other){ShowWindow(other,SW_SHOW);SetForegroundWindow(other);}CloseHandle(singleton);return 0;}
    mainScale=float(GetDpiForSystem())/96;uiFont=CreateFontW(-int(15*mainScale),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");appIcon=makeIcon();
    struct Class{const wchar_t* name;WNDPROC proc;};Class classes[]={{L"ShuntieMain",mainProc},{L"ShuntieEditor",overlayProc},{L"ShuntiePin",pinProc},{L"ShuntieRecord",recordProc},{L"ShuntieFrame",frameProc},{L"ShuntieSettings",settingsProc},{L"ShuntieTip",tipProc},{L"ShuntieStroke",strokeProc}};
    smallIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0));
    WNDCLASSEXW paletteClass{};paletteClass.cbSize=sizeof(paletteClass);paletteClass.lpfnWndProc=paletteProc;paletteClass.hInstance=h;paletteClass.hCursor=LoadCursorW(nullptr,IDC_ARROW);paletteClass.lpszClassName=L"ShuntiePalette";RegisterClassExW(&paletteClass);
    for(auto c:classes){WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=c.proc;wc.hInstance=h;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=appIcon;wc.hIconSm=smallIcon;wc.lpszClassName=c.name;if(!RegisterClassExW(&wc))return 1;}
    DWORD style=WS_POPUP|WS_CLIPCHILDREN;
    RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int w=int(344*mainScale),he=int(180*mainScale);
    mainWindow=CreateWindowExW(0,L"ShuntieMain",L"瞬贴 · 截图与录屏",style,(work.right-w)/2,(work.bottom-he)/2,w,he,nullptr,nullptr,h,nullptr);if(!mainWindow)return 1;setLabel(mainWindow,L"瞬贴 · 截图与录屏");
    std::wstring ini=root+L"\\settings.ini";for(int i=0;i<3;++i){keyCodes[i]=GetPrivateProfileIntW(L"shortcuts",(L"key"+std::to_wstring(i)).c_str(),keyCodes[i],ini.c_str());keyModifiers[i]=GetPrivateProfileIntW(L"shortcuts",(L"mod"+std::to_wstring(i)).c_str(),keyModifiers[i],ini.c_str());}gifFps=GetPrivateProfileIntW(L"export",L"fps",30,ini.c_str())==60?60:30;registerKeys();roundWindow(mainWindow);
    tray.cbSize=sizeof(tray);tray.hWnd=mainWindow;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=TRAY_MESSAGE;tray.hIcon=smallIcon?smallIcon:appIcon;wcscpy(tray.szTip,L"瞬贴 · 截图 / 贴图 / 录屏");Shell_NotifyIconW(NIM_ADD,&tray);
    refreshStatus();if(!wcsstr(args,L"--tray"))ShowWindow(mainWindow,SW_SHOW);UpdateWindow(mainWindow);MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)>0){
        // 工具按钮获得焦点后仍由编辑器处理 Enter、Esc 和撤销快捷键。
        if(strokeWindow&&msg.message==WM_KEYDOWN){SendMessageW(strokeWindow,msg.message,msg.wParam,msg.lParam);continue;}
        if(paletteWindow&&msg.message==WM_KEYDOWN){SendMessageW(paletteWindow,msg.message,msg.wParam,msg.lParam);continue;}

        if(overlay&&msg.message==WM_KEYDOWN){SendMessageW(overlay,msg.message,msg.wParam,msg.lParam);continue;}
        if(settingsWindow&&(msg.message==WM_KEYDOWN||msg.message==WM_SYSKEYDOWN)&&waitingHotkey){SendMessageW(settingsWindow,msg.message,msg.wParam,msg.lParam);continue;}
        if(!overlay&&(msg.hwnd==mainWindow||IsChild(mainWindow,msg.hwnd))&&IsDialogMessageW(mainWindow,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    // 退出时先销毁仍打开的贴图，释放其渲染资源。
    HWND pin;while((pin=FindWindowW(L"ShuntiePin",nullptr)))DestroyWindow(pin);
    DeleteObject(uiFont);DestroyIcon(appIcon);if(smallIcon)DestroyIcon(smallIcon);CloseHandle(singleton);release(wic);release(writeFactory);release(d2d);CoUninitialize();return 0;
}
