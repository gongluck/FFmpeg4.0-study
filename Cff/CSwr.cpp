/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CSwr.cpp
*  简要描述:    重采样
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "common.h"
#include "CSwr.h"

CSwr::~CSwr()
{
    unlock_opt();
}

int CSwr::set_src_opt(int64_t layout, int rate, enum AVSampleFormat fmt)
{
    LOCK();
    CHECKSTOP();

    src_layout_ = layout;
    src_rate_ = rate;
    src_sam_fmt_ = fmt;

    return 0;
}

int CSwr::set_dst_opt(int64_t layout, int rate, enum AVSampleFormat fmt)
{
    LOCK();
    CHECKSTOP();

    dst_layout_ = layout;
    dst_rate_ = rate;
    dst_sam_fmt_ = fmt;

    return 0;
}

int CSwr::lock_opt()
{
    LOCK();
    CHECKSTOP();

    swrctx_ = swr_alloc_set_opts(swrctx_, dst_layout_, dst_sam_fmt_, dst_rate_, src_layout_, src_sam_fmt_, src_rate_, 0, nullptr);
    if (swrctx_ == nullptr)
    {
        return AVERROR_BUG2;
    }
    CHECKFFRET(swr_init(swrctx_));

    status_ = LOCKED;
    return 0;
}

int CSwr::unlock_opt()
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

    return 0;
}

int CSwr::convert(uint8_t** out, int out_count, const uint8_t** in, int in_count)
{
    LOCK();
    CHECKNOTSTOP();

    return swr_convert(swrctx_, out, out_count, in, in_count);
}