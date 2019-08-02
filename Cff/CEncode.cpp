/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CEncode.cpp
*  简要描述:    编码
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "common.h"
#include "CEncode.h"

CEncode::~CEncode()
{
    close();
}

int CEncode::set_enc_callback(EncFrameCallback cb, void* param)
{
    LOCK();

    encframecb_ = cb;
    encframecbparam_ = param;

    return 0;
}

int CEncode::set_encodeid(AVCodecID id)
{
    LOCK();

    int ret = close();
    CHECKFFRET(ret);

    codec_ = avcodec_find_encoder(id);
    if (codec_ == nullptr)
    {
        ret = AVERROR(EINVAL);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }
    codectx_ = avcodec_alloc_context3(codec_);
    if (codectx_ == nullptr)
    {
        ret = AVERROR(ENOMEM);
    }

    return ret;
}

int CEncode::set_video_param(int64_t bitrate, int width, int height, AVRational timebase, AVRational framerate, int gop, int maxbframes, AVPixelFormat fmt)
{
    LOCK();
    int ret = 0;

    if (codectx_ == nullptr || codec_ == nullptr)
    {
        ret = AVERROR(EINVAL);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }

    codectx_->bit_rate = bitrate;
    codectx_->width = width;
    codectx_->height = height;
    codectx_->time_base = timebase;
    codectx_->framerate = framerate;
    codectx_->gop_size = gop;
    codectx_->max_b_frames = maxbframes;
    codectx_->pix_fmt = fmt;
    codectx_->codec_type = AVMEDIA_TYPE_VIDEO;

    return avcodec_open2(codectx_, codec_, nullptr);
}   

int CEncode::set_audio_param(int64_t bitrate, int samplerate, uint64_t channellayout, int channels, AVSampleFormat fmt, int& framesize)
{
    LOCK();
    int ret = 0;

    if (codectx_ == nullptr || codec_ == nullptr)
    {
        ret = AVERROR(EINVAL);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }

    codectx_->bit_rate = bitrate;
    codectx_->sample_rate = samplerate;
    codectx_->channel_layout = channellayout;
    codectx_->channels = channels;
    codectx_->sample_fmt = fmt;
    codectx_->codec_type = AVMEDIA_TYPE_AUDIO;

    ret = avcodec_open2(codectx_, codec_, nullptr);
    CHECKFFRET(ret);
    framesize = codectx_->frame_size;

    return ret;
}

int CEncode::get_codectx(const AVCodecContext*& codectx)
{
    codectx = codectx_;
    return 0;
}

int CEncode::encode(const AVFrame* frame)
{
    LOCK();
    int ret = 0;

    if (codectx_ == nullptr)
    {
        ret = AVERROR(EINVAL);
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
        return ret;
    }

    ret = avcodec_send_frame(codectx_, frame);
    CHECKFFRET(ret);

    while (ret >= 0) 
    {
        ret = avcodec_receive_packet(codectx_, &pkt_);
        CHECKFFRET(ret);

        if (encframecb_ != nullptr)
        {
            encframecb_(&pkt_, encframecbparam_);
        }
        av_packet_unref(&pkt_);
    }

    return ret;
}

int CEncode::close()
{
    LOCK();

    if (codectx_ != nullptr)
    {
        encode(nullptr);
    }
    avcodec_free_context(&codectx_);

    return 0;
}