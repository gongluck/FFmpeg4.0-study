/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CDecode.h
*  简要描述:    解码
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#ifndef __CDECODE_H__
#define __CDECODE_H__

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
#include <functional>

class CDecode
{
public:
    virtual ~CDecode();
    // 解码帧回调声明
    typedef void (*DecFrameCallback)(const AVFrame* frame, void* param);

    // 设置解码帧回调 
    int set_dec_callback(DecFrameCallback cb, void* param);

    // 设置硬解
    int set_hwdec_type(AVHWDeviceType hwtype, bool trans);

    // 设置解码器
    int set_codeid(AVCodecID id);
    int copy_param(const AVCodecParameters* par);

    // 打开解码器
    int codec_open();

    // 解码
    int decode(const AVPacket* packet);
    int decode(const void* data, uint32_t size);

    // 清理资源
    int clean_opt();

private:
    std::recursive_mutex mutex_;

    std::function<void(const AVFrame*, void*)> decframecb_ = nullptr;
    void* decframecbparam_ = nullptr;

    //ffmpeg
    AVCodecContext* codectx_ = nullptr;
    AVCodec* codec_ = nullptr;
    AVCodecParserContext* par_ = nullptr;
    AVHWDeviceType hwtype_ = AV_HWDEVICE_TYPE_NONE;
    AVPixelFormat hwfmt_ = AV_PIX_FMT_NONE;
    AVPacket pkt_ = { 0 };
    bool trans_ = false;
};

#endif//__CDECODE_H__