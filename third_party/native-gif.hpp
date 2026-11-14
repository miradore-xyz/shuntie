// SPDX-License-Identifier: AGPL-3.0-or-later
// 原尺寸增量 GIF：只对变化区域调色、压缩；不重采样空间像素。
#pragma once
#include <wincodec.h>
#include <propidl.h>
#include <cstdint>
#include <emmintrin.h>
#include "native-video.hpp"

// SSE2 为 Windows x64 的基础指令集；按块比较，避免逐字节扫描整幅 2K 画面。
inline bool sameGifPixels(const BYTE* a,const BYTE* b,size_t bytes){
    size_t n=0;for(;n+16<=bytes;n+=16)if(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(a+n)),_mm_loadu_si128(reinterpret_cast<const __m128i*>(b+n))))!=65535)return false;
    for(;n<bytes;++n)if(a[n]!=b[n])return false;return true;
}

template<class T> struct GifCom {
    T* p=nullptr;
    ~GifCom(){if(p)p->Release();}
    T** out(){return &p;}
    T* operator->(){return p;}
};

struct NativeGif {
    GifCom<IWICImagingFactory> factory;
    GifCom<IWICStream> stream;
    GifCom<IWICBitmapEncoder> encoder;
    HRESULT error=E_FAIL;
    bool initialized=false;
    using Quantize=int(*)(const void*,size_t,BYTE*,UINT*);
    HMODULE library=nullptr;
    Quantize quantize=nullptr;
    ULONGLONG quantMs=0,writeMs=0;uint64_t changedPixels=0;
    NativeGif(const std::wstring& path,const std::wstring& tool){
        library=LoadLibraryW((tool.substr(0,tool.find_last_of(L"\\/"))+L"\\gifski.dll").c_str());
        if(library)quantize=reinterpret_cast<Quantize>(GetProcAddress(library,"shuntie_quantize_rgba"));
        if(!quantize)return;
        initialized=SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
        error=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out()));
        if(SUCCEEDED(error))error=factory->CreateStream(stream.out());
        if(SUCCEEDED(error))error=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
        if(SUCCEEDED(error))error=factory->CreateEncoder(GUID_ContainerFormatGif,nullptr,encoder.out());
        if(SUCCEEDED(error))error=encoder->Initialize(stream.p,WICBitmapEncoderNoCache);
        GifCom<IWICMetadataQueryWriter> metadata;
        if(SUCCEEDED(error))error=encoder->GetMetadataQueryWriter(metadata.out());
        if(SUCCEEDED(error)){
            // Netscape 循环扩展：0 表示无限循环。
            BYTE application[]={'N','E','T','S','C','A','P','E','2','.','0'},data[]={3,1,0,0,0};
            PROPVARIANT value{};value.vt=VT_VECTOR|VT_UI1;value.caub={11,application};
            error=metadata->SetMetadataByName(L"/appext/Application",&value);
            value.caub={5,data};if(SUCCEEDED(error))error=metadata->SetMetadataByName(L"/appext/Data",&value);
        }
    }
    ~NativeGif(){
        // 先释放 COM 对象，再关闭本线程的 COM 环境。
        if(encoder.p){encoder.p->Release();encoder.p=nullptr;}
        if(stream.p){stream.p->Release();stream.p=nullptr;}
        if(factory.p){factory.p->Release();factory.p=nullptr;}
        if(initialized)CoUninitialize();
        if(library)FreeLibrary(library);
    }
    bool frame(const std::vector<unsigned char>& pixels,const std::vector<uint32_t>& positions,int width,RECT bounds,unsigned delay){
        if(FAILED(error))return false;
        UINT w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
        GifCom<IWICPalette> palette;
        GifCom<IWICBitmapFrameEncode> frame;
        GifCom<IPropertyBag2> properties;
        GifCom<IWICMetadataQueryWriter> metadata;
        // 首帧完整调色；后续只输入有变化的像素，矩形内其余位置保持透明。
        std::vector<uint32_t> samples;
        size_t count=positions.empty()?size_t(w)*h:positions.size();
        const auto source=reinterpret_cast<const uint32_t*>(pixels.data());
        if(!positions.empty()){samples.resize(count);for(size_t n=0;n<count;++n)samples[n]=source[positions[n]];}
        std::vector<BYTE> remapped(count),indexed(size_t(w)*h,255);UINT colors[256]{};
        auto began=GetTickCount64();changedPixels+=count;
        int colorCount=quantize(positions.empty()?source:samples.data(),count,remapped.data(),colors);
        quantMs+=GetTickCount64()-began;began=GetTickCount64();
        if(colorCount<1||colorCount>255){error=E_FAIL;return false;}
        if(positions.empty())indexed.swap(remapped);
        else for(size_t n=0;n<positions.size();++n){auto index=positions[n];indexed[(index/width-bounds.top)*w+index%width-bounds.left]=remapped[n];}
        error=factory->CreatePalette(palette.out());
        if(SUCCEEDED(error))error=palette->InitializeCustom(colors,256);
        if(SUCCEEDED(error))error=encoder->CreateNewFrame(frame.out(),properties.out());
        if(SUCCEEDED(error))error=frame->Initialize(properties.p);
        if(SUCCEEDED(error))error=frame->SetSize(w,h);
        auto format=GUID_WICPixelFormat8bppIndexed;
        if(SUCCEEDED(error))error=frame->SetPixelFormat(&format);
        if(SUCCEEDED(error))error=frame->SetPalette(palette.p);
        if(SUCCEEDED(error))error=frame->GetMetadataQueryWriter(metadata.out());
        auto set16=[&](LPCWSTR name,unsigned number){PROPVARIANT v{};v.vt=VT_UI2;v.uiVal=USHORT(number);if(SUCCEEDED(error))error=metadata->SetMetadataByName(name,&v);};
        set16(L"/imgdesc/Left",bounds.left);set16(L"/imgdesc/Top",bounds.top);set16(L"/grctlext/Delay",delay);
        PROPVARIANT disposal{};disposal.vt=VT_UI1;disposal.bVal=1;
        if(SUCCEEDED(error))error=metadata->SetMetadataByName(L"/grctlext/Disposal",&disposal);
        PROPVARIANT transparent{};transparent.vt=VT_BOOL;transparent.boolVal=VARIANT_TRUE;
        if(SUCCEEDED(error))error=metadata->SetMetadataByName(L"/grctlext/TransparencyFlag",&transparent);
        transparent.vt=VT_UI1;transparent.bVal=255;
        if(SUCCEEDED(error))error=metadata->SetMetadataByName(L"/grctlext/TransparentColorIndex",&transparent);
        if(SUCCEEDED(error))error=frame->WritePixels(h,w,UINT(indexed.size()),indexed.data());
        if(SUCCEEDED(error))error=frame->Commit();
        writeMs+=GetTickCount64()-began;
        return SUCCEEDED(error);
    }
    bool finish(){if(SUCCEEDED(error))error=encoder->Commit();return SUCCEEDED(error);}
};

// 相同帧合并时累加延时；局部变化保持原坐标，第一帧始终为完整画面。
bool encodeNativeGif(const std::wstring& tool,const std::wstring& video,const std::wstring& gif,const std::wstring& log,int fps,int width,int height){
    NativeGif encoder(gif,tool);if(FAILED(encoder.error))return false;
    NativeVideo decoder(video);if(decoder.error<0)return false;
    size_t frameBytes=size_t(width)*height*4;
    std::vector<unsigned char> current(frameBytes),previous(frameBytes);
    std::vector<uint32_t> positions,nextPositions;
    unsigned readFrames=0,submitted=0,pending=0;RECT bounds{0,0,width,height};bool ok=true;ULONGLONG began=GetTickCount64(),readMs=0,scanMs=0;
    auto submit=[&](unsigned end){
        unsigned duration=unsigned((uint64_t(end)*100+fps/2)/fps-(uint64_t(pending)*100+fps/2)/fps);
        while(duration){unsigned delay=std::min(duration,65535u);if(!encoder.frame(previous,positions,width,bounds,delay))return false;duration-=delay;++submitted;}
        return true;
    };
    while(ok){
        auto step=GetTickCount64();
        if(!decoder.next())break;
        unsigned index=decoder.sampleIndex(fps);
        if(readFrames&&index<readFrames)continue;
        if(!decoder.rgb(current,width,height)){ok=false;break;}
        readMs+=GetTickCount64()-step;step=GetTickCount64();
        if(!readFrames)previous.swap(current);
        else {
            RECT changed{width,height,0,0};
            nextPositions.clear();
            for(int y=0;y<height;++y){
                const auto rowA=current.data()+size_t(y)*width*4,rowB=previous.data()+size_t(y)*width*4;
                if(sameGifPixels(rowA,rowB,width*4))continue;
                for(int block=0;block<width;block+=16){
                    int end=std::min(block+16,width);if(sameGifPixels(rowA+block*4,rowB+block*4,(end-block)*4))continue;
                    for(int x=block;x<end;++x){auto a=reinterpret_cast<const uint32_t*>(rowA)+x,b=reinterpret_cast<const uint32_t*>(rowB)+x;if(*a==*b)continue;
                        nextPositions.push_back(uint32_t(size_t(y)*width+x));changed.left=std::min(changed.left,LONG(x));changed.right=std::max(changed.right,LONG(x+1));
                    }
                }
                changed.top=std::min(changed.top,LONG(y));changed.bottom=y+1;
            }
            scanMs+=GetTickCount64()-step;
            if(changed.right){ok=submit(index);bounds=changed;pending=index;previous.swap(current);positions.swap(nextPositions);}
        }
        readFrames=index+1;
    }
    if(ok&&readFrames)ok=submit(std::max(readFrames,decoder.endIndex(fps)));
    bool finished=ok&&decoder.error>=0&&encoder.finish();
    std::ofstream stats(log.c_str(),std::ios::app);stats<<"\nGIF native width="<<width<<" height="<<height<<" frames_read="<<readFrames<<" frames_written="<<submitted<<" elapsed_ms="<<(GetTickCount64()-began)<<" read_ms="<<readMs<<" scan_ms="<<scanMs<<" quant_ms="<<encoder.quantMs<<" write_ms="<<encoder.writeMs<<" changed_pixels="<<encoder.changedPixels<<" error="<<std::hex<<encoder.error<<"\n";
    return finished&&readFrames;
}
