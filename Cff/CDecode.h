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

class CDecode
{
public:
    // 状态
    enum STATUS{STOP, DECODING};
    // 解码帧回调声明
    typedef void (*DecFrameCallback)(const AVFrame* frame, void* param);

    // 设置解码帧回调 
    bool set_dec_callback(DecFrameCallback cb, void* param, std::string& err);

    // 设置硬解
    bool set_hwdec_type(AVHWDeviceType hwtype, bool trans, std::string& err);

    // 设置解码器
    bool copy_param(const AVCodecParameters* par, std::string& err);

    // 开始解码
    bool decode(const AVPacket* packet, std::string& err);

private:
    STATUS status_ = STOP;
    std::recursive_mutex mutex_;

    DecFrameCallback decframecb_ = nullptr;
    void* decframecbparam_ = nullptr;

    //ffmpeg
    AVCodecContext* codectx_ = nullptr;
    AVHWDeviceType hwtype_ = AV_HWDEVICE_TYPE_NONE;
    AVPixelFormat hwfmt_ = AV_PIX_FMT_NONE;
    bool trans_ = false;
};

#endif//__CDECODE_H__