/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CFilter.h
*  简要描述:    过滤器
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#ifndef __CFILTER_H__
#define __CFILTER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>

#ifdef __cplusplus
}
#endif

#include <string>
#include <mutex>
#include <functional>

class CFilter
{
public:
    virtual ~CFilter();
    // 输出回调声明
    typedef void (*FilterCallback)(const AVFrame* frame, void* param);

    // 设置解码帧回调 
    int set_filter_callback(FilterCallback cb, void* param);

    // 设置过滤器
    int init_video_filter(const std::string& args, const std::string& filters_descr, const AVPixelFormat pix_fmts[]);
    int init_audio_filter(const std::string& args, const std::string& filters_descr, const enum AVSampleFormat sample_fmts[], const int64_t layouts[], const int rates[]);

    // 输入一帧
    int add_frame(AVFrame* frame);

    // 清理
    int clear_filter();

private:
    std::recursive_mutex mutex_;

    std::function<void(const AVFrame*, void*)> filtercb_ = nullptr;
    void* filtercbparam_ = nullptr;

    //ffmpeg
    AVFilterGraph* filter_graph_ = nullptr;
    AVFilterContext* buffersrc_ctx_ = nullptr;
    AVFilterContext* buffersink_ctx_ = nullptr;
    const AVFilter* buffersrc_ = nullptr;
    const AVFilter* buffersink_ = nullptr;
    AVFilterInOut* inputs_ = nullptr;
    AVFilterInOut* outputs_ = nullptr;
    AVFrame filter_frame_ = { 0 };
};

#endif//__CFILTER_H__