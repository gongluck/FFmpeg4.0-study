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

#include <string>
#include <mutex>
#include <thread>

class CDemux
{
public:
    virtual ~CDemux();
    // 状态
    enum STATUS { STOP, DEMUXING };
    // 状态通知回调声明
    typedef void (*DemuxStatusCallback)(STATUS status, const std::string& err, void* param);
    // 解码帧回调声明
    typedef void (*DemuxPacketCallback)(const AVPacket* packet, int64_t timestamp, void* param);

    // 设置输入
    bool set_input(const std::string& input, std::string& err);
    // 获取输入
    const std::string& get_input(std::string& err);

    // 设置解码帧回调 
    bool set_demux_callback(DemuxPacketCallback cb, void* param, std::string& err);
    // 设置解码状态变化回调
    bool set_demux_status_callback(DemuxStatusCallback cb, void* param, std::string& err);

    // 打开输入
    bool openinput(std::string& err);
    // 开始解封装
    bool begindemux(std::string& err);
    // 停止解封装
    bool stopdemux(std::string& err);

    // 获取流索引
    int get_steam_index(AVMediaType type, std::string& err);
    // 获取流参数
    const AVCodecParameters* get_steam_par(int index, std::string& err);

    // 跳转到指定秒
    bool seek(int64_t timestamp, int index, int flags, std::string& err);

    // 启用设备采集
    bool device_register_all(std::string& err);
    // 设置输入格式
    bool set_input_format(const std::string& fmt, std::string& err);
    // 设置附加参数
    bool set_dic_opt(const std::string& key, const std::string& value, std::string& err);
    // 清理设置
    bool free_opt(std::string& err);

    // 设置bsf名称，影响回调的packet数据能否直接播放
    bool set_bsf_name(const std::string& bsf, std::string& err);

private:
    // 解封装线程
    bool demuxthread();

private:
    STATUS status_ = STOP;
    std::recursive_mutex mutex_;

    std::string input_;
    std::thread demuxth_;

    DemuxStatusCallback demuxstatuscb_ = nullptr;
    void* demuxstatuscbparam_ = nullptr;
    DemuxPacketCallback demuxpacketcb_ = nullptr;
    void* demuxpacketcbparam_ = nullptr;

    //ffmpeg
    AVFormatContext* fmtctx_ = nullptr;
    AVInputFormat* fmt_ = nullptr;
    AVDictionary* dic_ = nullptr;
    std::string bsfname_;
};

#endif//__CDEMUX_H__