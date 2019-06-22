#include "common.h"
#include "CDecode.h"

bool CDecode::set_dec_callback(DecFrameCallback cb, void* param, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";
    decframecb_ = cb;
    decframecbparam_ = param;
    return true;
}

bool CDecode::set_hwdec_type(AVHWDeviceType hwtype, bool trans, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";
    hwtype_ = hwtype;
    trans_ = trans;
    return true;
}

bool CDecode::copy_param(const AVCodecParameters* par, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";
    int ret;

    AVCodec* codec = avcodec_find_decoder(par->codec_id);
    if (codec == nullptr) 
    {
        err = "avcodec_find_decoder return nullptr";
        return false;
    }
    avcodec_free_context(&codectx_);
    codectx_ = avcodec_alloc_context3(codec);
    if (codectx_ == nullptr)
    {
        err = "avcodec_alloc_context3 return nullptr";
        return false;
    }
    ret = avcodec_parameters_to_context(codectx_, par);
    if (ret < 0)
    {
        avcodec_free_context(&codectx_);
        err = av_err2str(ret);
        return false;
    }
    ret = avcodec_open2(codectx_, codec, NULL);
    if (ret < 0)
    {
        avcodec_free_context(&codectx_);
        err = av_err2str(ret);
        return false;
    }
 
    return true;
}

bool CDecode::decode(const AVPacket* packet, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    int ret;
    err = "opt succeed.";

    // 发送将要解码的数据
    ret = avcodec_send_packet(codectx_, packet);
    CHECKFFRET(ret);
    AVFrame* frame = av_frame_alloc();
    AVFrame* traframe = av_frame_alloc();
    if (frame == nullptr || traframe == nullptr)
    {
        err = "av_frame_alloc() return nullptr.";
        av_frame_free(&frame);
        av_frame_free(&traframe);
        return false;
    }

    while (ret >= 0)
    {
        // 接收解码数据
        ret = avcodec_receive_frame(codectx_, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            // 不完整或者EOF
            break;
        }
        else if (ret < 0)
        {
            // 其他错误
            err = av_err2str(ret);
            break;
        }
        else
        {
            // 得到解码数据
            if (decframecb_ != nullptr)
            {
                if (hwtype_ != AV_HWDEVICE_TYPE_NONE // 使用硬解
                    && frame->format == hwfmt_ // 硬解格式
                    && trans_ // 转换
                    )
                {
                    ret = av_hwframe_transfer_data(traframe, frame, 0);
                    if (ret < 0)
                    {
                        err = av_err2str(ret);
                        break;
                    }
                    else
                    {
                        traframe->pts = frame->pts;
                        traframe->pkt_dts = frame->pkt_dts;
                        traframe->pkt_duration = frame->pkt_duration;
                        decframecb_(traframe, decframecbparam_);
                    }
                }
                else
                {
                    decframecb_(frame, decframecbparam_);
                }
            }
            // 这里没有直接break，是因为存在再次调用avcodec_receive_frame能拿到新数据的可能
        }
    }

    av_frame_free(&frame);
    av_frame_free(&traframe);
    return true;
}
