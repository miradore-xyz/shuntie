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
    assert(localize(L"长截图  Alt+L")==L"Long capture  Alt+L");
    assert(localize(L"录 GIF  Alt+G")==L"Record GIF  Alt+G");
    assert(localize(L"录视频  Alt+V")==L"Record video  Alt+V");
    assert(localize(L"线宽：12 像素")==L"Stroke: 12 px");
    assert(localize(L"GIF 帧率   60 帧/秒")==L"GIF frame rate   60 fps");
    assert(localize(L"GIF 画质   2K 均衡 (≤15MB)")==L"GIF quality   2K Balanced (≤15MB)");
    assert(localize(L"GIF 画质   均衡 (≤12MB)")==L"GIF quality   Balanced (≤12MB)");
    assert(localize(L"鼠标点击显示   已开启")==L"Click indicator   On");
    assert(localize(L"长截图")==L"Long capture");
    assert(localize(L"向下滚动自动拼接")==L"Scroll down to stitch");
    assert(localize(L"录视频（MP4）")==L"Record video (MP4)");
    assert(localize(L"录视频 (MP4) / 停止")==L"Record MP4 / Finish");
    assert(localize(L"录 GIF / 停止")==L"Record GIF / Finish");
    assert(localize(L"录视频")==L"Record video");
    assert(localize(L"视频已导出 · 14.2 MB")==L"Video exported · 14.2 MB");
    assert(localize(L"已导出 · GIF 5.8 MB · MP4 6.2 MB")==L"Exported · GIF 5.8 MB · MP4 6.2 MB");
    assert(localize(L"GIF 已导出 · 8.4 MB")==L"GIF exported · 8.4 MB");
    assert(localize(L"打开视频")==L"Open video");
    assert(localize(L"压缩中…")==L"Compressing…");
    english=false;
    assert(localize(L"Capture  Ctrl+1")==L"截图  Ctrl+1");
    assert(localize(L"Record  Not set")==L"录屏  未设置");
    assert(localize(L"Long capture  Alt+L")==L"长截图  Alt+L");
    assert(localize(L"Record GIF  Alt+G")==L"录 GIF  Alt+G");
    assert(localize(L"Record video  Alt+V")==L"录视频  Alt+V");
    assert(localize(L"Stroke: 12 px")==L"线宽：12 像素");
    assert(localize(L"GIF quality   2K Balanced (≤15MB)")==L"GIF 画质   2K 均衡 (≤15MB)");
    assert(localize(L"GIF quality   Balanced (≤12MB)")==L"GIF 画质   均衡 (≤12MB)");
    assert(localize(L"Click indicator   On")==L"鼠标点击显示   已开启");
    assert(localize(L"Long capture")==L"长截图");
    assert(localize(L"Scroll down to stitch")==L"向下滚动自动拼接");
    assert(localize(L"Record video (MP4)")==L"录视频（MP4）");
    assert(localize(L"Record MP4 / Finish")==L"录视频 (MP4) / 停止");
    assert(localize(L"Record GIF / Finish")==L"录 GIF / 停止");
    assert(localize(L"Record video")==L"录视频");
    assert(localize(L"Video exported · 14.2 MB")==L"视频已导出 · 14.2 MB");
    assert(localize(L"Exported · GIF 5.8 MB · MP4 6.2 MB")==L"已导出 · GIF 5.8 MB · MP4 6.2 MB");
    assert(localize(L"GIF exported · 8.4 MB")==L"GIF 已导出 · 8.4 MB");
    assert(localize(L"Open video")==L"打开视频");
    assert(localize(L"Compressing…")==L"压缩中…");
    std::cout<<"Language switching and dynamic values passed. / 中英文切换及动态值通过。\n";
}
