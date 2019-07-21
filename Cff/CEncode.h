/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CEncode.h
*  简要描述:    编码
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#ifndef __CENCODE_H__
#define __CENCODE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <libavcodec/avcodec.h>

#ifdef __cplusplus
}
#endif

#include <mutex>
#include <functional>

class CEncode
{
public:
    virtual ~CEncode();
    // 编码帧回调声明
    typedef void (*EncFrameCallback)(const AVPacket* packet, void* param);

    // 设置编码帧回调 
    int set_enc_callback(EncFrameCallback cb, void* param);

    // 设置编码器
    int set_encodeid(AVCodecID id);

    // 设置视频参数
    int set_video_param(int64_t bitrate, int width, int height, AVRational timebase, AVRational framerate, int gop, int maxbframes, AVPixelFormat fmt);
    // 设置音频参数
    int set_audio_param(int64_t bitrate, int samplerate, uint64_t channellayout, int channels, AVSampleFormat fmt, int& framesize);

    // 获取编码上下文
    int get_codectx(const AVCodecContext*& codectx);
    
    // 编码
    int encode(const AVFrame* frame);

    // 关闭
    int close();

private:
    std::recursive_mutex mutex_;

    std::function<void(const AVPacket*, void*)> encframecb_ = nullptr;
    void* encframecbparam_ = nullptr;

    //ffmpeg
    AVCodecContext* codectx_ = nullptr;
    AVCodec* codec_ = nullptr;
    AVPacket pkt_ = { 0 };
};

#endif//__CENCODE_H__