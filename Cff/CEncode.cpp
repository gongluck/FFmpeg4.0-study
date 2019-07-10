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
    std::string err;
    close(err);
}

bool CEncode::set_enc_callback(EncFrameCallback cb, void* param, std::string& err)
{
    LOCK();
    err = "opt succeed.";

    encframecb_ = cb;
    encframecbparam_ = param;

    return true;
}

bool CEncode::set_encodeid(AVCodecID id, std::string& err)
{
    LOCK();
    err = "opt succeed.";

    if (!close(err))
    {
        return false;
    }

    codec_ = avcodec_find_encoder(id);
    if (codec_ == nullptr)
    {
        err = "avcodec_find_encoder return nullptr";
        return false;
    }
    codectx_ = avcodec_alloc_context3(codec_);
    if (codectx_ == nullptr)
    {
        err = "avcodec_alloc_context3 return nullptr";
        return false;
    }

    return true;
}

bool CEncode::set_video_param(int64_t bitrate, int width, int height, AVRational timebase, AVRational framerate, int gop, int maxbframes, AVPixelFormat fmt, std::string& err)
{
    LOCK();
    err = "opt succeed.";
    int ret;

    if (codectx_ == nullptr)
    {
        err = "codectx_ is nullptr.";
        return false;
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

    ret = avcodec_open2(codectx_, codec_, nullptr);
    CHECKFFRET(ret);

    return true;
}

bool CEncode::encode(const AVFrame* frame, std::string& err)
{
    LOCK();
    err = "opt succeed.";
    int ret;

    ret = avcodec_send_frame(codectx_, frame);
    CHECKFFRET(ret);

    while (ret >= 0) 
    {
        ret = avcodec_receive_packet(codectx_, &pkt_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return true;
        } 
        CHECKFFRET(ret);

        if (encframecb_ != nullptr)
        {
            encframecb_(&pkt_, encframecbparam_);
        }
    }

    return true;
}

bool CEncode::close(std::string& err)
{
    LOCK();
    err = "opt succeed.";

    avcodec_free_context(&codectx_);

    return true;
}