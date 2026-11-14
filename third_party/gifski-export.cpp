// SPDX-License-Identifier: AGPL-3.0-or-later
// 独立原尺寸 GIF 转换器；主程序仅通过命令行和普通文件与它交互。
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
std::string utf8(const std::wstring& value){int n=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),out.data(),n,nullptr,nullptr);return out;}
#include "native-gif.hpp"
int wmain(int count,wchar_t** args){
    if(count!=9)return 2;
    int fps=_wtoi(args[6]),width=_wtoi(args[7]),height=_wtoi(args[8]);
    if(fps<1||fps>50||width<1||height<1||width>65535||height>65535)return 2;
    // 限制局部调色的线程数，避免每个小更新都唤醒整机线程池。
    SetEnvironmentVariableW(L"RAYON_NUM_THREADS",L"2");
    return encodeNativeGif(args[2],args[3],args[4],args[5],fps,width,height)?0:1;
}
