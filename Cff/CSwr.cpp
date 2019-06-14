#include "common.h"
#include "CSwr.h"

CSwr::~CSwr()
{
    std::string err;
    unlock_opt(err);
}

bool CSwr::set_src_opt(int64_t layout, int rate, enum AVSampleFormat fmt, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    src_layout_ = layout;
    src_rate_ = rate;
    src_sam_fmt_ = fmt;
    return true;
}

bool CSwr::set_dst_opt(int64_t layout, int rate, enum AVSampleFormat fmt, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    dst_layout_ = layout;
    dst_rate_ = rate;
    dst_sam_fmt_ = fmt;
    return true;
}

bool CSwr::lock_opt(std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    swrctx_ = swr_alloc_set_opts(swrctx_, dst_layout_, dst_sam_fmt_, dst_rate_, src_layout_, src_sam_fmt_, src_rate_, 0, nullptr);
    if (swrctx_ == nullptr)
    {
        err = "swr_alloc_set_opts(swrctx_, dst_layout_, dst_sam_fmt_, dst_rate_, src_layout_, src_sam_fmt_, src_rate_, 0, nullptr) return nullptr.";
        return false;
    }
    int ret = swr_init(swrctx_);
    CHECKFFRET(ret);
    status_ = LOCKED;
    return true;
}

bool CSwr::unlock_opt(std::string& err)
{
    LOCK();
    swr_free(&swrctx_);
    status_ = STOP;
    src_sam_fmt_ = AV_SAMPLE_FMT_NONE;
    dst_sam_fmt_ = AV_SAMPLE_FMT_NONE;
    src_layout_ = AV_CH_LAYOUT_MONO;
    dst_layout_ = AV_CH_LAYOUT_MONO;
    src_rate_ = 0;
    dst_rate_ = 0;
    return true;
}

int CSwr::convert(uint8_t** out, int out_count, const uint8_t** in, int in_count, std::string& err)
{
    LOCK();
    CHECKNOTSTOP(err);
    int ret = swr_convert(swrctx_, out, out_count, in, in_count);
    CHECKFFRET(ret);
    return ret;
}