#pragma once
#include <string>
#include <utility>
// 英文界面文本集中维护；每条英文都有对应的中文原文。
inline bool english=false;
inline std::wstring localize(std::wstring value){
    static const std::pair<const wchar_t*,const wchar_t*> dictionary[]={
        {L"截图、标注、贴图。",L"Capture. Mark up. Pin."},{L"录下操作，一键复制 GIF。",L"Record it. Copy as GIF."},
        {L"鼠标悬停查看工具",L"Hover to see the tool"},{L"点击修改快捷键",L"Click to change keys"},
        {L"请按新的快捷键…",L"Press a shortcut…"},{L"未设置",L"Not set"},{L"（占用）",L" (in use)"},
        {L"清除截图快捷键",L"Clear capture shortcut"},{L"清除贴图快捷键",L"Clear pin shortcut"},{L"清除录屏快捷键",L"Clear record shortcut"},
        {L"关闭设置",L"Close settings"},{L"开机启动   已开启",L"Launch at startup   On"},{L"开机启动   已关闭",L"Launch at startup   Off"},
        {L"复制已录 GIF（会暂停）",L"Copy recorded GIF (pauses)"},{L"暂停 / 继续",L"Pause / Resume"},{L"结束录制",L"Finish recording"},
        {L"复制截图（Enter）",L"Copy image (Enter)"},{L"保存（Ctrl+S）",L"Save (Ctrl+S)"},{L"撤销（Ctrl+Z）",L"Undo (Ctrl+Z)"},{L"重做（Ctrl+Y）",L"Redo (Ctrl+Y)"},
        {L"选择（V）",L"Select (V)"},{L"方框（R）",L"Rectangle (R)"},{L"箭头（A）",L"Arrow (A)"},{L"画笔（P）",L"Pen (P)"},{L"马赛克（M）",L"Mosaic (M)"},{L"取消（Esc）",L"Cancel (Esc)"},
        {L"拖动鼠标，截取一块屏幕",L"Drag to capture an area"},{L"按 Esc 取消 · 支持跨屏截图",L"Esc to cancel · Multi-monitor capture"},
        {L"框选录制区域",L"Select recording area"},{L"重新框选",L"Select again"},{L"开始录制",L"Start recording"},{L"录制选区",L"Record selection"},
        {L"选择颜色",L"Choose a color"},{L"颜色：",L"Color: "},{L"颜色：红色",L"Color: Red"},{L"线宽：",L"Stroke: "},{L"线宽 ",L"Stroke "},{L" 像素",L" px"},
        {L"GIF 帧率   ",L"GIF frame rate   "},{L" 帧/秒",L" fps"},{L"开机启动",L"Launch at startup"},
        {L"输出目录",L"Output folder"},{L"退出软件",L"Quit"},{L"收起",L"Hide"},{L"显示主窗口",L"Show window"},
        {L"录屏 / 停止",L"Record / Finish"},{L"复制 GIF",L"Copy GIF"},{L"GIF 已导出",L"GIF exported"},{L"GIF 已复制",L"GIF copied"},
        {L"已暂停 ",L"Paused "},{L"导出中…",L"Exporting…"},{L"准备中…",L"Starting…"},
        {L"选区过小。",L"The selected area is too small."},{L"复制失败，请稍后重试。",L"Copy failed. Please try again."},
        {L"剪贴板暂时被其他程序占用，请再试一次。",L"The clipboard is busy. Please try again."},
        {L"录屏首版需要在单个显示器内框选；截图可以跨屏。",L"Keep the recording area on one monitor. Screenshots may span monitors."},
        {L"该快捷键已被其他程序占用，请换一个组合。",L"This shortcut is already in use. Choose another combination."},
        {L"开机启动设置未能保存。",L"Could not save the startup setting."},{L"保存失败，请检查目标目录。",L"Could not save the image. Check the destination folder."},
        {L"截图生成失败，请重试。",L"Could not create the screenshot. Please try again."},
        {L"剪贴板中没有可用的图片。请先截图，或复制一张图片。",L"No image in the clipboard. Capture or copy an image first."},
        {L"无法读取屏幕。请确认桌面没有锁定。",L"Could not capture the screen. Make sure the desktop is unlocked."},
        {L"无法创建贴图窗口。",L"Could not create the pin window."},
        {L"导出失败，已录片段保留；详情见同名日志。",L"Export failed. Recorded parts were kept; see the log file."},
        {L"录制中断，已录片段保留；详情见同名日志。",L"Recording stopped unexpectedly. Recorded parts were kept; see the log file."},
        {L"无法启动录制，请查看输出目录中的日志。",L"Could not start recording. See the log in the output folder."},
        {L"缺少编码器，请保留程序旁的 bin 文件夹。",L"Encoders are missing. Keep the bin folder beside the app."},
        {L"GIF 动图文件已复制，可粘贴到支持文件的聊天或编辑软件。",L"GIF file copied. Paste it into an app that supports files."},
        {L"GIF 已生成，剪贴板繁忙，请重试复制",L"GIF ready. Clipboard busy; try copying again."},
        {L"瞬贴 · 设置",L"Shuntie · Settings"},{L"瞬贴 · 截图与录屏",L"Shuntie"},{L"瞬贴 · 截图标注",L"Shuntie · Capture"},
        {L"设置",L"Settings"},{L"选择",L"Select"},{L"方框",L"Rectangle"},{L"箭头",L"Arrow"},{L"画笔",L"Pen"},{L"马赛克",L"Mosaic"},
        {L"截图  ",L"Capture  "},{L"贴图  ",L"Pin  "},{L"录屏  ",L"Record  "},
        {L"截图",L"Capture"},{L"贴图",L"Pin"},{L"录屏",L"Record"},{L"保存",L"Save"},{L"取消",L"Cancel"},{L"退出",L"Quit"},
        {L"版本 ",L"Version "},{L"瞬贴",L"Shuntie"}
    };
    // 先匹配完整文本；动态数字和快捷键再使用前缀、后缀替换。
    for(auto& item:dictionary){const wchar_t* from=english?item.first:item.second;const wchar_t* to=english?item.second:item.first;if(value==from)return to;}
    if(!english){for(auto& item:dictionary){std::wstring from=item.second;if(from.size()<4)continue;size_t pos=0;while((pos=value.find(from,pos))!=std::wstring::npos){value.replace(pos,from.size(),item.first);pos+=wcslen(item.first);}}return value;}
    for(auto& item:dictionary){std::wstring from=item.first;size_t pos=0;while((pos=value.find(from,pos))!=std::wstring::npos){value.replace(pos,from.size(),item.second);pos+=wcslen(item.second);}}
    return value;
}
inline void setLabel(HWND hwnd,const std::wstring& text){auto value=localize(text);SetWindowTextW(hwnd,value.c_str());}
