/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CDecode.cpp
*  简要描述:    解码
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "common.h"
#include "CDecode.h"

CDecode::~CDecode()
{
    clean_opt();
}

int CDecode::set_dec_callback(DecFrameCallback cb, void* param)
{
    LOCK();

    decframecb_ = cb;
    decframecbparam_ = param;

    return 0;
}

int CDecode::set_hwdec_type(AVHWDeviceType hwtype, bool trans)
{
    LOCK();

    hwtype_ = hwtype;
    trans_ = trans;

    return 0;
}

int CDecode::set_codeid(AVCodecID id)
{
    LOCK();
    int ret = clean_opt();
    CHECKFFRET(ret);

    do
    {
        codec_ = avcodec_find_decoder(id);
        if (codec_ == nullptr)
        {
            ret = AVERROR(EINVAL);
            av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
            break;
        }
        codectx_ = avcodec_alloc_context3(codec_);
        if (codectx_ == nullptr)
        {
            ret = AVERROR(ENOMEM);
            av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
            break;
        }
        par_ = av_parser_init(codec_->id);
        if (par_ == nullptr)
        {
            ret = 0;
            //ret = AVERROR(EINVAL);
            av_log(nullptr, AV_LOG_WARNING, "%s %d : %ld\n", __FILE__, __LINE__, AVERROR(EINVAL));
            //break;
        }

        if (hwtype_ != AV_HWDEVICE_TYPE_NONE)
        {
            // 查询硬解码支持
            for (int i = 0;; i++)
            {
                const AVCodecHWConfig* config = avcodec_get_hw_config(codec_, i);
                if (config == nullptr)
                {
                    ret = AVERROR(EINVAL);
                    av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
                    break;
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    config->device_type == hwtype_)
                {
                    // 硬解上下文
                    AVBufferRef* hwbufref = nullptr;
                    ret = av_hwdevice_ctx_create(&hwbufref, hwtype_, nullptr, nullptr, 0);
                    if (ret < 0)
                    {
                        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
                        break;
                    }
                    else
                    {
                        codectx_->hw_device_ctx = av_buffer_ref(hwbufref);
                        if (codectx_->hw_device_ctx == nullptr)
                        {
                            ret = AVERROR_BUG;
                            av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
                            break;
                        }
                        av_buffer_unref(&hwbufref);
                        hwfmt_ = config->pix_fmt;
                        return 0;
                    }
                }
            }
            // 执行出错
            break;
        }
        // 执行成功
        return ret;
    } while (true);

    // 执行出错
    clean_opt();
    return ret;
}

int CDecode::copy_param(const AVCodecParameters* par)
{
    LOCK();

    int ret = set_codeid(par->codec_id);
    CHECKFFRET(ret);
    ret = avcodec_parameters_to_context(codectx_, par);
    CHECKFFRET(ret);

    return 0;
}

int CDecode::codec_open()
{
    LOCK();
    int ret = 0;

    if (codectx_ == nullptr || codec_ == nullptr)
    {
        ret = AVERROR(EINVAL);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }
    ret = avcodec_open2(codectx_, codec_, nullptr);
    CHECKFFRET(ret);

    return ret;
}

int CDecode::decode(const AVPacket* packet)
{
    LOCK();
    int ret = 0;

    if (packet == nullptr || codectx_ == nullptr)
    {
        ret = AVERROR(EINVAL);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }
    
    // 发送将要解码的数据
    ret = avcodec_send_packet(codectx_, packet);
    CHECKFFRET(ret);

    AVFrame* frame = av_frame_alloc();
    AVFrame* traframe = av_frame_alloc();
    if (frame == nullptr || traframe == nullptr)
    {
        av_frame_free(&frame);
        av_frame_free(&traframe);
        ret = AVERROR(ENOMEM);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }

    while (ret >= 0)
    {
        // 接收解码数据
        ret = avcodec_receive_frame(codectx_, frame);
        if (ret < 0) 
        {
            // 不完整或者EOF
            // 其他错误
            av_log(nullptr, AV_LOG_DEBUG, "%s %d : %ld\n", __FILE__, __LINE__, ret);
            break;
        }
        else
        {
            // 得到解码数据
            if (decframecb_ != nullptr)
            {
                if (hwtype_ != AV_HWDEVICE_TYPE_NONE // 使用硬解
                    && frame->format == hwfmt_ // 硬解格式
                    && trans_ // 显卡->内存转换
                    )
                {
                    ret = av_hwframe_transfer_data(traframe, frame, 0);
                    if (ret < 0)
                    {
                        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
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
    return ret;
}

int CDecode::decode(const void* data, uint32_t size)
{
    LOCK();
    int ret = 0;

    if (par_ == nullptr || codectx_ == nullptr)
    {
        ret = AVERROR(EINVAL);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }

    int pos = 0;
    while (size > 0) 
    {
        ret = av_parser_parse2(par_, codectx_, &pkt_.data, &pkt_.size, static_cast<const uint8_t*>(data)+pos, size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        CHECKFFRET(ret);
        pos += ret;
        size -= ret;

        if (pkt_.size > 0)
        {
            ret = decode(&pkt_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                av_log(nullptr, AV_LOG_DEBUG, "%s %d : %ld\n", __FILE__, __LINE__, ret);
                av_usleep(10);
                continue;
            }
        }
    }

    return 0;
}

int CDecode::clean_opt()
{
    LOCK();

    codec_ = nullptr;
    av_parser_close(par_);
    par_ = nullptr;
    avcodec_free_context(&codectx_);

    return 0;
}