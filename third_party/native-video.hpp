// SPDX-License-Identifier: AGPL-3.0-or-later
// 在转换器内解码与缩放，避免原尺寸 RGB 帧经过跨进程管道反复复制。
#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

struct NativeVideo {
    AVFormatContext* container=nullptr;
    AVCodecContext* decoder=nullptr;
    AVFrame* frame=nullptr;
    AVFrame* outputFrame=nullptr;
    AVPacket* packet=nullptr;
    SwsContext* scaler=nullptr;
    int stream=-1,error=-1;
    int scalerSrcW=0,scalerSrcH=0,scalerSrcFmt=-1,scalerDstW=0,scalerDstH=0;
    bool draining=false,firstTimeSet=false;
    double firstTime=-1,lastEnd=0;
    NativeVideo(const std::wstring& path){
        auto file=utf8(path);error=avformat_open_input(&container,file.c_str(),nullptr,nullptr);
        if(error<0)return;error=avformat_find_stream_info(container,nullptr);if(error<0)return;
        const AVCodec* codec=nullptr;stream=av_find_best_stream(container,AVMEDIA_TYPE_VIDEO,-1,-1,&codec,0);if(stream<0){error=stream;return;}
        decoder=avcodec_alloc_context3(codec);if(!decoder){error=AVERROR(ENOMEM);return;}
        error=avcodec_parameters_to_context(decoder,container->streams[stream]->codecpar);if(error<0)return;
        decoder->thread_count=std::clamp(int(std::thread::hardware_concurrency()),2,16);
        error=avcodec_open2(decoder,codec,nullptr);if(error<0)return;
        frame=av_frame_alloc();outputFrame=av_frame_alloc();packet=av_packet_alloc();if(!frame||!outputFrame||!packet)error=AVERROR(ENOMEM);
    }
    ~NativeVideo(){sws_freeContext(scaler);av_frame_free(&frame);av_frame_free(&outputFrame);av_packet_free(&packet);avcodec_free_context(&decoder);avformat_close_input(&container);}
    int sourceWidth()const{return decoder?decoder->width:0;}
    int sourceHeight()const{return decoder?decoder->height:0;}
    bool next(){
        if(error<0)return false;
        for(;;){
            int code=avcodec_receive_frame(decoder,frame);
            if(code==0)return true;
            if(code==AVERROR_EOF)return false;
            if(code!=AVERROR(EAGAIN)){error=code;return false;}
            for(;;){
                code=av_read_frame(container,packet);
                if(code<0){
                    if(code!=AVERROR_EOF){error=code;return false;}
                    if(draining)return false;draining=true;code=avcodec_send_packet(decoder,nullptr);break;
                }
                if(packet->stream_index!=stream){av_packet_unref(packet);continue;}
                code=avcodec_send_packet(decoder,packet);av_packet_unref(packet);break;
            }
            if(code<0){error=code;return false;}
        }
    }
    unsigned sampleIndex(int fps){
        auto video=container->streams[stream];double unit=av_q2d(video->time_base);
        double time=frame->best_effort_timestamp==AV_NOPTS_VALUE?lastEnd:frame->best_effort_timestamp*unit;
        if(!firstTimeSet){firstTime=time;firstTimeSet=true;}
        double duration=frame->duration>0?frame->duration*unit:1./av_q2d(av_guess_frame_rate(container,video,frame));
        lastEnd=time+duration;
        return unsigned(std::max(0.,std::round((time-firstTime)*fps)));
    }
    unsigned endIndex(int fps)const{return unsigned(std::max(1.,std::round((lastEnd-firstTime)*fps)));}
    bool rgb(std::vector<BYTE>& pixels,int dstWidth,int dstHeight){
        if(frame->width<=0||frame->height<=0||dstWidth<=0||dstHeight<=0){error=AVERROR(EINVAL);return false;}
        size_t need=size_t(dstWidth)*size_t(dstHeight)*4;
        if(pixels.size()<need)pixels.resize(need);
        if(!scaler||scalerSrcW!=frame->width||scalerSrcH!=frame->height||scalerSrcFmt!=frame->format||scalerDstW!=dstWidth||scalerDstH!=dstHeight){
            sws_freeContext(scaler);scaler=nullptr;
            scaler=sws_alloc_context();if(!scaler){error=AVERROR(ENOMEM);return false;}
            av_opt_set_int(scaler,"srcw",frame->width,0);av_opt_set_int(scaler,"srch",frame->height,0);
            av_opt_set_int(scaler,"dstw",dstWidth,0);av_opt_set_int(scaler,"dsth",dstHeight,0);
            av_opt_set_int(scaler,"src_format",frame->format,0);av_opt_set_int(scaler,"dst_format",AV_PIX_FMT_RGBA,0);
            av_opt_set_int(scaler,"threads",std::clamp(int(std::thread::hardware_concurrency()),2,16),0);
            int flags=(dstWidth<frame->width||dstHeight<frame->height)?SWS_AREA:SWS_BICUBIC;
            av_opt_set_int(scaler,"sws_flags",flags,0);
            error=sws_init_context(scaler,nullptr,nullptr);if(error<0){sws_freeContext(scaler);scaler=nullptr;return false;}
            int matrix=frame->colorspace==AVCOL_SPC_BT709?SWS_CS_ITU709:frame->colorspace==AVCOL_SPC_BT2020_NCL?SWS_CS_BT2020:SWS_CS_ITU601;
            auto coefficients=sws_getCoefficients(matrix);
            sws_setColorspaceDetails(scaler,coefficients,frame->color_range==AVCOL_RANGE_JPEG,coefficients,1,0,1<<16,1<<16);
            scalerSrcW=frame->width;scalerSrcH=frame->height;scalerSrcFmt=frame->format;scalerDstW=dstWidth;scalerDstH=dstHeight;
        }
        av_frame_unref(outputFrame);outputFrame->format=AV_PIX_FMT_RGBA;outputFrame->width=dstWidth;outputFrame->height=dstHeight;
        // 使用已分配的帧缓冲；新版整帧接口会执行并行条带转换。
        outputFrame->buf[0]=av_buffer_create(pixels.data(),need,[](void*,uint8_t*){},nullptr,0);
        if(!outputFrame->buf[0]){error=AVERROR(ENOMEM);return false;}
        outputFrame->data[0]=pixels.data();outputFrame->linesize[0]=dstWidth*4;
        error=sws_scale_frame(scaler,outputFrame,frame);if(error<0)return false;
        return true;
    }
};
