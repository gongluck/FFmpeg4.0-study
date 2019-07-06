/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CDemux.cpp
*  简要描述:    解封装
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "common.h"
#include "CDemux.h"

CDemux::~CDemux()
{
    std::string err;
    stopdemux(err);
    free_opt(err);
}

bool CDemux::set_input(const std::string& input, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    if (input.empty())
    {
        err = "input is empty.";
        return false;
    }
    else
    {
        input_ = input;
        return true;
    }
}

const std::string& CDemux::get_input(std::string& err)
{
    LOCK();
    err = "opt succeed.";
    return input_;
}

bool CDemux::set_demux_callback(DemuxPacketCallback cb, void* param, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    demuxpacketcb_ = cb;
    demuxpacketcbparam_ = param;

    return true;
}

bool CDemux::set_demux_status_callback(DemuxStatusCallback cb, void* param, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    demuxstatuscb_ = cb;
    demuxstatuscbparam_ = param;

    return true;
}

bool CDemux::openinput(std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";
    int ret = 0;

    avformat_close_input(&fmtctx_);
    fmtctx_ = avformat_alloc_context();
    if (fmtctx_ == nullptr)
    {
        err = "avformat_alloc_context() return nullptr.";
        return false;
    }
    ret = avformat_open_input(&fmtctx_, input_.c_str(), fmt_, &dic_);
    CHECKFFRET(ret);

    ret = avformat_find_stream_info(fmtctx_, nullptr);
    CHECKFFRET(ret);

    av_dump_format(fmtctx_, 0, input_.c_str(), 0);

    return true;
}

bool CDemux::begindemux(std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    status_ = DEMUXING;
    std::thread th(&CDemux::demuxthread, this);
    demuxth_.swap(th);

    return true;
}

bool CDemux::stopdemux(std::string& err)
{
    LOCK();
    err = "opt succeed.";

    status_ = STOP;
    if (demuxth_.joinable())
    {
        demuxth_.join();
    }
    avformat_close_input(&fmtctx_);

    return true;
}

bool CDemux::demuxthread()
{
    int ret;
    std::string err;

    AVPacket* packet = av_packet_alloc();
    const AVBitStreamFilter* bsf = nullptr;
    AVBSFContext* bsfctx = nullptr;
    AVCodecParameters* codecpar = nullptr;
    int vindex = -1;
    do
    {
        if (fmtctx_ == nullptr)
        {
            err = "fmtctx is nullptr.";
            break;
        }
        else if (packet == nullptr)
        {
            err = "av_packet_alloc() return nullptr.";
            break;
        }
        // 初始化packet
        av_init_packet(packet);

        // bsf
        if (!bsfname_.empty())
        {
            bsf = av_bsf_get_by_name(bsfname_.c_str());
            if (bsf == nullptr)
            {
                err = "av_bsf_get_by_name() return nullptr.";
                break;
            }
            ret = av_bsf_alloc(bsf, &bsfctx);
            if (ret < 0)
            {
                err = av_err2str(ret);
                break;
            }
            for (int i = 0; i < fmtctx_->nb_streams; ++i)
            {
                if (fmtctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                {
                    codecpar = fmtctx_->streams[i]->codecpar;
                    vindex = i;
                    break;
                }
            }
            if (codecpar == nullptr)
            {
                err = "can not find codecpar.";
                break;
            }
            ret = avcodec_parameters_copy(bsfctx->par_in, codecpar);
            if (ret < 0)
            {
                err = av_err2str(ret);
                break;
            }
            ret = av_bsf_init(bsfctx);
            if (ret < 0)
            {
                err = av_err2str(ret);
                break;
            }
        }

        // 循环读数据解码数据
        while (true)
        {
            if (status_ != DEMUXING)
            {
                break;
            }

            // 读数据
            ret = av_read_frame(fmtctx_, packet);
            if (ret < 0)
            {
                err = av_err2str(ret);
                break; //这里认为视频读取完了
            }
            else if (demuxpacketcb_ != nullptr)
            {
                if (packet->stream_index == vindex && bsfctx != nullptr)
                {
                    ret = av_bsf_send_packet(bsfctx, packet);
                    if (ret < 0)
                    {
                        err = av_err2str(ret);
                        break;
                    }
                    while (ret >= 0)
                    {
                        ret = av_bsf_receive_packet(bsfctx, packet);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        {
                            // 不完整或者EOF
                            break;
                        }
                        else if (ret < 0)
                        {
                            // 其他错误
                            err = av_err2str(ret);
                            if (demuxstatuscb_ != nullptr)
                            {
                                demuxstatuscb_(DEMUXING, err, demuxstatuscbparam_);
                            }
                            break;
                        }
                        else
                        {
                            demuxpacketcb_(packet, av_rescale_q_rnd(packet->pts, fmtctx_->streams[packet->stream_index]->time_base, { 1, 1 }, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)), demuxpacketcbparam_);
                        }
                    }
                }
                else
                {
                    demuxpacketcb_(packet, av_rescale_q_rnd(packet->pts, fmtctx_->streams[packet->stream_index]->time_base, { 1, 1 }, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)), demuxpacketcbparam_);
                }
            }

            // 不再引用指向的缓冲区
            av_packet_unref(packet);
        }
        break;
    } while (true);

    // 清理bsf
    av_bsf_free(&bsfctx);
    // 清理packet
    av_packet_free(&packet);

    status_ = STOP;
    if (demuxstatuscb_ != nullptr)
    {
        demuxstatuscb_(STOP, err, demuxstatuscbparam_);
    }

    return true;
}

int CDemux::get_steam_index(AVMediaType type, std::string& err)
{
    TRYLOCK();
    err = "opt succeed.";

    int ret = av_find_best_stream(fmtctx_, type, -1, -1, nullptr, 0);
    UNLOCK();
    if (ret < 0)
    {
        err = av_err2str(ret);
        return -1;
    }
    else
    {
        return ret;
    }
} 

const AVCodecParameters* CDemux::get_steam_par(int index, std::string& err)
{
    TRYLOCK();
    const AVCodecParameters* par = nullptr;
    err = "opt succeed.";

    if (index < 0 || static_cast<unsigned int>(index) >= fmtctx_->nb_streams)
    {
        err = "stream index err.";
    }
    else
    {
        par = fmtctx_->streams[index]->codecpar;
    }
    UNLOCK();

    return par;
}

bool CDemux::seek(int64_t timestamp, int index, int flags, std::string& err)
{
    TRYLOCK();
    err = "opt succeed.";

    int ret = av_seek_frame(fmtctx_, index, av_rescale_q_rnd(timestamp, { 1, 1 }, fmtctx_->streams[index]->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)), flags);
    UNLOCK();
    if (ret < 0)
    {
        err = av_err2str(ret);
        return false;
    }
    else
    {
        return true;
    }
}

bool CDemux::device_register_all(std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    avdevice_register_all();

    return true;
}

bool CDemux::set_input_format(const std::string& fmt, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    if (fmt.empty())
    {
        err = "fmt is empty.";
        return false;
    }
    else
    {
        fmt_ = av_find_input_format(fmt.c_str());
        if (fmt_ == nullptr)
        {
            err = "can not find fmt " + fmt;
            return false;
        }
    }

    return true;
}

bool CDemux::set_dic_opt(const std::string& key, const std::string& value, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    if (key.empty() || value.empty())
    {
        err = "input is empty.";
        return false;
    }

    CHECKFFRET(av_dict_set(&dic_, key.c_str(), value.c_str(), 0));

    return true;
}

bool CDemux::free_opt(std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    av_dict_free(&dic_);
    fmt_ = nullptr;

    return true;
}

bool CDemux::set_bsf_name(const std::string& bsf, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";

    bsfname_ = bsf;

    return true;
}