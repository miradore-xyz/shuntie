#define NOMINMAX
#define UNICODE
#include "../src/graphics.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);assert(initGraphics());Image image;image.width=112;image.height=56;image.pixels.resize(112*56*4);for(size_t i=0;i<image.pixels.size();i+=4){image.pixels[i]=BYTE(i%251);image.pixels[i+1]=87;image.pixels[i+2]=199;image.pixels[i+3]=255;}
    HWND owner=CreateWindowExW(0,L"STATIC",L"图片剪贴板检查",WS_POPUP,0,0,0,0,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(owner);assert(copyImage(owner,image));assert(OpenClipboard(nullptr));
    UINT pngFormat=RegisterClipboardFormatW(L"PNG");assert(IsClipboardFormatAvailable(pngFormat)&&IsClipboardFormatAvailable(CF_DIBV5)&&IsClipboardFormatAvailable(CF_DIB));
    auto dib=GetClipboardData(CF_DIBV5);auto header=static_cast<BITMAPV5HEADER*>(GlobalLock(dib));assert(header&&header->bV5Width==112&&header->bV5Height==-56&&header->bV5AlphaMask==0xFF000000);assert(memcmp(reinterpret_cast<BYTE*>(header)+sizeof(*header),image.pixels.data(),image.pixels.size())==0);GlobalUnlock(dib);
    HGLOBAL png=GetClipboardData(pngFormat);auto bytes=static_cast<BYTE*>(GlobalLock(png));IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;assert(SUCCEEDED(wic->CreateStream(&stream)));assert(SUCCEEDED(stream->InitializeFromMemory(bytes,DWORD(GlobalSize(png)))));assert(SUCCEEDED(wic->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder)));assert(SUCCEEDED(decoder->GetFrame(0,&frame)));assert(SUCCEEDED(wic->CreateFormatConverter(&converter)));assert(SUCCEEDED(converter->Initialize(frame,GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)));std::vector<BYTE> decoded(image.pixels.size());assert(SUCCEEDED(converter->CopyPixels(nullptr,112*4,UINT(decoded.size()),decoded.data())));assert(decoded==image.pixels);release(converter);release(frame);release(decoder);release(stream);GlobalUnlock(png);
    auto native=fromBitmap(static_cast<HBITMAP>(GetClipboardData(CF_BITMAP)));assert(native.valid()&&native.width==112&&native.pixels==image.pixels);CloseClipboard();DestroyWindow(owner);assert(OpenClipboard(nullptr));assert(GetClipboardData(pngFormat)!=nullptr);CloseClipboard();
    std::ofstream("artifacts/clipboard-image-result.json")<<"{\"png_roundtrip\":true,\"dib_v5_pixels\":true,\"native_bitmap\":true,\"owner_close_preserved\":true}";release(wic);release(writeFactory);release(d2d);CoUninitialize();std::cout<<"Clipboard image roundtrip passed. / PNG、DIB 和原生图片剪贴板读回检查通过。\n";
}
