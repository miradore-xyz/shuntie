#define NOMINMAX
#define UNICODE
#include "../src/graphics.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

// 用应用同一套 Direct2D 绘制函数检查画笔落笔、松手及其他工具的选中框。
static int mintPixels(const Image& image){
    int count=0;for(size_t i=0;i<image.pixels.size();i+=4){auto p=&image.pixels[i];if(p[1]>145&&p[0]>100&&p[2]<165&&p[1]>p[2]+40)++count;}return count;
}
static Image scene(const Image& source,const snip::Shape& shape,bool drawing,const wchar_t* caption){
    IWICBitmap* surface=nullptr;ID2D1RenderTarget* rt=nullptr;
    assert(SUCCEEDED(wic->CreateBitmap(source.width,source.height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&surface)));
    assert(SUCCEEDED(d2d->CreateWicBitmapRenderTarget(surface,D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96),&rt)));
    auto bg=bitmapFor(rt,source);rt->BeginDraw();rt->DrawBitmap(bg);renderShape(rt,shape,source);renderSelectionFrame(rt,shape,1,drawing);text(rt,caption,{20,15,405,55},17,0xECF0F3);assert(SUCCEEDED(rt->EndDraw()));
    Image image;image.width=source.width;image.height=source.height;image.pixels.resize(source.pixels.size());assert(SUCCEEDED(surface->CopyPixels(nullptr,image.width*4,UINT(image.pixels.size()),image.pixels.data())));release(bg);release(rt);release(surface);return image;
}
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);assert(initGraphics());
    Image source;source.width=420;source.height=270;source.pixels.resize(420*270*4);
    for(size_t i=0;i<source.pixels.size();i+=4){source.pixels[i]=31;source.pixels[i+1]=26;source.pixels[i+2]=22;source.pixels[i+3]=255;}
    Image grid;grid.width=840;grid.height=1080;grid.pixels.resize(size_t(grid.width)*grid.height*4);
    const snip::Tool tools[]={snip::Tool::Rectangle,snip::Tool::Arrow,snip::Tool::Mosaic,snip::Tool::Pen};const wchar_t* names[]={L"方框",L"箭头",L"马赛克",L"画笔"};
    for(int row=0;row<4;++row){snip::Shape shape;shape.tool=tools[row];shape.a={70,85};shape.b={350,220};shape.points={{70,190},{120,100},{190,210},{260,95},{350,180}};shape.stroke=5;
        for(int col=0;col<2;++col){bool drawing=col==0;auto label=std::wstring(names[row])+(drawing?L" · 绘制中":L" · 松手后");auto image=scene(source,shape,drawing,label.c_str());int colored=mintPixels(image);if(row==3&&drawing)assert(colored==0);else assert(colored>100);
            for(int y=0;y<270;++y)memcpy(&grid.pixels[(size_t(row*270+y)*840+col*420)*4],&image.pixels[size_t(y)*420*4],420*4);
        }
        auto exported=flatten(source,{0,0,420,270},{shape});assert(exported.valid()&&mintPixels(exported)==0);
        shape.move(5,-8);shape.rotation=24;auto transformed=scene(source,shape,false,L"移动和旋转后");assert(mintPixels(transformed)>100);
    }
    std::filesystem::create_directories("artifacts");assert(savePng(grid,L"artifacts/selection-frames-0.1.4.png"));release(wic);release(writeFactory);release(d2d);CoUninitialize();std::cout<<"Selection rendering and export checks passed. / 选中框显示规则、变换和导出检查通过。\n";
}
