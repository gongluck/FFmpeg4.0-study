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
    stopdemux();
    free_opt();
}

int CDemux::set_input(const std::string& input)
{
    LOCK();
    CHECKSTOP();

    if (input.empty())
    {
        return EINVAL;
    }
    else
    {
        input_.assign(input);
        return 0;
    }
}

int CDemux::get_input(std::string& input)
{
    LOCK();
    if (input_.empty())
    {
        return EINVAL;
    }
    else
    {
        input.assign(input_);
        return 0;
    }
}

int CDemux::set_demux_callback(DemuxPacketCallback cb, void* param)
{
    LOCK();
    CHECKSTOP();

    demuxpacketcb_ = cb;
    demuxpacketcbparam_ = param;

    return 0;
}

int CDemux::set_demux_status_callback(DemuxStatusCallback cb, void* param)
{
    LOCK();
    CHECKSTOP();

    demuxstatuscb_ = cb;
    demuxstatuscbparam_ = param;

    return 0;
}

int CDemux::openinput()
{
    LOCK();
    CHECKSTOP();
    int ret = 0;

    avformat_close_input(&fmtctx_);
    fmtctx_ = avformat_alloc_context();
    if (fmtctx_ == nullptr)
    {
        return AVERROR_BUG;
    }
    ret = avformat_open_input(&fmtctx_, input_.c_str(), fmt_, &dic_);
    CHECKFFRET(ret);

    ret = avformat_find_stream_info(fmtctx_, nullptr);
    CHECKFFRET(ret);

    av_dump_format(fmtctx_, 0, input_.c_str(), 0);

    return 0;
}

int CDemux::begindemux()
{
    LOCK();
    CHECKSTOP();

    status_ = DEMUXING;
    std::thread th(&CDemux::demuxthread, this);
    demuxth_.swap(th);

    return 0;
}

int CDemux::stopdemux()
{
    LOCK();

    status_ = STOP;
    if (demuxth_.joinable())
    {
        demuxth_.join();
    }
    avformat_close_input(&fmtctx_);

    return 0;
}

int CDemux::demuxthread()
{
    int ret = 0;

    AVPacket* packet = av_packet_alloc();
    const AVBitStreamFilter* bsf = nullptr;
    AVBSFContext* bsfctx = nullptr;
    AVCodecParameters* codecpar = nullptr;
    std::map<unsigned int, AVBSFContext*> bsfctxs;
    do
    {
        if (fmtctx_ == nullptr)
        {
            ret = EINVAL;
            break;
        }
        else if (packet == nullptr)
        {
            ret = AVERROR_BUG;
            break;
        }
        // 初始化packet
        av_init_packet(packet);

        // BitStreamFilter
        if (bsfs_.size() > 0)
        {
            for (const auto i : bsfs_)
            {
                if (i.first >= fmtctx_->nb_streams ||
                    i.second.empty() ||
                    (bsf = av_bsf_get_by_name(i.second.c_str())) == nullptr ||
                    (codecpar = fmtctx_->streams[i.first]->codecpar) == nullptr ||
                    av_bsf_alloc(bsf, &bsfctx) < 0)
                {
                    continue;
                }
                if(avcodec_parameters_copy(bsfctx->par_in, codecpar) < 0 ||
                    av_bsf_init(bsfctx) < 0)
                {
                    av_bsf_free(&bsfctx);
                    continue;
                }
                bsfctxs[i.first] = bsfctx;
            }
        }

        // 循环读数据解码数据
        while (true)
        {
            av_usleep(10);
            if (status_ != DEMUXING)
            {
                ret = AVERROR_EOF;
                break;
            }

            // 读数据
            ret = av_read_frame(fmtctx_, packet);
            if (ret < 0)
            {
                break; //这里认为视频读取完了
            }
            else if (demuxpacketcb_ != nullptr)
            {
                if (bsfctxs[packet->stream_index] != nullptr)
                {
                    bsfctx = bsfctxs[packet->stream_index];
                    ret = av_bsf_send_packet(bsfctx, packet);
                    if (ret < 0)
                    {
                        break;
                    }
                    while (ret >= 0)
                    {
                        ret = av_bsf_receive_packet(bsfctx, packet);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        {
                            // 不完整或者EOF
                            av_usleep(10);
                            break;
                        }
                        else if (ret < 0)
                        {
                            // 其他错误
                            if (demuxstatuscb_ != nullptr)
                            {
                                demuxstatuscb_(DEMUXING, ret, demuxstatuscbparam_);
                            }
                            break;
                        }
                        else
                        {
                            demuxpacketcb_(packet, fmtctx_->streams[packet->stream_index]->time_base, demuxpacketcbparam_);
                        }
                    }
                }
                else
                {
                    demuxpacketcb_(packet, fmtctx_->streams[packet->stream_index]->time_base, demuxpacketcbparam_);
                }
            }

            // 不再引用指向的缓冲区
            av_packet_unref(packet);
        }
        break;
    } while (true);

    // 清理bsf
    for (auto& i : bsfctxs)
    {
        av_bsf_free(&i.second);
    }
    // 清理packet
    av_packet_free(&packet);

    status_ = STOP;
    if (demuxstatuscb_ != nullptr)
    {
        demuxstatuscb_(STOP, ret, demuxstatuscbparam_);
    }

    return true;
}

int CDemux::get_steam_index(AVMediaType type, int& index)
{
    TRYLOCK();
    if (fmtctx_ == nullptr)
    {
        UNLOCK();
        return EINVAL;
    }

    int ret = av_find_best_stream(fmtctx_, type, -1, -1, nullptr, 0);
    UNLOCK();
    if (ret >= 0)
    {
        index = ret;
        return 0;
    }
    else
    {
        return ret;
    }
} 

int CDemux::get_stream_par(int index, const AVCodecParameters*& par)
{
    TRYLOCK();

    if (index < 0 || static_cast<unsigned int>(index) >= fmtctx_->nb_streams)
    {
        UNLOCK();
        return EINVAL;
    }
    else
    {
        par = fmtctx_->streams[index]->codecpar;
    }
    UNLOCK();

    return 0;
}

int CDemux::seek(int64_t timestamp, int index, int flags)
{
    TRYLOCK();

    int ret = av_seek_frame(fmtctx_, index, av_rescale_q_rnd(timestamp, { 1, 1 }, fmtctx_->streams[index]->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)), flags);
    UNLOCK();
    
    return ret;
}

int CDemux::device_register_all()
{
    LOCK();
    CHECKSTOP();

    avdevice_register_all();

    return 0;
}

int CDemux::set_input_format(const std::string& fmt)
{
    LOCK();
    CHECKSTOP();

    if (fmt.empty())
    {
        return EINVAL;
    }
    else
    {
        fmt_ = av_find_input_format(fmt.c_str());
        if (fmt_ == nullptr)
        {
            return AVERROR_BUG;
        }
    }

    return 0;
}

int CDemux::set_dic_opt(const std::string& key, const std::string& value)
{
    LOCK();
    CHECKSTOP();

    if (key.empty() || value.empty())
    {
        return EINVAL;
    }

    return av_dict_set(&dic_, key.c_str(), value.c_str(), 0);
}

int CDemux::free_opt()
{
    LOCK();
    CHECKSTOP();

    av_dict_free(&dic_);
    fmt_ = nullptr;
    bsfs_.clear();

    return 0;
}

int CDemux::set_bsf_name(unsigned int index, const std::string& bsf)
{
    LOCK();
    CHECKSTOP();

    if (bsf.empty())
    {
        return EINVAL;
    }

    bsfs_[index] = bsf;

    return 0;
}