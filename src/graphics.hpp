#pragma once
#include "model.hpp"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <memory>
#include "language.hpp"

template<class T> void release(T*& p){if(p){p->Release();p=nullptr;}}
struct Image {int width=0,height=0;std::vector<unsigned char> pixels;bool valid()const{return width>0&&height>0&&!pixels.empty();}};
inline ID2D1Factory* d2d=nullptr;
inline IDWriteFactory* writeFactory=nullptr;
inline IWICImagingFactory* wic=nullptr;
inline bool initGraphics(){return SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&d2d))&&SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&writeFactory)))&&SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)));}
inline Image loadImageResource(int id){
    Image im;HRSRC resource=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);if(!resource)return im;DWORD size=SizeofResource(nullptr,resource);BYTE* bytes=static_cast<BYTE*>(LockResource(LoadResource(nullptr,resource)));
    IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;HRESULT hr=wic->CreateStream(&stream);
    if(SUCCEEDED(hr))hr=stream->InitializeFromMemory(bytes,size);if(SUCCEEDED(hr))hr=wic->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder);if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);if(SUCCEEDED(hr))hr=wic->CreateFormatConverter(&converter);if(SUCCEEDED(hr))hr=converter->Initialize(frame,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(hr)){UINT w,h;converter->GetSize(&w,&h);im.width=int(w);im.height=int(h);im.pixels.resize(size_t(w)*h*4);if(FAILED(converter->CopyPixels(nullptr,w*4,UINT(im.pixels.size()),im.pixels.data())))im={};}release(converter);release(frame);release(decoder);release(stream);return im;
}
inline Image fromBitmap(HBITMAP bitmap){
    Image im;BITMAP b{};if(!GetObjectW(bitmap,sizeof(b),&b))return im;
    im.width=b.bmWidth;im.height=std::abs(b.bmHeight);im.pixels.resize(size_t(im.width)*im.height*4);
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),im.width,-im.height,1,32,BI_RGB};
    HDC dc=GetDC(nullptr);int ok=GetDIBits(dc,bitmap,0,im.height,im.pixels.data(),&info,DIB_RGB_COLORS);ReleaseDC(nullptr,dc);
    if(!ok)return {};for(size_t i=3;i<im.pixels.size();i+=4)im.pixels[i]=255;return im;
}
inline Image captureDesktop(RECT r){
    HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);int w=r.right-r.left,h=r.bottom-r.top;
    HBITMAP bitmap=CreateCompatibleBitmap(screen,w,h);HGDIOBJ old=SelectObject(memory,bitmap);
    BOOL ok=BitBlt(memory,0,0,w,h,screen,r.left,r.top,SRCCOPY|CAPTUREBLT);
    SelectObject(memory,old);Image im=ok?fromBitmap(bitmap):Image{};DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);return im;
}
inline Image clipboardImage(HWND owner){
    if(!OpenClipboard(owner))return {};Image im;
    auto bitmap=static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));if(bitmap)im=fromBitmap(bitmap);
    CloseClipboard();return im;
}
inline bool copyImage(HWND owner,const Image& im){
    if(!im.valid()||!OpenClipboard(owner))return false;size_t bytes=sizeof(BITMAPINFOHEADER)+im.pixels.size();HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,bytes);
    if(!data){CloseClipboard();return false;}auto p=static_cast<unsigned char*>(GlobalLock(data));
    if(!p){GlobalFree(data);CloseClipboard();return false;}
    BITMAPINFOHEADER header{sizeof(BITMAPINFOHEADER),im.width,-im.height,1,32,BI_RGB};memcpy(p,&header,sizeof(header));memcpy(p+sizeof(header),im.pixels.data(),im.pixels.size());GlobalUnlock(data);
    EmptyClipboard();bool ok=SetClipboardData(CF_DIB,data)!=nullptr;if(!ok)GlobalFree(data);CloseClipboard();return ok;
}
inline bool savePng(const Image& im,const std::wstring& path){
    IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;IPropertyBag2* props=nullptr;
    HRESULT hr=wic->CreateStream(&stream);if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
    if(SUCCEEDED(hr))hr=wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);
    if(SUCCEEDED(hr))hr=encoder->Initialize(stream,WICBitmapEncoderNoCache);
    if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,&props);if(SUCCEEDED(hr))hr=frame->Initialize(props);
    if(SUCCEEDED(hr))hr=frame->SetSize(im.width,im.height);WICPixelFormatGUID fmt=GUID_WICPixelFormat32bppBGRA;
    if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&fmt);if(SUCCEEDED(hr))hr=frame->WritePixels(im.height,im.width*4,UINT(im.pixels.size()),const_cast<BYTE*>(im.pixels.data()));
    if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();
    release(props);release(frame);release(encoder);release(stream);return SUCCEEDED(hr);
}
inline D2D1_COLOR_F rgb(unsigned color,float alpha=1){return D2D1::ColorF(color,alpha);}
inline D2D1_RECT_F rect(snip::Box b){return {b.l,b.t,b.r,b.b};}
inline D2D1_POINT_2F pt(snip::Point p){return {p.x,p.y};}
inline ID2D1Bitmap* bitmapFor(ID2D1RenderTarget* rt,const Image& im,bool alpha=false){ID2D1Bitmap* b=nullptr;if(im.valid())rt->CreateBitmap(D2D1::SizeU(im.width,im.height),im.pixels.data(),im.width*4,D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,alpha?D2D1_ALPHA_MODE_PREMULTIPLIED:D2D1_ALPHA_MODE_IGNORE),96,96),&b);return b;}
inline void text(ID2D1RenderTarget* rt,const std::wstring& value,snip::Box box,float size,unsigned color,bool bold=false){
    IDWriteTextFormat* format=nullptr;ID2D1SolidColorBrush* brush=nullptr;
    writeFactory->CreateTextFormat(L"Microsoft YaHei UI",nullptr,bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"zh-cn",&format);
    auto translated=localize(value);rt->CreateSolidColorBrush(rgb(color),&brush);if(format&&brush)rt->DrawText(translated.c_str(),UINT(translated.size()),format,rect(box),brush);release(brush);release(format);
}
inline void fill(ID2D1RenderTarget* rt,snip::Box box,unsigned color,float alpha=1){ID2D1SolidColorBrush* b=nullptr;rt->CreateSolidColorBrush(rgb(color,alpha),&b);if(b)rt->FillRectangle(rect(box),b);release(b);}
inline void renderShape(ID2D1RenderTarget* rt,const snip::Shape& s,const Image& source){
    ID2D1SolidColorBrush* brush=nullptr;rt->CreateSolidColorBrush(rgb(s.color),&brush);if(!brush)return;auto bounds=s.rawBounds();D2D1_MATRIX_3X2_F previous;rt->GetTransform(&previous);rt->SetTransform(D2D1::Matrix3x2F::Rotation(s.rotation,pt(s.center()))*previous);
    switch(s.tool){
    case snip::Tool::Rectangle:rt->DrawRectangle(rect(bounds),brush,s.stroke);break;
    case snip::Tool::Arrow:{rt->DrawLine(pt(s.a),pt(s.b),brush,s.stroke);float angle=std::atan2(s.b.y-s.a.y,s.b.x-s.a.x),len=14+s.stroke*2;
        snip::Point c{s.b.x-len*std::cos(angle-0.48f),s.b.y-len*std::sin(angle-0.48f)},d{s.b.x-len*std::cos(angle+0.48f),s.b.y-len*std::sin(angle+0.48f)};
        rt->DrawLine(pt(s.b),pt(c),brush,s.stroke);rt->DrawLine(pt(s.b),pt(d),brush,s.stroke);break;}
    case snip::Tool::Pen:if(s.points.size()>1){ID2D1PathGeometry* path=nullptr;ID2D1GeometrySink* sink=nullptr;ID2D1StrokeStyle* style=nullptr;auto props=D2D1::StrokeStyleProperties();props.startCap=props.endCap=D2D1_CAP_STYLE_ROUND;props.lineJoin=D2D1_LINE_JOIN_ROUND;d2d->CreateStrokeStyle(props,nullptr,0,&style);if(SUCCEEDED(d2d->CreatePathGeometry(&path))&&SUCCEEDED(path->Open(&sink))){sink->BeginFigure(pt(s.points.front()),D2D1_FIGURE_BEGIN_HOLLOW);for(size_t i=1;i<s.points.size();++i)sink->AddLine(pt(s.points[i]));sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();rt->DrawGeometry(path,brush,s.stroke,style);}release(style);release(sink);release(path);}break;
    case snip::Tool::Mosaic:{
        int left=std::max(0,int(bounds.l)),top=std::max(0,int(bounds.t)),right=std::min(source.width,int(bounds.r)),bottom=std::min(source.height,int(bounds.b));
        const int block=14;
        for(int y=top;y<bottom;y+=block)for(int x=left;x<right;x+=block){
            unsigned blue=0,green=0,red=0,count=0;
            for(int j=y;j<std::min(y+block,bottom);++j)for(int i=x;i<std::min(x+block,right);++i){auto p=&source.pixels[(size_t(j)*source.width+i)*4];blue+=p[0];green+=p[1];red+=p[2];++count;}
            if(count){brush->SetColor(rgb(((red/count)<<16)|((green/count)<<8)|(blue/count)));rt->FillRectangle(D2D1::RectF(float(x),float(y),float(std::min(x+block,right)),float(std::min(y+block,bottom))),brush);}
        }break;}
    default:break;
    }rt->SetTransform(previous);release(brush);
}
inline Image flatten(const Image& source,snip::Box selection,const std::vector<snip::Shape>& shapes){
    int x=std::max(0,int(selection.l)),y=std::max(0,int(selection.t)),w=std::min(source.width,int(selection.r))-x,h=std::min(source.height,int(selection.b))-y;
    if(w<1||h<1)return {};IWICBitmap* surface=nullptr;ID2D1RenderTarget* rt=nullptr;
    HRESULT hr=wic->CreateBitmap(w,h,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&surface);
    if(SUCCEEDED(hr))hr=d2d->CreateWicBitmapRenderTarget(surface,D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96),&rt);
    Image out;if(SUCCEEDED(hr)){
        ID2D1Bitmap* bg=bitmapFor(rt,source);rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Translation(float(-x),float(-y)));
        if(bg)rt->DrawBitmap(bg,D2D1::RectF(0,0,float(source.width),float(source.height)));
        for(auto& shape:shapes)renderShape(rt,shape,source);hr=rt->EndDraw();release(bg);
        if(SUCCEEDED(hr)){out.width=w;out.height=h;out.pixels.resize(size_t(w)*h*4);if(FAILED(surface->CopyPixels(nullptr,w*4,UINT(out.pixels.size()),out.pixels.data())))out={};}
    }release(rt);release(surface);return out;
}
