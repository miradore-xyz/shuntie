#ifndef UNICODE
#define UNICODE
#endif
#include "../src/recorder.cpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

int wmain(){
    // 1. 原生 2K 优先压缩计划单元测试
    {
        auto p25=planVideoCompression(25.0,2560,1600);
        assert(p25.outWidth==2560&&p25.outHeight==1600);
        assert((p25.outWidth%2)==0&&(p25.outHeight%2)==0);
        assert(p25.outFps==30&&p25.crf==27&&p25.targetKbps>=5500);

        auto p60=planVideoCompression(60.0,2560,1440);
        assert(p60.outWidth==2560&&p60.outHeight==1440);
        assert(p60.outFps==24&&p60.crf==28&&p60.targetKbps==2346);

        auto p300=planVideoCompression(300.0,2560,1440);
        assert(p300.outWidth==960&&p300.outHeight==540);
        assert(p300.outFps==12&&p300.targetKbps==469);

        auto pPortrait=planVideoCompression(60.0,1440,2560);
        assert(pPortrait.outWidth==1440&&pPortrait.outHeight==2560&&pPortrait.outFps==24);

        auto pSmall=planVideoCompression(25.0,640,480);
        assert(pSmall.outWidth==640&&pSmall.outHeight==480&&pSmall.outFps==30);

        auto pLong=planVideoCompression(1800.0,2560,1440);
        assert(pLong.targetKbps>=120&&pLong.outWidth==640&&pLong.outHeight==360&&pLong.outFps==10);
    }

    wchar_t current[32768];
    GetCurrentDirectoryW(32768,current);
    std::wstring base=current;
    std::wstring ff=base+L"\\dist\\bin\\ffmpeg.exe";
    assert(nonempty(ff));

    std::filesystem::path tmpDir=std::filesystem::path(base)/L"tools"/L"video-test-tmp";
    std::error_code ec;
    std::filesystem::remove_all(tmpDir,ec);
    std::filesystem::create_directories(tmpDir,ec);
    struct Cleanup {
        std::filesystem::path dir;
        ~Cleanup(){std::error_code e;std::filesystem::remove_all(dir,e);}
    } cleanup{tmpDir};

    std::wstring log=(tmpDir/L"compress.log").wstring();
    auto generateSample=[&](const std::wstring& outPath,int w,int h,int fps,int durSec,int noiseLevel,int crf)->bool{
        std::wstring vf=L"testsrc2=size="+std::to_wstring(w)+L"x"+std::to_wstring(h)+L":rate="+std::to_wstring(fps)+L":duration="+std::to_wstring(durSec);
        if(noiseLevel>0)vf+=L",noise=alls="+std::to_wstring(noiseLevel)+L":allf=t+u";
        std::wstring args=L"-hide_banner -nostdin -y -f lavfi -i "+quote(vf)+
            L" -c:v libx264 -preset ultrafast -crf "+std::to_wstring(crf)+L" -pix_fmt yuv420p -an "+quote(outPath);
        Child child;
        return launch(child,ff,args,log)&&result(child)==0&&nonempty(outPath);
    };

    // 2. 真实 25 秒 2K (2560x1440) 桌面操作模拟：保留原生 2560x1440 分辨率并压到 12MB 以内
    {
        std::wstring raw=(tmpDir/L"raw_2k_25s.mp4").wstring();
        std::wstring out=(tmpDir/L"out_2k_25s.mp4").wstring();
        std::wstring args=L"-hide_banner -nostdin -y "
            L"-f lavfi -i \"color=c=0x1e2228:s=2560x1440:r=30:d=25\" "
            L"-f lavfi -i \"testsrc2=s=1280x800:r=30:d=25\" "
            L"-filter_complex \"[0:v][1:v]overlay=x=640+120*sin(t):y=320+60*cos(t),drawgrid=w=120:h=120:t=1:c=0x2d333b\" "
            L"-c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p -an "+quote(raw);
        Child child;
        assert(launch(child,ff,args,log)&&result(child)==0&&nonempty(raw));
        uint64_t rawBytes=fileSizeBytes(raw);
        assert(rawBytes>kVideoSkipCompressBytes);

        ULONGLONG t0=GetTickCount64();
        bool ok=compressVideoToLimit(ff,raw,out,log,25000,2560,1440,kVideoMaxBytes);
        double elapsedSec=double(GetTickCount64()-t0)/1000.0;
        assert(ok);
        uint64_t outBytes=fileSizeBytes(out);
        assert(outBytes>0&&outBytes<=12ull*1024*1024);
        double dur=probeVideoDurationSec(ff,out,0.0);
        assert(std::fabs(dur-25.0)<=1.5);

        std::wstring resLog=(tmpDir/L"res_2k.log").wstring();
        DeleteFileW(resLog.c_str());
        Child probeRes;
        if(launch(probeRes,ff,L"-hide_banner -nostdin -i "+quote(out),resLog))result(probeRes);
        std::ifstream rin(resLog.c_str(),std::ios::binary);
        std::ostringstream rss;rss<<rin.rdbuf();
        assert(rss.str().find("2560x1440")!=std::string::npos);

        std::cout<<"[Case 0: 25s 2K desktop] raw="<<(double(rawBytes)/(1024.0*1024.0))
                 <<" MB -> 2560x1440 compressed="<<(double(outBytes)/(1024.0*1024.0))
                 <<" MB ("<<outBytes<<" B) in "<<elapsedSec<<"s, dur="<<dur<<"s\n";
    }

    // 3. 25 秒高动态 1080p 视频（原始远超 20MB）压缩测试
    {
        std::wstring raw=(tmpDir/L"raw25.mp4").wstring();
        std::wstring out=(tmpDir/L"out25.mp4").wstring();
        assert(generateSample(raw,1920,1080,30,25,10,18));
        uint64_t rawBytes=fileSizeBytes(raw);
        assert(rawBytes>kVideoMaxBytes);
        ULONGLONG t0=GetTickCount64();
        bool ok=compressVideoToLimit(ff,raw,out,log,25000,1920,1080,kVideoMaxBytes);
        double elapsedSec=double(GetTickCount64()-t0)/1000.0;
        assert(ok);
        uint64_t outBytes=fileSizeBytes(out);
        assert(outBytes>0&&outBytes<=kVideoMaxBytes);
        double dur=probeVideoDurationSec(ff,out,0.0);
        assert(std::fabs(dur-25.0)<=1.5);
        std::cout<<"[Case 1: 25s 1080p noise] raw="<<(double(rawBytes)/(1024.0*1024.0))
                 <<" MB -> compressed="<<(double(outBytes)/(1024.0*1024.0))
                 <<" MB ("<<outBytes<<" B) in "<<elapsedSec<<"s, dur="<<dur<<"s\n";
    }

    // 4. 60 秒高动态 1080p 视频压缩测试
    {
        std::wstring raw=(tmpDir/L"raw60.mp4").wstring();
        std::wstring out=(tmpDir/L"out60.mp4").wstring();
        assert(generateSample(raw,1920,1080,30,60,10,20));
        uint64_t rawBytes=fileSizeBytes(raw);
        assert(rawBytes>kVideoMaxBytes);
        ULONGLONG t0=GetTickCount64();
        bool ok=compressVideoToLimit(ff,raw,out,log,60000,1920,1080,kVideoMaxBytes);
        double elapsedSec=double(GetTickCount64()-t0)/1000.0;
        assert(ok);
        uint64_t outBytes=fileSizeBytes(out);
        assert(outBytes>0&&outBytes<=kVideoMaxBytes);
        double dur=probeVideoDurationSec(ff,out,0.0);
        assert(std::fabs(dur-60.0)<=1.5);
        std::cout<<"[Case 2: 60s 1080p noise] raw="<<(double(rawBytes)/(1024.0*1024.0))
                 <<" MB -> compressed="<<(double(outBytes)/(1024.0*1024.0))
                 <<" MB ("<<outBytes<<" B) in "<<elapsedSec<<"s, dur="<<dur<<"s\n";
    }

    // 5. 8 秒小体积视频直接快速直通（不重复转码）
    {
        std::wstring raw=(tmpDir/L"raw8.mp4").wstring();
        std::wstring out=(tmpDir/L"out8.mp4").wstring();
        assert(generateSample(raw,640,360,15,8,0,21));
        uint64_t rawBytes=fileSizeBytes(raw);
        assert(rawBytes>0&&rawBytes<kVideoSkipCompressBytes);
        bool ok=compressVideoToLimit(ff,raw,out,log,8000,640,360,kVideoMaxBytes);
        assert(ok);
        uint64_t outBytes=fileSizeBytes(out);
        assert(outBytes==rawBytes);
        std::cout<<"[Case 3: 8s small passthrough] raw="<<rawBytes<<" B -> out="<<outBytes<<" B\n";
    }

    std::cout<<"Video compression tests passed. / 视频压缩测试通过。\n";
    return 0;
}
