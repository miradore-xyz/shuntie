#include "recorder.hpp"
#include <shellapi.h>
#include <shlobj.h>
#include <dxgi.h>
#include <vector>
#include <algorithm>

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
bool nonempty(const std::wstring& path){WIN32_FILE_ATTRIBUTE_DATA d{};return GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&d) && (d.nFileSizeHigh || d.nFileSizeLow>0);}
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
