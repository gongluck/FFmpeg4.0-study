#include "common.h"
#include "CSws.h"

CSws::~CSws()
{
    std::string err;
    unlock_opt(err);
}

bool CSws::set_src_opt(AVPixelFormat pixfmt, int w, int h, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    src_pix_fmt_ = pixfmt;
    src_w_ = w;
    src_h_ = h;
    return true;
}

bool CSws::set_dst_opt(AVPixelFormat pixfmt, int w, int h, std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    dst_pix_fmt_ = pixfmt;
    dst_w_ = w;
    dst_h_ = h;
    return true;
}

bool CSws::lock_opt(std::string& err)
{
    LOCK();
    CHECKSTOP(err);
    swsctx_ = sws_getContext(src_w_, src_h_, src_pix_fmt_, dst_w_, dst_h_, dst_pix_fmt_, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsctx_ == nullptr)
    {
        err = "sws_getContext(src_w_, src_h_, src_pix_fmt_, dst_w_, dst_h_, dst_pix_fmt_, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr) return nullptr.";
        return false;
    }
    status_ = LOCKED;
    return true;
}

bool CSws::unlock_opt(std::string& err)
{
    LOCK();
    sws_freeContext(swsctx_);
    swsctx_ = nullptr;
    status_ = STOP;
    src_w_ = 0;
    src_h_ = 0;
    dst_w_ = 0;
    dst_h_ = 0;
    return true;
}

int CSws::scale(const uint8_t* const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, uint8_t* const dst[], const int dstStride[], std::string& err)
{
    LOCK();
    CHECKNOTSTOP(err);
    int ret = sws_scale(swsctx_, srcSlice, srcStride, srcSliceY, srcSliceH, dst, dstStride);
    CHECKFFRET(ret);
    return ret;
}