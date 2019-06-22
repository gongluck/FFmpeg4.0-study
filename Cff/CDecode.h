#ifndef __CDECODE_H__
#define __CDECODE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>

#ifdef __cplusplus
}
#endif

#include <string>
#include <mutex>
#include <thread>

class CDecode
{
public:
    ~CDecode();

    // 状态
    enum STATUS{STOP, DECODING};
    // 帧类型
    enum FRAMETYPE {ERR, VIDEO, AUDIO};
    // 状态通知回调声明
    typedef void (*DecStatusCallback)(STATUS status, std::string err, void* param);
    // 解码帧回调声明
    typedef void (*DecFrameCallback)(const AVFrame* frame, FRAMETYPE frametype, int64_t timestamp, void* param);

    // 设置输入
    bool set_input(const std::string& input, std::string& err);
    // 获取输入
    const std::string& get_input(std::string& err);

    // 设置解码帧回调 
    bool set_dec_callback(DecFrameCallback cb, void* param, std::string& err);
    // 设置解码状态变化回调
    bool set_dec_status_callback(DecStatusCallback cb, void* param, std::string& err);

    // 设置硬解
    bool set_hwdec_type(AVHWDeviceType hwtype, bool trans, std::string& err);

    // 开始解码
    bool begindecode(std::string& err);
    // 停止解码
    bool stopdecode(std::string& err);

    // 跳转到指定秒
    bool seek(int64_t timestamp, int flags, std::string& err);

    // 启用设备采集
    bool device_register_all(std::string& err);
    // 设置输入格式
    bool set_input_format(std::string fmt, std::string& err);
    // 设置附加参数
    bool set_dic_opt(std::string key, std::string value, std::string& err);
    // 清理设置
    bool free_opt(std::string& err);

private:
    // 解码线程
    bool decodethread();

private:
    STATUS status_ = STOP;
    std::recursive_mutex mutex_;

    std::string input_;
    int vindex_ = -1;
    int aindex_ = -1;
    std::thread decodeth_;

    DecStatusCallback decstatuscb_ = nullptr;
    void* decstatuscbparam_ = nullptr;
    DecFrameCallback decframecb_ = nullptr;
    void* decframecbparam_ = nullptr;

    //ffmpeg
    AVFormatContext* fmtctx_ = nullptr;
    AVCodecContext* vcodectx_ = nullptr;
    AVCodecContext* acodectx_ = nullptr;
    AVInputFormat* fmt_ = nullptr;
    AVDictionary* dic_ = nullptr;

    AVHWDeviceType hwtype_ = AV_HWDEVICE_TYPE_NONE;
    AVPixelFormat hwfmt_ = AV_PIX_FMT_NONE;
    bool trans_ = false;
};

#endif//__CDECODE_H__