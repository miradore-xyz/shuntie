#define UNICODE
#include <windows.h>
#include "../src/language.hpp"
#include <cassert>
#include <iostream>
int main(){
    english=true;
    assert(localize(L"截图、标注、贴图。")==L"Capture. Mark up. Pin.");
    assert(localize(L"截图  Ctrl+1")==L"Capture  Ctrl+1");
    assert(localize(L"录屏  未设置")==L"Record  Not set");
    assert(localize(L"线宽：12 像素")==L"Stroke: 12 px");
    assert(localize(L"GIF 帧率   60 帧/秒")==L"GIF frame rate   60 fps");
    english=false;
    assert(localize(L"Capture  Ctrl+1")==L"截图  Ctrl+1");
    assert(localize(L"Record  Not set")==L"录屏  未设置");
    assert(localize(L"Stroke: 12 px")==L"线宽：12 像素");
    std::cout<<"Language switching and dynamic values passed. / 中英文切换及动态值通过。\n";
}
