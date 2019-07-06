/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CSws.h
*  简要描述:    帧转换
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#ifndef __CSWS_H__
#define __CSWS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <libswscale/swscale.h>
#include <libavutil/imgutils.h> // av_image_alloc

#ifdef __cplusplus
}
#endif

#include <string>
#include <mutex>

class CSws
{
public:
    virtual ~CSws();

    // 状态
    enum STATUS { STOP, LOCKED };
    // 设置源参数
    bool set_src_opt(AVPixelFormat pixfmt, int w, int h, std::string& err);
    // 设置目标参数
    bool set_dst_opt(AVPixelFormat pixfmt, int w, int h, std::string& err);
    // 锁定设置
    bool lock_opt(std::string& err);
    // 解除锁定
    bool unlock_opt(std::string& err);
    // 转换
    int scale(const uint8_t* const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, uint8_t* const dst[], const int dstStride[], std::string& err);

private:
    STATUS status_ = STOP;
    std::recursive_mutex mutex_;

    SwsContext* swsctx_ = nullptr;

    AVPixelFormat src_pix_fmt_ = AV_PIX_FMT_NONE;
    AVPixelFormat dst_pix_fmt_ = AV_PIX_FMT_NONE;
    int src_w_ = 0;
    int src_h_ = 0;
    int dst_w_ = 0;
    int dst_h_ = 0;
};

#endif//__CSWS_H__