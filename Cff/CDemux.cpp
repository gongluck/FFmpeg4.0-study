#include "common.h"
#include "CDemux.h"

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
    int ret;

    if (!stopdemux(err))
    {
        return false;
    }

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

    if (fmtctx_ == nullptr && demuxstatuscb_ != nullptr)
    {
        status_ = STOP;
        demuxstatuscb_(STOP, "fmtctx is nullptr", demuxstatuscbparam_);
        return false;
    }

    // 分配AVPacket
    AVPacket* packet = av_packet_alloc();
    if (packet == nullptr)
    {
        if (demuxstatuscb_ != nullptr)
        {
            status_ = STOP;
            demuxstatuscb_(STOP, "av_packet_alloc() or av_frame_alloc() return nullptr.", demuxstatuscbparam_);
        }
        av_packet_free(&packet);
        return false;
    }
    // 初始化packet
    av_init_packet(packet);

    // 循环读数据解码数据
    while (true)
    {
        if (status_ != DEMUXING)
            break;

        // 读数据
        ret = av_read_frame(fmtctx_, packet);
        if (ret < 0)
        {
            if (demuxstatuscb_ != nullptr)
            {
                status_ = STOP;
                demuxstatuscb_(STOP, av_err2str(ret), demuxstatuscbparam_);
            }
            break; //这里认为视频读取完了
        }
        else if (demuxpacketcb_ != nullptr)
        {
            demuxpacketcb_(packet, av_rescale_q_rnd(packet->pts, fmtctx_->streams[packet->stream_index]->time_base, { 1, 1 }, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)), demuxpacketcbparam_);
        }

        // 不再引用指向的缓冲区
        av_packet_unref(packet);
    }

    // 清理packet和frame
    av_packet_free(&packet);

    return true;
}

int CDemux::get_steam_index(AVMediaType type, std::string& err)
{
    TRYLOCK();
    err = "opt succeed.";

    int ret = av_find_best_stream(fmtctx_, type, -1, -1, nullptr, 0);
    this->mutex_.unlock();
    if (ret < 0)
    {
        err = av_err2str(ret);
    }
    return ret;
} 

const AVCodecParameters* CDemux::get_steam_par(int index, std::string& err)
{
    const AVCodecParameters* par = nullptr;
    err = "opt succeed.";

    if (!this->mutex_.try_lock())
    {
        err = "decoder is busing.";
    }
    else if (index >= fmtctx_->nb_streams || index < 0)
    {
        err = "stream index err";
        this->mutex_.unlock();
    }
    else
    {
        par = fmtctx_->streams[index]->codecpar;
        this->mutex_.unlock();
    }

    return par;
}

bool CDemux::seek(int64_t timestamp, int index, int flags, std::string& err)
{
    TRYLOCK();
    err = "opt succeed.";

    int ret = av_seek_frame(fmtctx_, index, av_rescale_q_rnd(timestamp, { 1, 1 }, fmtctx_->streams[index]->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)), flags);
    this->mutex_.unlock();
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

bool CDemux::set_input_format(std::string fmt, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    err = "opt succeed.";
    if (fmt.empty())
    {
        err = "input is empty.";
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
        else
        {
            return true;
        }
    }
}

bool CDemux::set_dic_opt(std::string key, std::string value, std::string& err)
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