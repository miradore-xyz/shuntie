#include "../src/recorder.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cassert>
#include <iostream>
static bool waitFor(Recorder& recorder,RecordingStatus::Phase phase){ULONGLONG end=GetTickCount64()+30000;while(GetTickCount64()<end){auto state=recorder.status();if(state.phase==phase)return true;if(state.phase==RecordingStatus::Failed)return false;Sleep(30);}return false;}
static std::vector<char> readFile(const std::wstring& name){std::ifstream file(name.c_str(),std::ios::binary);return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
int main(){wchar_t directory[32768];GetCurrentDirectoryW(32768,directory);std::wstring base=directory,output=base+L"\\artifacts\\session-cache";std::filesystem::create_directories(output);Recorder recorder;assert(recorder.start({100,100,740,580},base+L"\\dist",output,50));assert(waitFor(recorder,RecordingStatus::Recording));Sleep(600);recorder.pause();assert(waitFor(recorder,RecordingStatus::Paused));recorder.copySnapshot();ULONGLONG limit=GetTickCount64()+30000;while(GetTickCount64()<limit&&!recorder.status().copyVersion)Sleep(50);auto snapshot=recorder.status();assert(snapshot.copyVersion&&snapshot.phase==RecordingStatus::Paused);auto before=readFile(snapshot.gif);ULONGLONG began=GetTickCount64();recorder.stop();assert(waitFor(recorder,RecordingStatus::Complete));auto final=recorder.status();assert(!before.empty()&&before==readFile(final.gif));std::ofstream("artifacts/snapshot-cache-result.json")<<"{\"reused_identical_gif\":true,\"finish_ms\":"<<(GetTickCount64()-began)<<"}";std::cout<<"Snapshot reuse passed. / 暂停复制后直接结束复用同一 GIF，通过。\n";}
