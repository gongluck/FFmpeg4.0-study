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

#include <string>
#include <mutex>

class CEncode
{
public:
    virtual ~CEncode();
    // 编码帧回调声明
    typedef void (*EncFrameCallback)(const AVPacket* packet, void* param);

    // 设置编码帧回调 
    bool set_enc_callback(EncFrameCallback cb, void* param, std::string& err);

    // 设置编码器
    bool set_encodeid(AVCodecID id, std::string& err);

    // 设置视频参数
    bool set_video_param(int64_t bitrate, int width, int height, AVRational timebase, AVRational framerate, int gop, int maxbframes, AVPixelFormat fmt, std::string& err);
    // 设置音频参数
    bool set_audio_param(int64_t bitrate, int samplerate, uint64_t channellayout, int channels, AVSampleFormat fmt, int& framesize, std::string& err);
    
    // 编码
    bool encode(const AVFrame* frame, std::string& err);

    // 关闭
    bool close(std::string& err);

private:
    std::recursive_mutex mutex_;

    EncFrameCallback encframecb_ = nullptr;
    void* encframecbparam_ = nullptr;

    //ffmpeg
    AVCodecContext* codectx_ = nullptr;
    AVCodec* codec_ = nullptr;
    AVPacket pkt_;
};

#endif//__CENCODE_H__