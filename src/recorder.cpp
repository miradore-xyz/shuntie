#include "recorder.hpp"
#include <shellapi.h>
#include <shlobj.h>
#include <dxgi.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::wstring quote(const std::wstring& s) { return L"\""+s+L"\""; }
struct Child {
    PROCESS_INFORMATION process{};
    HANDLE input=nullptr;
    ~Child(){if(input)CloseHandle(input);if(process.hThread)CloseHandle(process.hThread);if(process.hProcess)CloseHandle(process.hProcess);}
};
bool launch(Child& child,const std::wstring& exe,const std::wstring& args,const std::wstring& log,HANDLE source=nullptr,HANDLE sink=nullptr) {
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    HANDLE inRead=nullptr,inWrite=nullptr;
    if(!source){
        if(!CreatePipe(&inRead,&inWrite,&sa,0))return false;
        SetHandleInformation(inWrite,HANDLE_FLAG_INHERIT,0);
    }
    HANDLE err=CreateFileW(log.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(err==INVALID_HANDLE_VALUE){if(inRead)CloseHandle(inRead);if(inWrite)CloseHandle(inWrite);return false;}
    STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdInput=source?source:inRead;si.hStdOutput=sink?sink:err;si.hStdError=err;
    // 限定继承句柄，避免转换管道被无关子进程持有而无法结束。
    HANDLE inherited[]={si.hStdInput,si.hStdOutput,err};
    std::vector<HANDLE> handles;
    for(HANDLE h:inherited)if(std::find(handles.begin(),handles.end(),h)==handles.end())handles.push_back(h);
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> storage(bytes);
    auto list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    bool initialized=InitializeProcThreadAttributeList(list,1,0,&bytes)!=FALSE;
    STARTUPINFOEXW ex{};ex.StartupInfo=si;ex.StartupInfo.cb=sizeof(ex);ex.lpAttributeList=list;
    bool attrs=initialized && UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles.data(),handles.size()*sizeof(HANDLE),nullptr,nullptr);
    std::wstring command=quote(exe)+L" "+args;
    BOOL ok=attrs && CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&ex.StartupInfo,&child.process);
    if(initialized)DeleteProcThreadAttributeList(list);
    CloseHandle(err);if(inRead)CloseHandle(inRead);
    if(ok)child.input=inWrite;else if(inWrite)CloseHandle(inWrite);
    return ok!=FALSE;
}
DWORD result(Child& child){WaitForSingleObject(child.process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(child.process.hProcess,&code);return code;}
uint64_t fileSizeBytes(const std::wstring& path){
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&d))return 0;
    return (uint64_t(d.nFileSizeHigh)<<32)|uint64_t(d.nFileSizeLow);
}
bool nonempty(const std::wstring& path){return fileSizeBytes(path)>0;}
constexpr uint64_t kVideoMaxBytes=20000000ULL;
constexpr uint64_t kVideoSkipCompressBytes=3000000ULL;
struct VideoCompressPlan {
    int outWidth=0,outHeight=0,outFps=30,targetKbps=1500,maxrateKbps=1725,bufsizeKbps=3450,gop=120,crf=27;
};
VideoCompressPlan planVideoCompression(double durationSec,int srcW,int srcH,uint64_t maxBytes=kVideoMaxBytes,double fillRatio=0.88){
    durationSec=std::max(1.0,durationSec);
    double aimBytes=double(maxBytes)*std::clamp(fillRatio,0.35,0.95);
    int maxAvgKbps=std::clamp(int(std::floor(aimBytes*8.0/1000.0/durationSec)),120,10000);
    int maxLongSide=640,outFps=10,crf=32;
    if(maxAvgKbps>=3200){maxLongSide=2560;outFps=30;crf=(durationSec>=20.0?27:25);}
    else if(maxAvgKbps>=1600){maxLongSide=2560;outFps=24;crf=28;}
    else if(maxAvgKbps>=950){maxLongSide=1920;outFps=20;crf=29;}
    else if(maxAvgKbps>=500){maxLongSide=1280;outFps=15;crf=30;}
    else if(maxAvgKbps>=260){maxLongSide=960;outFps=12;crf=31;}
    int outW=0,outH=0;
    if(srcW>=2&&srcH>=2){
        int evenW=srcW&~1,evenH=srcH&~1;
        int longSide=std::max(evenW,evenH);
        if(longSide>maxLongSide){
            double scale=double(maxLongSide)/double(longSide);
            outW=std::max(2,int(std::round(evenW*scale))&~1);
            outH=std::max(2,int(std::round(evenH*scale))&~1);
        }else{
            outW=evenW;outH=evenH;
        }
    }
    int maxrateKbps=maxAvgKbps;
    int bufsizeKbps=maxAvgKbps*2;
    int gop=std::max(outFps*2,outFps*4);
    return {outW,outH,outFps,maxAvgKbps,maxrateKbps,bufsizeKbps,gop,crf};
}
double probeVideoDurationSec(const std::wstring& ff,const std::wstring& inputMp4,double fallbackSec){
    std::wstring probeLog=inputMp4+L".probe.log";
    DeleteFileW(probeLog.c_str());
    Child child;
    if(launch(child,ff,L"-hide_banner -nostdin -i "+quote(inputMp4),probeLog))result(child);
    std::ifstream in(probeLog.c_str(),std::ios::binary);
    std::ostringstream ss;ss<<in.rdbuf();
    std::string content=ss.str();
    in.close();
    DeleteFileW(probeLog.c_str());
    size_t pos=content.find("Duration:");
    if(pos!=std::string::npos){
        int h=0,m=0;double s=0;
        if(sscanf(content.c_str()+pos,"Duration: %d:%d:%lf",&h,&m,&s)==3){
            double sec=h*3600.0+m*60.0+s;
            if(sec>0.1)return sec;
        }
    }
    return std::max(1.0,fallbackSec);
}
bool compressVideoToLimit(const std::wstring& ff,const std::wstring& inputMp4,const std::wstring& outputMp4,const std::wstring& log,ULONGLONG durationMs,int srcW,int srcH,uint64_t maxBytes=kVideoMaxBytes){
    uint64_t rawBytes=fileSizeBytes(inputMp4);
    if(rawBytes==0)return false;
    if(rawBytes<=std::min(maxBytes,kVideoSkipCompressBytes)){
        if(inputMp4!=outputMp4)return CopyFileW(inputMp4.c_str(),outputMp4.c_str(),FALSE)!=FALSE&&nonempty(outputMp4);
        return true;
    }
    double durationSec=probeVideoDurationSec(ff,inputMp4,double(durationMs)/1000.0);
    auto runPass=[&](const VideoCompressPlan& plan,const std::wstring& dest,bool crfVbvMode)->bool{
        std::wstring vf=L"fps="+std::to_wstring(plan.outFps);
        if(plan.outWidth>0&&plan.outHeight>0&&(plan.outWidth!=srcW||plan.outHeight!=srcH))
            vf+=L",scale="+std::to_wstring(plan.outWidth)+L":"+std::to_wstring(plan.outHeight)+L":flags=area";
        int maxrate=crfVbvMode?plan.maxrateKbps:std::max(plan.targetKbps+20,int(std::round(plan.targetKbps*1.05)));
        int bufsize=crfVbvMode?plan.bufsizeKbps:std::max(plan.targetKbps,maxrate);
        std::wstring commonTail=L" -pix_fmt yuv420p -profile:v high -an -movflags +faststart "+quote(dest);
        for(int attempt=0;attempt<2;++attempt){
            DeleteFileW(dest.c_str());
            std::wstring args=L"-hide_banner -nostdin -y -i "+quote(inputMp4)+L" -vf "+vf;
            if(crfVbvMode){
                if(attempt==0){
                    args+=L" -c:v libx264 -preset veryfast -crf "+std::to_wstring(plan.crf)+
                        L" -g "+std::to_wstring(plan.gop)+
                        L" -maxrate "+std::to_wstring(maxrate)+L"k -bufsize "+std::to_wstring(bufsize)+L"k"+commonTail;
                }else{
                    args+=L" -c:v h264_nvenc -preset p6 -tune hq -rc vbr -cq "+std::to_wstring(std::min(51,plan.crf+2))+
                        L" -multipass qres -spatial_aq 1 -temporal_aq 1 -g "+std::to_wstring(plan.gop)+
                        L" -b:v 0 -maxrate "+std::to_wstring(maxrate)+L"k -bufsize "+std::to_wstring(bufsize)+L"k"+commonTail;
                }
            }else{
                if(attempt==0){
                    args+=L" -c:v libx264 -preset veryfast -g "+std::to_wstring(plan.gop)+
                        L" -b:v "+std::to_wstring(plan.targetKbps)+L"k -maxrate "+std::to_wstring(maxrate)+
                        L"k -bufsize "+std::to_wstring(bufsize)+L"k"+commonTail;
                }else{
                    args+=L" -c:v h264_nvenc -preset p5 -rc vbr -multipass qres -g "+std::to_wstring(plan.gop)+
                        L" -b:v "+std::to_wstring(plan.targetKbps)+L"k -maxrate "+std::to_wstring(maxrate)+
                        L"k -bufsize "+std::to_wstring(bufsize)+L"k"+commonTail;
                }
            }
            Child child;
            if(launch(child,ff,args,log)&&result(child)==0&&nonempty(dest))return true;
        }
        DeleteFileW(dest.c_str());
        return false;
    };
    auto plan1=planVideoCompression(durationSec,srcW,srcH,maxBytes,0.88);
    std::wstring tmp1=outputMp4+L".cmp1.mp4";
    if(!runPass(plan1,tmp1,true)){DeleteFileW(tmp1.c_str());return false;}
    uint64_t size1=fileSizeBytes(tmp1);
    std::wstring bestTmp=tmp1;uint64_t bestSize=size1;
    if(size1>maxBytes&&plan1.targetKbps>120){
        double retryFill=std::clamp(0.84*(double(maxBytes)/double(size1)),0.42,0.80);
        auto plan2=planVideoCompression(durationSec,srcW,srcH,maxBytes,retryFill);
        std::wstring tmp2=outputMp4+L".cmp2.mp4";
        if(runPass(plan2,tmp2,false)){
            uint64_t size2=fileSizeBytes(tmp2);
            if(size2>0&&size2<bestSize){
                bestTmp=tmp2;bestSize=size2;DeleteFileW(tmp1.c_str());
            }else{
                DeleteFileW(tmp2.c_str());
            }
        }else{
            DeleteFileW(tmp2.c_str());
        }
    }
    if(rawBytes<=maxBytes&&bestSize>=rawBytes){
        DeleteFileW(bestTmp.c_str());
        if(inputMp4!=outputMp4)return CopyFileW(inputMp4.c_str(),outputMp4.c_str(),FALSE)!=FALSE&&nonempty(outputMp4);
        return true;
    }
    if(!MoveFileExW(bestTmp.c_str(),outputMp4.c_str(),MOVEFILE_REPLACE_EXISTING)){
        DeleteFileW(bestTmp.c_str());return false;
    }
    return nonempty(outputMp4);
}
// FFmpeg 默认 D3D11 设备的输出索引；无法匹配时交给兼容捕获。
int outputFor(RECT area,RECT& monitor) {
    IDXGIFactory1* factory=nullptr;IDXGIAdapter1* adapter=nullptr;int match=-1;
    if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)))) {
        if(SUCCEEDED(factory->EnumAdapters1(0,&adapter))) {
            for(UINT i=0;;++i){IDXGIOutput* out=nullptr;if(FAILED(adapter->EnumOutputs(i,&out)))break;
                DXGI_OUTPUT_DESC desc{};out->GetDesc(&desc);auto r=desc.DesktopCoordinates;
                if(area.left>=r.left&&area.top>=r.top&&area.right<=r.right&&area.bottom<=r.bottom){match=int(i);monitor=r;}
                out->Release();
            }adapter->Release();
        }factory->Release();
    }return match;
}
}
std::wstring formatFileSizeMB(uint64_t bytes){
    double mb=double(bytes)/(1024.0*1024.0);
    wchar_t buf[32]{};
    swprintf(buf,32,mb>=1.0?L"%.1f MB":L"%.2f MB",mb);
    return buf;
}
#include "record_session.inl"
bool copyFileToClipboard(HWND owner,const std::wstring& file){
    if(!nonempty(file)||!OpenClipboard(owner))return false;
    size_t bytes=sizeof(DROPFILES)+(file.size()+2)*sizeof(wchar_t);
    HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,bytes);
    if(!data){CloseClipboard();return false;}
    auto drop=static_cast<DROPFILES*>(GlobalLock(data));
    if(!drop){GlobalFree(data);CloseClipboard();return false;}
    drop->pFiles=sizeof(DROPFILES);drop->fWide=TRUE;
    memcpy(reinterpret_cast<unsigned char*>(drop)+sizeof(DROPFILES),file.c_str(),(file.size()+1)*sizeof(wchar_t));GlobalUnlock(data);
    EmptyClipboard();bool ok=SetClipboardData(CF_HDROP,data)!=nullptr;if(!ok)GlobalFree(data);
    if(ok){HGLOBAL effect=GlobalAlloc(GMEM_MOVEABLE,sizeof(DWORD));if(effect){auto p=static_cast<DWORD*>(GlobalLock(effect));if(p){*p=DROPEFFECT_COPY;GlobalUnlock(effect);if(!SetClipboardData(RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT),effect))GlobalFree(effect);}else GlobalFree(effect);}}
    CloseClipboard();return ok;
}
