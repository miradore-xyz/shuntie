// 录屏状态机：暂停时封存片段，继续时创建新片段；导出不包含暂停等待的时间。
#include <fstream>

Recorder::~Recorder(){stop();if(worker.joinable())worker.join();for(HANDLE h:{stopEvent,pauseEvent,resumeEvent,copyEvent})CloseHandle(h);}
RecordingStatus Recorder::status(){std::lock_guard<std::mutex> lock(mutex);return current;}
bool Recorder::busy(){auto p=status().phase;return p==RecordingStatus::Starting||p==RecordingStatus::Recording||p==RecordingStatus::Paused||p==RecordingStatus::Stopping||p==RecordingStatus::Exporting;}
void Recorder::update(RecordingStatus::Phase p,const std::wstring& m){std::lock_guard<std::mutex> lock(mutex);current.phase=p;current.message=m;}
bool Recorder::start(RECT area,const std::wstring& root,const std::wstring& output,int fps){
    if(busy())return false;if(worker.joinable())worker.join();for(HANDLE h:{stopEvent,pauseEvent,resumeEvent,copyEvent})ResetEvent(h);
    {std::lock_guard<std::mutex> lock(mutex);current={};current.phase=RecordingStatus::Starting;current.message=L"正在启动录制…";}
    worker=std::thread(&Recorder::run,this,area,root,output,fps);return true;
}
void Recorder::stop(){SetEvent(stopEvent);}
void Recorder::pause(){if(status().phase==RecordingStatus::Recording)SetEvent(pauseEvent);}
void Recorder::resume(){if(status().phase==RecordingStatus::Paused)SetEvent(resumeEvent);}
void Recorder::copySnapshot(){auto p=status().phase;if(p==RecordingStatus::Recording||p==RecordingStatus::Paused)SetEvent(copyEvent);}

namespace {
std::string utf8(const std::wstring& value){int n=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),out.data(),n,nullptr,nullptr);return out;}
bool mergeSegments(const std::wstring& ff,const std::vector<std::wstring>& segments,const std::wstring& video,const std::wstring& log){
    if(segments.empty())return false;if(segments.size()==1)return CopyFileW(segments[0].c_str(),video.c_str(),FALSE)!=FALSE;
    std::wstring list=video+L".列表.txt";std::ofstream file(list.c_str(),std::ios::binary);
    for(auto path:segments){std::replace(path.begin(),path.end(),L'\\',L'/');auto encoded=utf8(path);std::string escaped;for(char c:encoded){if(c=='\'')escaped+="'\\''";else escaped+=c;}file<<"file '"<<escaped<<"'\n";}
    file.close();Child child;bool ok=launch(child,ff,L"-hide_banner -nostdin -y -f concat -safe 0 -i "+quote(list)+L" -c copy -movflags +faststart "+quote(video),log)&&result(child)==0;DeleteFileW(list.c_str());return ok&&nonempty(video);
}
// 使用独立转换器保存完整色度，并复用未变化的画面。
bool encodeGif(const std::wstring& ff,const std::wstring& gifski,const std::wstring& video,const std::wstring& gif,const std::wstring& log,int fps,int width,int height){
    fps=std::clamp(fps,1,50);
    std::wstring helper=gifski.substr(0,gifski.find_last_of(L"\\/"))+L"\\gifski-export.exe";if(nonempty(helper)){Child process;return launch(process,helper,quote(ff)+L" "+quote(gifski)+L" "+quote(video)+L" "+quote(gif)+L" "+quote(log)+L" "+std::to_wstring(fps)+L" "+std::to_wstring(width)+L" "+std::to_wstring(height),log)&&result(process)==0&&nonempty(gif);}
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE read=nullptr,write=nullptr;if(!CreatePipe(&read,&write,&sa,0))return false;
    Child encoder,decoder;bool enc=launch(encoder,gifski,L"--quiet --quality 90 --width "+std::to_wstring(width)+L" --height "+std::to_wstring(height)+L" --fps "+std::to_wstring(fps)+L" -o "+quote(gif)+L" -",log,read);CloseHandle(read);
    bool dec=enc&&launch(decoder,ff,L"-hide_banner -nostdin -i "+quote(video)+L" -vf fps="+std::to_wstring(fps)+L" -pix_fmt yuv420p -f yuv4mpegpipe -",log,nullptr,write);CloseHandle(write);
    DWORD dc=dec?result(decoder):1,ec=enc?result(encoder):1;return dc==0&&ec==0&&nonempty(gif);
}
}
void Recorder::run(RECT area,std::wstring root,std::wstring output,int fps){
    std::wstring ff=root+L"\\bin\\ffmpeg.exe",gifski=root+L"\\bin\\gifski.exe";
    if(!nonempty(ff)||!nonempty(gifski)){update(RecordingStatus::Failed,L"缺少编码器，请保留程序旁的 bin 文件夹。");return;}
    SYSTEMTIME time;GetLocalTime(&time);wchar_t name[96];swprintf(name,96,L"\\录屏_%04d%02d%02d_%02d%02d%02d_%03d",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
    std::wstring base=output+name,video=base+L".mp4",log=base+L".log",parts=base+L".片段";CreateDirectoryW(parts.c_str(),nullptr);
    int width=(area.right-area.left)&~1,height=(area.bottom-area.top)&~1;if(width<2||height<2){update(RecordingStatus::Failed,L"选区过小。");return;}
    std::vector<std::wstring> segments;ULONGLONG elapsed=0;bool needCapture=true,finish=false,copy=false;int chosenBackend=-1,snapshot=0;size_t exportedSegments=0;std::wstring lastSnapshot;
    while(!finish){
        if(needCapture){
            update(RecordingStatus::Starting,L"正在启动录制…");bool captured=false;
            std::wstring segment=parts+L"\\"+std::to_wstring(segments.size())+L".mp4";
            for(int attempt=chosenBackend<0?0:chosenBackend;attempt<2&&!captured;++attempt){
                if(WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0){finish=true;break;}
                RECT monitor{};int index=outputFor(area,monitor);bool hardware=attempt==0&&index>=0;if(attempt==0&&!hardware)continue;
                std::wstring args=L"-hide_banner -y ",backend;
                if(hardware){args+=L"-f lavfi -i ddagrab=output_idx="+std::to_wstring(index)+L":framerate=60:draw_mouse=1:offset_x="+std::to_wstring(area.left-monitor.left)+L":offset_y="+std::to_wstring(area.top-monitor.top)+L":video_size="+std::to_wstring(width)+L"x"+std::to_wstring(height)+L" -c:v h264_nvenc -preset p1 -tune lossless -rc constqp -qp 0 -rgb_mode yuv444 -profile:v high444p ";backend=L"显卡捕获 · NVIDIA 硬件编码";}
                else{args+=L"-f gdigrab -framerate 60 -draw_mouse 1 -offset_x "+std::to_wstring(area.left)+L" -offset_y "+std::to_wstring(area.top)+L" -video_size "+std::to_wstring(width)+L"x"+std::to_wstring(height)+L" -i desktop -c:v libx264rgb -preset ultrafast -crf 0 ";backend=L"兼容捕获 · 软件编码";}
                args+=L"-an -fps_mode cfr -r 60 -movflags +faststart "+quote(segment);
                Child child;ULONGLONG began=GetTickCount64();if(!launch(child,ff,args,log))continue;
                if(WaitForSingleObject(child.process.hProcess,1000)==WAIT_OBJECT_0)continue;
                chosenBackend=attempt;
                {std::lock_guard<std::mutex> lock(mutex);current.backend=backend;current.video=video;current.started=began;current.elapsedMs=elapsed;current.phase=RecordingStatus::Recording;current.message=L"正在录制";}
                HANDLE events[]={stopEvent,pauseEvent,copyEvent,child.process.hProcess};DWORD reason=WaitForMultipleObjects(4,events,FALSE,INFINITE);
                if(reason!=WAIT_OBJECT_0+3){update(RecordingStatus::Stopping,L"正在保存片段…");DWORD bytes;if(child.input){WriteFile(child.input,"q\n",2,&bytes,nullptr);CloseHandle(child.input);child.input=nullptr;}}
                if(result(child)!=0||!nonempty(segment)||reason==WAIT_OBJECT_0+3){update(RecordingStatus::Failed,L"录制中断，已录片段保留；详情见同名日志。");return;}
                elapsed+=GetTickCount64()-began;segments.push_back(segment);captured=true;finish=reason==WAIT_OBJECT_0;copy=reason==WAIT_OBJECT_0+2;
                {std::lock_guard<std::mutex> lock(mutex);current.elapsedMs=elapsed;current.started=0;}
            }
            if(!captured&&!finish){update(RecordingStatus::Failed,L"无法启动录制，请查看输出目录中的日志。");return;}
            needCapture=false;
        }
        if(finish)break;
        if(copy){
            update(RecordingStatus::Exporting,L"正在生成并复制 GIF…");std::wstring gif=base+L"_快照"+std::to_wstring(++snapshot)+L".gif";
            bool ok=mergeSegments(ff,segments,video,log)&&encodeGif(ff,gifski,video,gif,log,fps,width,height);if(ok){exportedSegments=segments.size();lastSnapshot=gif;}
            {std::lock_guard<std::mutex> lock(mutex);if(ok){current.gif=gif;++current.copyVersion;}current.phase=RecordingStatus::Paused;current.message=ok?L"GIF 已生成，录制保持暂停":L"GIF 导出失败，可重试或继续录制";}
            copy=false;
        }else update(RecordingStatus::Paused,L"已暂停");
        HANDLE events[]={stopEvent,resumeEvent,copyEvent};DWORD reason=WaitForMultipleObjects(3,events,FALSE,INFINITE);
        finish=reason==WAIT_OBJECT_0;needCapture=reason==WAIT_OBJECT_0+1;copy=reason==WAIT_OBJECT_0+2;
    }
    if(segments.empty()){RemoveDirectoryW(parts.c_str());update(RecordingStatus::Idle,L"已取消录制");return;}
    update(RecordingStatus::Exporting,L"正在保存视频并导出 GIF…");std::wstring gif=base+L".gif";
    bool ready=exportedSegments==segments.size()&&!lastSnapshot.empty()&&nonempty(video)&&CopyFileW(lastSnapshot.c_str(),gif.c_str(),FALSE);
    if(!ready&&(!mergeSegments(ff,segments,video,log)||!encodeGif(ff,gifski,video,gif,log,fps,width,height))){update(RecordingStatus::Failed,L"导出失败，已录片段保留；详情见同名日志。");return;}
    // 成功输出完整视频和 GIF 后才清理本次会话的片段文件。
    for(const auto& segment:segments)DeleteFileW(segment.c_str());RemoveDirectoryW(parts.c_str());
    {std::lock_guard<std::mutex> lock(mutex);current.gif=gif;current.video=video;current.phase=RecordingStatus::Complete;current.message=L"GIF 已导出";}
}
