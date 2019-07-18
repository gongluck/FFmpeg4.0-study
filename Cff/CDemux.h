/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    CDemux.h
*  简要描述:    解封装
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#ifndef __CDEMUX_H__
#define __CDEMUX_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>

#ifdef __cplusplus
}
#endif

#include "common.h"
#include <string>
#include <mutex>
#include <thread>
#include <map>
#include <functional>

class CDemux
{
public:
    virtual ~CDemux();
    // 状态
    enum STATUS { STOP, DEMUXING };
    // 状态通知回调声明
    typedef void (*DemuxStatusCallback)(STATUS status, int err, void* param);
    // 解封装帧回调声明
    typedef void (*DemuxPacketCallback)(const AVPacket* packet, AVRational timebase, void* param);

    // 设置输入
    int set_input(const std::string& input);
    // 获取输入
    int get_input(std::string& input);

    // 设置解封装帧回调 
    int set_demux_callback(DemuxPacketCallback cb, void* param);
    // 设置解封装状态变化回调
    int set_demux_status_callback(DemuxStatusCallback cb, void* param);

    // 打开输入
    int openinput();
    // 开始解封装
    int begindemux();
    // 停止解封装
    int stopdemux();

    // 获取流索引
    int get_steam_index(AVMediaType type, int& index);
    // 获取流参数
    int get_steam_par(int index, const AVCodecParameters*& par);

    // 跳转到指定秒
    int seek(int64_t timestamp, int index, int flags);

    // 启用设备采集
    int device_register_all();
    // 设置输入格式
    int set_input_format(const std::string& fmt);
    // 设置附加参数
    int set_dic_opt(const std::string& key, const std::string& value);
    // 清理设置
    int free_opt();

    // 设置bsf名称，影响回调的packet数据能否直接播放
    int set_bsf_name(unsigned int index, const std::string& bsf);

private:
    // 解封装线程
    int demuxthread();

private:
    STATUS status_ = STOP;
    std::recursive_mutex mutex_;

    std::string input_;
    std::thread demuxth_;

    std::function<void(STATUS, int, void*)> demuxstatuscb_ = nullptr;
    void* demuxstatuscbparam_ = nullptr;
    std::function<void(const AVPacket*, AVRational, void*)> demuxpacketcb_ = nullptr;
    void* demuxpacketcbparam_ = nullptr;

    //ffmpeg
    AVFormatContext* fmtctx_ = nullptr;
    AVInputFormat* fmt_ = nullptr;
    AVDictionary* dic_ = nullptr;
    std::map<unsigned int, std::string> bsfs_;
};

#endif//__CDEMUX_H__