// SPDX-License-Identifier: AGPL-3.0-or-later
// 独立 Gifski 转换器；通过普通视频和 GIF 文件与主程序交互。
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
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
std::string utf8(const std::wstring& value){int n=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),out.data(),n,nullptr,nullptr);return out;}
// 直接向 Gifski 输入完整 RGBA，避免额外的 4:2:0 降色；跳过完全相同的帧并保留时间轴。
#include "gifski.h"
bool encodeGifFrames(const std::wstring& ff,const std::wstring& tool,const std::wstring& video,const std::wstring& gif,const std::wstring& log,int fps,int width,int height){
    HMODULE library=LoadLibraryW((tool.substr(0,tool.find_last_of(L"\\/"))+L"\\gifski.dll").c_str());if(!library)return false;
    auto create=reinterpret_cast<decltype(&gifski_new)>(GetProcAddress(library,"gifski_new"));auto output=reinterpret_cast<decltype(&gifski_set_file_output)>(GetProcAddress(library,"gifski_set_file_output"));auto add=reinterpret_cast<decltype(&gifski_add_frame_rgba)>(GetProcAddress(library,"gifski_add_frame_rgba"));auto finish=reinterpret_cast<decltype(&gifski_finish)>(GetProcAddress(library,"gifski_finish"));auto lossy=reinterpret_cast<decltype(&gifski_set_lossy_quality)>(GetProcAddress(library,"gifski_set_lossy_quality"));
    if(!create||!output||!add||!finish){FreeLibrary(library);return false;}
    GifskiSettings settings{};settings.quality=90;settings.fast=true;settings.repeat=0;gifski* encoder=create(&settings);if(!encoder){FreeLibrary(library);return false;}if(lossy)lossy(encoder,100);
    if(output(encoder,utf8(gif).c_str())!=GIFSKI_OK){finish(encoder);FreeLibrary(library);return false;}
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE read=nullptr,write=nullptr;if(!CreatePipe(&read,&write,&sa,1<<20)){finish(encoder);FreeLibrary(library);return false;}SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);
    Child decoder;bool started=launch(decoder,ff,L"-hide_banner -loglevel error -nostdin -i "+quote(video)+L" -vf fps="+std::to_wstring(fps)+L" -pix_fmt rgba -f rawvideo -",log,nullptr,write);CloseHandle(write);
    size_t frameBytes=size_t(width)*height*4;std::vector<unsigned char> current(frameBytes),previous(frameBytes);unsigned readFrames=0,submitted=0;int lastSubmitted=-1;bool ok=started;ULONGLONG began=GetTickCount64();
    auto submit=[&](const std::vector<unsigned char>& pixels,int index){if(index<=lastSubmitted)return true;if(add(encoder,submitted,UINT(width),UINT(height),pixels.data(),double(index)/fps)!=GIFSKI_OK)return false;++submitted;lastSubmitted=index;return true;};
    while(ok){size_t offset=0;DWORD bytes=0;while(offset<frameBytes&&ReadFile(read,current.data()+offset,DWORD(frameBytes-offset),&bytes,nullptr)&&bytes)offset+=bytes;if(!offset)break;if(offset!=frameBytes){ok=false;break;}
        if(!readFrames)ok=submit(current,0);
        else if(current!=previous){if(lastSubmitted<int(readFrames)-1)ok=submit(previous,int(readFrames)-1);if(ok)ok=submit(current,int(readFrames));}
        if(!ok)break;previous.swap(current);++readFrames;
    }
    if(ok&&readFrames){if(lastSubmitted<int(readFrames)-2)ok=submit(previous,int(readFrames)-2);if(ok&&lastSubmitted<int(readFrames)-1)ok=submit(previous,int(readFrames)-1);}CloseHandle(read);
    DWORD dc=started?result(decoder):1;GifskiError ec=finish(encoder);FreeLibrary(library);
    std::ofstream stats(log.c_str(),std::ios::app);stats<<"\nGIF RGBA frames_read="<<readFrames<<" frames_submitted="<<submitted<<" elapsed_ms="<<(GetTickCount64()-began)<<"\n";
    return ok&&readFrames&&dc==0&&ec==GIFSKI_OK&&nonempty(gif);
}

int wmain(int count,wchar_t** args){if(count!=9)return 2;return encodeGifFrames(args[1],args[2],args[3],args[4],args[5],_wtoi(args[6]),_wtoi(args[7]),_wtoi(args[8]))?0:1;}
