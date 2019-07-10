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
    bool set_output(const std::string& output, std::string& err);

    // 添加流
    int add_stream(AVCodecID id, std::string& err);

    // 设置编码器
    bool copy_param(int index, const AVCodecParameters* par, std::string& err);

    // 打开输出
    bool open(std::string& err);

    // 写数据
    bool write_frame(AVPacket* packet, std::string& err);

    // 关闭输出
    bool close(std::string& err);

private:
    STATUS status_ = STOP;
    std::recursive_mutex mutex_;

    std::string output_;

    // ffmpeg
    AVFormatContext* fmt_ = nullptr;
};

#endif//__COUTPUT_H__