#include "common.h"
#include "CDecode.h"

CDecode::~CDecode()
{
    std::string err;
    clean_opt(err);
}

bool CDecode::set_dec_callback(DecFrameCallback cb, void* param, std::string& err)
{
    LOCK();
    err = "opt succeed.";

    decframecb_ = cb;
    decframecbparam_ = param;

    return true;
}

bool CDecode::set_hwdec_type(AVHWDeviceType hwtype, bool trans, std::string& err)
{
    LOCK();
    err = "opt succeed.";

    hwtype_ = hwtype;
    trans_ = trans;

    return true;
}

bool CDecode::set_codeid(AVCodecID id, std::string& err)
{
    LOCK();
    err = "opt succeed.";
    int ret = 0;

    if (!clean_opt(err))
    {
        return false;
    }

    do
    {
        codec_ = avcodec_find_decoder(id);
        if (codec_ == nullptr)
        {
            err = "avcodec_find_decoder return nullptr";
            break;
        }

        codectx_ = avcodec_alloc_context3(codec_);
        if (codectx_ == nullptr)
        {
            err = "avcodec_alloc_context3 return nullptr";
            break;
        }

        par_ = av_parser_init(codec_->id);
        if (par_ == nullptr)
        {
            err = "av_parser_init return nullptr";
            break;
        }

        if (hwtype_ != AV_HWDEVICE_TYPE_NONE)
        {
            // 查询硬解码支持
            for (int i = 0;; i++)
            {
                const AVCodecHWConfig* config = avcodec_get_hw_config(codec_, i);
                if (config == nullptr)
                {
                    err = codec_->name + std::string(" not support ") + av_hwdevice_get_type_name(hwtype_);
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
                        err = av_err2str(ret);
                        break;
                    }
                    else
                    {
                        codectx_->hw_device_ctx = av_buffer_ref(hwbufref);
                        if (codectx_->hw_device_ctx == nullptr)
                        {
                            err = "av_buffer_ref(hwbufref) return nullptr.";
                            break;
                        }
                        av_buffer_unref(&hwbufref);
                        hwfmt_ = config->pix_fmt;
                        return true;
                    }
                }
            }
        }
        return true;
    } while (true);

    std::string e;
    clean_opt(e);
    return false;
}

bool CDecode::copy_param(const AVCodecParameters* par, std::string& err)
{
    LOCK();
    err = "opt succeed.";
    int ret = 0;

    if (par == nullptr)
    {
        err = "par is nullptr";
        return false;
    }

    if (!set_codeid(par->codec_id, err))
    {
        return false;
    }
    
    ret = avcodec_parameters_to_context(codectx_, par);
    if (ret < 0)
    {
        clean_opt(err);
        err = av_err2str(ret);
        return false;
    }

    return true;
}

bool CDecode::codec_open(std::string& err)
{
    LOCK();
    err = "opt succeed.";
    int ret = 0;

    if (codectx_ == nullptr || codec_ == nullptr)
    {
        err = "codectx_ is nullptr or codec_ is nullptr";
        return false;
    }

    ret = avcodec_open2(codectx_, codec_, nullptr);
    if (ret < 0)
    {
        err = av_err2str(ret);
        return false;
    }

    return true;
}

bool CDecode::decode(const AVPacket* packet, std::string& err)
{
    LOCK();
    err = "opt succeed.";
    int ret = 0;

    if (packet == nullptr)
    {
        err == "packet is nullptr.";
        return false;
    }

    if (codectx_ == nullptr)
    {
        err = "codectx_ is nullptr.";
        return false;
    }
    
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

bool CDecode::decode(const void* data, uint32_t size, std::string& err)
{
    LOCK();
    err = "opt succeed.";
    int ret = 0;

    int pos = 0;
    while (size > 0) 
    {
        ret = av_parser_parse2(par_, codectx_, &pkt_.data, &pkt_.size, static_cast<const uint8_t*>(data)+pos, size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        CHECKFFRET(ret);
        pos += ret;
        size -= ret;

        if (pkt_.size > 0)
        {
            ret = decode(&pkt_, err);
            CHECKFFRET(ret);
        }
    }

    return true;
}

bool CDecode::clean_opt(std::string& err)
{
    LOCK();
    err = "opt succeed.";

    codec_ = nullptr;
    av_parser_close(par_);
    avcodec_free_context(&codectx_);

    return true;
}