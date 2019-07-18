/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    COutput.h
*  简要描述:    输出
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#ifndef __COUTPUT_H__
#define __COUTPUT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif

#include <string>
#include <mutex>

class COutput
{
public:
    virtual ~COutput();
    // 状态
    enum STATUS { STOP, OPENED };

    // 设置输入
    int set_output(const std::string& output);

    // 添加流
    int add_stream(AVCodecID id, int& index);
    // 获取时基
    int get_timebase(int index, AVRational& timebase);

    // 设置编码器
    int copy_param(int index, const AVCodecParameters* par);
    int copy_param(int index, const AVCodecContext* codectx);

    // 打开输出
    int open();

    // 写数据
    int write_frame(AVPacket* packet);

    // 关闭输出
    int close();

private:
    STATUS status_ = STOP;
    std::recursive_mutex mutex_;

    std::string output_;

    // ffmpeg
    AVFormatContext* fmt_ = nullptr;
};

#endif//__COUTPUT_H__