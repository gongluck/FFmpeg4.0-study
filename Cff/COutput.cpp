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
    std::string err;
    close(err);
}

bool COutput::set_output(const std::string& output, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    if (output.empty())
    {
        err = "output is empty.";
        return false;
    }
    else
    {
        output_ = output;
        return true;
    }
}

int COutput::add_stream(AVCodecID id, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    if (fmt_ == nullptr)
    {
        CHECKFFRET(avformat_alloc_output_context2(&fmt_, nullptr, nullptr, output_.c_str()));
    }
    AVCodec* codec = avcodec_find_encoder(id);
    if (codec == nullptr)
    {
        err = "can not find encoder of " + id;
        return false;
    }

    AVStream* stream = avformat_new_stream(fmt_, codec);
    if (stream == nullptr)
    {
        err = "avformat_new_stream return nullptr.";
        return false;
    }
    return stream->index;
}

bool COutput::copy_param(int index, const AVCodecParameters* par, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";
    int ret = 0;

    ret = avcodec_parameters_copy(fmt_->streams[index]->codecpar, par);
    CHECKFFRET(ret);

    return true;
}

bool COutput::open(std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";
    int ret = 0;

    if (fmt_ == nullptr)
    {
        err = "fmt_ is nullptr.";
        return false;
    }

    ret = avio_open2(&fmt_->pb, output_.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
    CHECKFFRET(ret);

    ret = avformat_write_header(fmt_, nullptr);
    CHECKFFRET(ret);
    status_ = OPENED;

    return true;
}

bool COutput::write_frame(AVPacket* packet, std::string& err)
{
    LOCK();
    CHECKNOTSTOP(err);
    err = "opt succeed.";
    int ret = 0;

    ret = av_interleaved_write_frame(fmt_, packet);
    CHECKFFRET(ret);

    return true;
}

bool COutput::close(std::string& err)
{
    LOCK();
    CHECKNOTSTOP(err);
    err = "opt succeed.";
    int ret = 0;

    ret = av_write_trailer(fmt_);
    CHECKFFRET(ret);
    ret = avio_closep(&fmt_->pb);
    CHECKFFRET(ret);
    avformat_free_context(fmt_);
    fmt_ = nullptr;

    status_ = STOP;

    return true;
}