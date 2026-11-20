// 传入实际视频进行导出回归和耗时测量，不修改输入文件。
#include "../src/recorder.cpp"
#include <iostream>
int wmain(int count,wchar_t** arguments){
    if(count!=6&&count!=7)return 2;wchar_t current[32768];GetCurrentDirectoryW(32768,current);std::wstring base=current;
    int quality=count>=7?_wtoi(arguments[6]):3;
    bool ok=encodeGif(base+L"\\dist\\bin\\ffmpeg.exe",base+L"\\dist\\bin\\gifski.exe",arguments[1],arguments[2],std::wstring(arguments[2])+L".log",_wtoi(arguments[5]),_wtoi(arguments[3]),_wtoi(arguments[4]),quality);std::cout<<(ok?"GIF export passed. / GIF 导出通过。\n":"GIF export failed. / GIF 导出失败。\n");return ok?0:1;
}
