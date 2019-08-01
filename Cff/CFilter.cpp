/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CFilter.cpp
*  简要描述:    过滤器
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "common.h"
#include "CFilter.h"

CFilter::~CFilter()
{
}

int CFilter::set_filter_callback(FilterCallback cb, void* param)
{
    LOCK();

    filtercb_ = cb;
    filtercbparam_ = param;

    return 0;
}

int CFilter::init_filter(const std::string& args, const std::string& filters_descr, AVPixelFormat pix_fmts[])
{
    LOCK();

    int ret = clear_filter();
    CHECKFFRET(ret);

    buffersrc_ = avfilter_get_by_name("buffer");
    buffersink_ = avfilter_get_by_name("buffersink");
    if (buffersrc_ == nullptr || buffersink_ == nullptr)
    {
        return AVERROR(EINVAL);
    }

    inputs_ = avfilter_inout_alloc();
    outputs_ = avfilter_inout_alloc();
    filter_graph_ = avfilter_graph_alloc();
    if (inputs_ == nullptr || outputs_ == nullptr || filter_graph_ == nullptr)
    {
        clear_filter();
        return AVERROR(ENOMEM);
    }

    ret = avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc_, "in", args.c_str(), nullptr, filter_graph_);
    CHECKFFRET(ret);

    ret = avfilter_graph_create_filter(&buffersink_ctx_, buffersink_, "out", nullptr, nullptr, filter_graph_);
    CHECKFFRET(ret);

    ret = av_opt_set_int_list(buffersink_ctx_, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    CHECKFFRET(ret);

    outputs_->name = av_strdup("in");
    outputs_->filter_ctx = buffersrc_ctx_;
    outputs_->pad_idx = 0;
    outputs_->next = nullptr;

    inputs_->name = av_strdup("out");
    inputs_->filter_ctx = buffersink_ctx_;
    inputs_->pad_idx = 0;
    inputs_->next = nullptr;

    ret = avfilter_graph_parse_ptr(filter_graph_, filters_descr.c_str(), &inputs_, &outputs_, nullptr);
    CHECKFFRET(ret);

    ret = avfilter_graph_config(filter_graph_, nullptr);
    CHECKFFRET(ret);

    return ret;
}

int CFilter::add_frame(AVFrame* frame)
{
    LOCK();

    int ret = av_buffersrc_add_frame_flags(buffersrc_ctx_, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    CHECKFFRET(ret);

    while (ret >= 0) 
    {
        ret = av_buffersink_get_frame(buffersink_ctx_, &filter_frame_);
        CHECKFFRET(ret);
        if (filtercb_ != nullptr)
        {
            filtercb_(&filter_frame_, filtercbparam_);
        }
        av_frame_unref(&filter_frame_);
    }

    return ret;
}

int CFilter::clear_filter()
{
    LOCK();

    avfilter_inout_free(&inputs_);
    avfilter_inout_free(&outputs_);
    buffersrc_ = nullptr;
    buffersink_ = nullptr;

    return 0;
}