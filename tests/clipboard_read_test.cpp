#define NOMINMAX
#define UNICODE
#include "../src/graphics.hpp"
#include <cassert>
#include <iostream>
int main(){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);assert(initGraphics());assert(OpenClipboard(nullptr));HGLOBAL data=GetClipboardData(RegisterClipboardFormatW(L"PNG"));assert(data);auto bytes=static_cast<BYTE*>(GlobalLock(data));IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;assert(SUCCEEDED(wic->CreateStream(&stream)));assert(SUCCEEDED(stream->InitializeFromMemory(bytes,DWORD(GlobalSize(data)))));assert(SUCCEEDED(wic->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder)));assert(SUCCEEDED(decoder->GetFrame(0,&frame)));UINT width,height;assert(SUCCEEDED(frame->GetSize(&width,&height)));auto image=fromBitmap(static_cast<HBITMAP>(GetClipboardData(CF_BITMAP)));assert(image.valid()&&UINT(image.width)==width&&UINT(image.height)==height);release(frame);release(decoder);release(stream);GlobalUnlock(data);CloseClipboard();std::cout<<"{\"png_width\":"<<width<<",\"png_height\":"<<height<<",\"native_bitmap_matches\":true}\n";release(wic);release(writeFactory);release(d2d);CoUninitialize();}
