/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    COutput.cpp
*  简要描述:    输出
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "common.h"
#include "COutput.h"

COutput::~COutput()
{
    close();
}

int COutput::set_output(const std::string& output)
{
    LOCK();
    CHECKSTOP();

    if (output.empty())
    {
        return AVERROR(EINVAL);
    }
    else
    {
        output_ = output;
        return 0;
    }
}

int COutput::add_stream(AVCodecID id, int& index)
{
    LOCK();
    CHECKSTOP();

    if (output_.empty())
    {
        return AVERROR(EINVAL);
    }
    if (fmt_ == nullptr)
    {
        int ret = avformat_alloc_output_context2(&fmt_, nullptr, nullptr, output_.c_str());
        CHECKFFRET(ret);
    }

    AVCodec* codec = avcodec_find_encoder(id);
    if (codec == nullptr)
    {
        return AVERROR(EINVAL);
    }

    AVStream* stream = avformat_new_stream(fmt_, codec);
    if (stream == nullptr)
    {
        return AVERROR_BUG;
    }
    index = stream->index;
    return 0;
}

int COutput::get_timebase(int index, AVRational& timebase)
{
    LOCK();
    CHECKNOTSTOP();

    timebase = fmt_->streams[index]->time_base;
    return 0;
}

int COutput::copy_param(unsigned int index, const AVCodecParameters* par)
{
    LOCK();
    CHECKSTOP();
    
    if (fmt_ == nullptr || fmt_->nb_streams <= index)
    {
        return AVERROR(EINVAL);
    }

    return avcodec_parameters_copy(fmt_->streams[index]->codecpar, par);
}    

int COutput::copy_param(unsigned int index, const AVCodecContext* codectx)
{
    LOCK();
    CHECKSTOP();
    
    if (fmt_ == nullptr || fmt_->nb_streams <= index)
    {
        return AVERROR(EINVAL);
    }
    
    return avcodec_parameters_from_context(fmt_->streams[index]->codecpar, codectx);
}

int COutput::open()
{
    LOCK();
    CHECKSTOP();

    if (fmt_ == nullptr || output_.empty())
    {
        return AVERROR(EINVAL);
    }

    int ret = avio_open2(&fmt_->pb, output_.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
    CHECKFFRET(ret);

    av_dump_format(fmt_, 0, output_.c_str(), 1);

    ret = avformat_write_header(fmt_, nullptr);
    CHECKFFRET(ret);
    status_ = OPENED;

    return 0;
}

int COutput::write_frame(AVPacket* packet)
{
    LOCK();
    CHECKNOTSTOP();

    if (fmt_ == nullptr)
    {
        return AVERROR(EINVAL);
    }
    return av_interleaved_write_frame(fmt_, packet);
}

int COutput::close()
{
    LOCK();
    CHECKNOTSTOP();

    int ret = av_write_trailer(fmt_);
    CHECKFFRET(ret);
    ret = avio_closep(&fmt_->pb);
    CHECKFFRET(ret);
    if (fmt_ != nullptr)
    {
        av_dump_format(fmt_, 0, output_.c_str(), 1);
        avformat_free_context(fmt_);
        fmt_ = nullptr;
    }
    status_ = STOP;

    return 0;
}