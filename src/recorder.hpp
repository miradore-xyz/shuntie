#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mutex>
#include <string>
#include <thread>

struct RecordingStatus {
    enum Phase { Idle, Starting, Recording, Paused, Stopping, Exporting, Complete, Failed } phase=Idle;
    std::wstring message=L"就绪", video, gif, backend;
    ULONGLONG started=0;
    ULONGLONG elapsedMs=0;
    unsigned copyVersion=0;
};
class Recorder {
    std::mutex mutex;
    RecordingStatus current;
    std::thread worker;
    HANDLE stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HANDLE pauseEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE resumeEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE copyEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    void run(RECT area, std::wstring root, std::wstring output, int fps);
    void update(RecordingStatus::Phase phase, const std::wstring& message);
public:
    ~Recorder();
    RecordingStatus status();
    bool busy();
    bool start(RECT area,const std::wstring& root,const std::wstring& output,int gifFps);
    void stop();
    void pause();
    void resume();
    void copySnapshot();
};
// 以文件格式写入剪贴板，保留动画；同时声明“复制”拖放语义。
bool copyFileToClipboard(HWND owner,const std::wstring& file);
