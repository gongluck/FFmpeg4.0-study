/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CSws.cpp
*  简要描述:    帧转换
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "common.h"
#include "CSws.h"

CSws::~CSws()
{
    unlock_opt();
}

int CSws::set_src_opt(AVPixelFormat pixfmt, int w, int h)
{
    LOCK();
    CHECKSTOP();

    src_pix_fmt_ = pixfmt;
    src_w_ = w;
    src_h_ = h;

    return 0;
}

int CSws::set_dst_opt(AVPixelFormat pixfmt, int w, int h)
{
    LOCK();
    CHECKSTOP();

    dst_pix_fmt_ = pixfmt;
    dst_w_ = w;
    dst_h_ = h;

    return 0;
}

int CSws::lock_opt()
{
    LOCK();
    CHECKSTOP();
    int ret = 0;

    swsctx_ = sws_getContext(src_w_, src_h_, src_pix_fmt_, dst_w_, dst_h_, dst_pix_fmt_, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsctx_ == nullptr)
    {
        ret = AVERROR_BUG2;
        av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
    }
    else
    {
        status_ = LOCKED;
    }

    return ret;
}

int CSws::unlock_opt()
{
    LOCK();
    
    sws_freeContext(swsctx_);
    swsctx_ = nullptr;
    status_ = STOP;
    src_w_ = 0;
    src_h_ = 0;
    dst_w_ = 0;
    dst_h_ = 0;

    return 0;
}

int CSws::scale(const uint8_t* const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, uint8_t* const dst[], const int dstStride[])
{
    LOCK();
    CHECKNOTSTOP();

    return sws_scale(swsctx_, srcSlice, srcStride, srcSliceY, srcSliceH, dst, dstStride);
}