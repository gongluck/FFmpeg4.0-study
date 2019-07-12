/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    test.cpp
*  简要描述:    测试
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#include "CDemux.h"
#include "CDecode.h"
#include "CSws.h"
#include "CSwr.h"
#include "COutput.h"
#include "CEncode.h"
#include <iostream>
#include <fstream>
extern "C"
{
#include <libavutil/audio_fifo.h>
}


//#define SEEK

int g_vindex = -1;
int g_aindex = -1;
int g_vindex_output = -1;
int g_aindex_output = -1;
int g_framesize = 1024;

#define TESTCHECKRET(ret)\
if(!ret)\
{\
    std::cerr << err << std::endl;\
}

void DemuxStatusCB(CDemux::STATUS status, const std::string& err, void* param)
{
    std::cout << std::this_thread::get_id() << " got a status " << status << " " << err << std::endl;
}

void DemuxPacketCB(const AVPacket* packet, int64_t timestamp, void* param)
{
    std::cout << std::this_thread::get_id() <<
        " got a packet , index : " << packet->stream_index <<
        " timestamp : " << timestamp << std::endl;

#ifdef SEEK
    std::string err;
    CDemux* demux = static_cast<CDemux*>(param);
    if (demux != nullptr && timestamp > 10 && !demux->seek(5, packet->stream_index, AVSEEK_FLAG_ANY, err))
    {
        std::cerr << err << std::endl;
    }
#endif
    if (packet->stream_index == g_vindex)
    {
        static std::ofstream out("out.h264", std::ios::binary | std::ios::trunc);
        if (out.is_open())
        {
            out.write(reinterpret_cast<char*>(const_cast<uint8_t*>(packet->data)), packet->size);
        }
    }
    else if (packet->stream_index == g_aindex)
    {
        // 没有打adts头，aac不能正常播放
        static std::ofstream out("out.aac", std::ios::binary | std::ios::trunc);
        if (out.is_open())
        {
            out.write(reinterpret_cast<char*>(const_cast<uint8_t*>(packet->data)), packet->size);
        }
    }
}

void DemuxPacketCB_save(const AVPacket* packet, int64_t timestamp, void* param)
{
    std::string err;
    auto output = static_cast<COutput*>(param);
    if (output == nullptr)
    {
        return;
    }
    if (packet->stream_index == g_vindex ||
        packet->stream_index == g_aindex)
    {
        auto pkt = const_cast<AVPacket*>(packet);
        pkt->stream_index = g_vindex_output == -1 ? g_aindex_output : g_vindex_output;
        if (!output->write_frame(pkt, err))
        {
            std::cerr << err << std::endl;
        }
    }
}

void DemuxDesktopCB(const AVPacket* packet, int64_t timestamp, void* param)
{
    std::cout << std::this_thread::get_id() <<
        " got a packet , index : " << packet->stream_index <<
        " timestamp : " << timestamp << std::endl;

    CDecode* decode = static_cast<CDecode*>(param);
    std::string err;
    if (decode != nullptr)
    {
        if (!decode->decode(packet, err))
        {
            std::cerr << err << std::endl;
        }
    }
}

void DemuxSystemSoundCB(const AVPacket* packet, int64_t timestamp, void* param)
{
    std::cout << std::this_thread::get_id() <<
        " got a packet , index : " << packet->stream_index <<
        " timestamp : " << timestamp << std::endl;

    CDecode* decode = static_cast<CDecode*>(param);
    std::string err;
    if (decode != nullptr)
    {
        if (!decode->decode(packet, err))
        {
            std::cerr << err << std::endl;
        }
    }
}

void DecVideoFrameCB(const AVFrame* frame, void* param)
{
    std::string err;
    CEncode* enc = static_cast<CEncode*>(param);
    if (enc != nullptr)
    {
        if (frame->format != AV_PIX_FMT_YUV420P)
        {
            CSws sws;
            AVFrame f = { 0 };
            f.width = 1920;
            f.height = 1080;
            f.format = AV_PIX_FMT_YUV420P;
            av_frame_get_buffer(&f, 1);
            av_frame_make_writable(&f);
            TESTCHECKRET(sws.set_src_opt(static_cast<AVPixelFormat>(frame->format), frame->width, frame->height, err));
            TESTCHECKRET(sws.set_dst_opt(AV_PIX_FMT_YUV420P, 1920, 1080, err));
            TESTCHECKRET(sws.lock_opt(err));
            int lines = sws.scale(frame->data, frame->linesize, 0, frame->height, f.data, f.linesize, err);
            std::cout << "sws " << lines << " lines." << std::endl;
            TESTCHECKRET(enc->encode(&f, err));
            return;
        }
        else
        {
            //TESTCHECKRET(enc->encode(frame, err));
        }
    }

    if (frame->format == AV_PIX_FMT_DXVA2_VLD)
    {
        std::cout << "got a dxva2vld " << frame->width << "x" << frame->height << std::endl;
    }
    else if (frame->format == AV_PIX_FMT_NV12)
    {
        std::cout << "got a nv12 " << frame->width << "x" << frame->height << std::endl;
        static std::ofstream video("out.nv12", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
        video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
    }
    else if (frame->format == AV_PIX_FMT_YUV420P)
    {
        std::cout << "got a yuv420p " << frame->width << "x" << frame->height << std::endl;
        static std::ofstream video("out.yuv", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
        video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
        video.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
    }
    else if (frame->format == AV_PIX_FMT_BGRA)
    {
        std::cout << "got a bgra" << std::endl;
        static std::ofstream video("out.bgra", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
    }
}

void DecAudioFrameCB(const AVFrame * frame, void* param)
{
    std::string err;
    CEncode* enc = static_cast<CEncode*>(param);
    if (enc != nullptr)
    {
        if (frame->format != AV_SAMPLE_FMT_FLTP)
        {
            static AVFrame fltp_frame = { 0 };
            static CSwr swr;//static是修复杂音的关键!数据有缓存!
            static bool binit = false;
            if (!binit)
            {
                fltp_frame.nb_samples = g_framesize;
                fltp_frame.format = AV_SAMPLE_FMT_FLTP;
                fltp_frame.channel_layout = AV_CH_LAYOUT_STEREO;
                fltp_frame.sample_rate = 44100;

                TESTCHECKRET(swr.set_src_opt(AV_CH_LAYOUT_STEREO, frame->sample_rate, static_cast<AVSampleFormat>(frame->format), err));
                TESTCHECKRET(swr.set_dst_opt(AV_CH_LAYOUT_STEREO, fltp_frame.sample_rate, static_cast<AVSampleFormat>(fltp_frame.format), err));
                TESTCHECKRET(swr.lock_opt(err));

                binit = true;
            }
            
            av_frame_get_buffer(&fltp_frame, 1);
            av_frame_make_writable(&fltp_frame);
            
            int samples = swr.convert(reinterpret_cast<uint8_t**>(&fltp_frame.data), fltp_frame.nb_samples, (const uint8_t**)(frame->data), frame->nb_samples, err);
            std::cout << "convert samples : " << samples << std::endl;
            fltp_frame.nb_samples = samples;

            // AAC输入大小有要求g_framesize
            static AVAudioFifo* fifo = av_audio_fifo_alloc(static_cast<AVSampleFormat>(fltp_frame.format), av_get_channel_layout_nb_channels(fltp_frame.channel_layout), fltp_frame.sample_rate * 2);
            av_audio_fifo_write(fifo, reinterpret_cast<void**>(fltp_frame.data), fltp_frame.nb_samples);
            while (av_audio_fifo_size(fifo) >= g_framesize)
            {
                static AVFrame ff = { 0 };
                ff.nb_samples = g_framesize;
                ff.format = AV_SAMPLE_FMT_FLTP;
                ff.channel_layout = AV_CH_LAYOUT_STEREO;
                av_frame_get_buffer(&ff, 1);
                av_frame_make_writable(&ff);
                av_audio_fifo_read(fifo, reinterpret_cast<void**>(ff.data), g_framesize);
                TESTCHECKRET(enc->encode(&ff, err));
            }
            return;
        }
    }
    else
    {
        //TESTCHECKRET(enc->encode(frame, err));
    }

    if (frame->format == AV_SAMPLE_FMT_FLTP)
    {
        std::cout << "got a fltp" << std::endl;
        static std::ofstream audio("out.pcm", std::ios::binary | std::ios::trunc);
        auto size = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));
        for (int i = 0; i < frame->nb_samples; ++i)
        {
            for (int j = 0; j < frame->channels; ++j)
            {
                audio.write(reinterpret_cast<const char*>(frame->data[j] + size * i), size);
            }
        }
    }
    else if (frame->format == AV_SAMPLE_FMT_S16)
    {
        std::cout << "got a s16" << std::endl;
        static std::ofstream audio("out.pcm", std::ios::binary | std::ios::trunc);
        audio.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0]);
    }
}

void EncVideoFrameCB(const AVPacket * packet, void* param)
{
    std::string err;
    COutput* out = static_cast<COutput*>(param);
    if (out != nullptr)
    {
        auto timebase = out->get_timebase(g_vindex_output, err);
        static int i = 0;
        const_cast<AVPacket*>(packet)->stream_index = g_vindex_output;
        const_cast<AVPacket*>(packet)->pts = av_rescale_q(i, { 1, 10 }, timebase);
        const_cast<AVPacket*>(packet)->dts = packet->pts;
        const_cast<AVPacket*>(packet)->duration = av_rescale_q(1, { 1, 10 }, timebase);
        i++;
        out->write_frame(const_cast<AVPacket*>(packet), err);
    }
    else
    {
        static std::ofstream h264("encode.h264", std::ios::binary);
        h264.write(reinterpret_cast<char*>(packet->data), packet->size);
    }
}

void EncAudioFrameCB(const AVPacket * packet, void* param)
{
    std::string err;
    COutput* out = static_cast<COutput*>(param);
    if (out != nullptr)
    {
        auto timebase = out->get_timebase(g_aindex_output, err);
        static uint64_t nextpts = 0;
        const_cast<AVPacket*>(packet)->stream_index = g_aindex_output;
        const_cast<AVPacket*>(packet)->pts = nextpts;
        const_cast<AVPacket*>(packet)->dts = nextpts;
        nextpts += packet->duration;
        out->write_frame(const_cast<AVPacket*>(packet), err);
    }
    else
    {
        static std::ofstream aac("encode.mp3", std::ios::binary);
        aac.write(reinterpret_cast<char*>(packet->data), packet->size);
    }
}

// 解封装
void test_demux()
{
    bool ret = false;
    std::string err;
    CDemux demux;

    ret = demux.set_input("in.flv", err);
    //ret = demux.set_input("in.h264", err);
    //ret = demux.set_input("in.aac", err);
    TESTCHECKRET(ret);

    ret = demux.set_demux_callback(DemuxPacketCB, &demux, err);
    TESTCHECKRET(ret);

    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux, err);
    TESTCHECKRET(ret);

    ret = demux.set_bsf_name("h264_mp4toannexb", err);
    TESTCHECKRET(ret);

    ret = demux.openinput(err);
    TESTCHECKRET(ret);

    g_vindex = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, err);
    std::cout << err << std::endl;
    g_aindex = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, err);
    std::cout << err << std::endl;

    ret = demux.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = demux.stopdemux(err);
    TESTCHECKRET(ret);
}

// 解码h264
void test_decode_h264()
{
    bool ret = false;
    std::string err;
    std::ifstream h264("in.h264", std::ios::binary);
    char buf[1024] = { 0 };
    CDecode decode;

    ret = decode.set_dec_callback(DecVideoFrameCB, &decode, err);
    TESTCHECKRET(ret);
    //ret = decode.set_hwdec_type(AV_HWDEVICE_TYPE_DXVA2, true, err);
    //TESTCHECKRET(ret);
    ret = decode.set_codeid(AV_CODEC_ID_H264, err);
    TESTCHECKRET(ret);
    ret = decode.codec_open(err);
    TESTCHECKRET(ret);

    while (!h264.eof())
    {
        h264.read(buf, sizeof(buf));
        ret = decode.decode(buf, sizeof(buf), err);
        TESTCHECKRET(ret);
    }
}

// 解码aac
void test_decode_aac()
{
    bool ret = false;
    std::string err;
    std::ifstream aac("in.aac", std::ios::binary);
    char buf[1024] = { 0 };
    CDecode decode;

    ret = decode.set_dec_callback(DecAudioFrameCB, &decode, err);
    TESTCHECKRET(ret);
    ret = decode.set_codeid(AV_CODEC_ID_AAC, err);
    TESTCHECKRET(ret);
    ret = decode.codec_open(err);
    TESTCHECKRET(ret);

    while (!aac.eof())
    {
        aac.read(buf, sizeof(buf));
        ret = decode.decode(buf, sizeof(buf), err);
        TESTCHECKRET(ret);
    }
}

// 解码mp3
void test_decode_mp3()
{
    bool ret = false;
    std::string err;
    std::ifstream mp3("in.mp3", std::ios::binary);
    char buf[1024] = { 0 };
    CDecode decode;

    ret = decode.set_dec_callback(DecAudioFrameCB, &decode, err);
    TESTCHECKRET(ret);
    ret = decode.set_codeid(AV_CODEC_ID_MP3, err);
    TESTCHECKRET(ret);
    ret = decode.codec_open(err);
    TESTCHECKRET(ret);

    while (!mp3.eof())
    {
        mp3.read(buf, sizeof(buf));
        ret = decode.decode(buf, sizeof(buf), err);
        TESTCHECKRET(ret);
    }
}

// 视频帧转换
void test_sws()
{
    bool ret = false;
    std::string err;
    std::ifstream yuv("in.yuv", std::ios::binary);
    CSws sws;

    // 分配图像数据内存
    uint8_t* src[4] = { 0 };
    int srclinesize[4] = { 0 };
    uint8_t* dst[4] = { 0 };
    int dstlinesize[4] = { 0 };

    int srcsize = av_image_alloc(src, srclinesize, 640, 432, AV_PIX_FMT_YUV420P, 1);
    int dstsize = av_image_alloc(dst, dstlinesize, 320, 240, AV_PIX_FMT_BGR24, 1);
    yuv.read(reinterpret_cast<char*>(src[0]), 640 * 432);
    yuv.read(reinterpret_cast<char*>(src[1]), 640 * 432 / 4);
    yuv.read(reinterpret_cast<char*>(src[2]), 640 * 432 / 4);

    ret = sws.set_src_opt(AV_PIX_FMT_YUV420P, 640, 432, err);
    TESTCHECKRET(ret);
    ret = sws.set_dst_opt(AV_PIX_FMT_BGR24, 320, 240, err);
    TESTCHECKRET(ret);
    ret = sws.lock_opt(err);
    TESTCHECKRET(ret);
    int size = sws.scale(src, srclinesize, 0, 432, dst, dstlinesize, err);
    std::cout << "sws " << size << " line" << std::endl;
    std::ofstream bgr("out.bgr", std::ios::binary);
    bgr.write(reinterpret_cast<char*>(dst[0]), dstsize);
    ret = sws.unlock_opt(err);
    TESTCHECKRET(ret);

    // 清理
    if (src != nullptr)
    {
        av_freep(&src[0]);
    }
    av_freep(&src);
    if (dst != nullptr)
    {
        av_freep(&dst[0]);
    }
    av_freep(&dst);
}

// 音频重采样
void test_swr()
{
    bool ret = false;
    std::string err;
    std::ifstream pcm("in.pcm", std::ios::binary);
    CSwr swr;

    // 分配音频数据内存
    uint8_t** src = nullptr;
    int srclinesize = 0;
    uint8_t** dst = nullptr;
    int dstlinesize = 0;

    // 分配音频数据内存
    int srcsize = av_samples_alloc_array_and_samples(&src, &srclinesize, 2, 44100, AV_SAMPLE_FMT_S16, 1);
    int dstsize = av_samples_alloc_array_and_samples(&dst, &dstlinesize, 2, 48000, AV_SAMPLE_FMT_S16P, 1);
    // 获取样本格式对应的每个样本大小(Byte)
    int persize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16P);
    // 获取布局对应的通道数
    int channel = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

    ret = swr.set_src_opt(AV_CH_LAYOUT_STEREO, 44100, AV_SAMPLE_FMT_S16, err);
    TESTCHECKRET(ret);
    ret = swr.set_dst_opt(AV_CH_LAYOUT_STEREO, 48000, AV_SAMPLE_FMT_S16P, err);
    TESTCHECKRET(ret);
    ret = swr.lock_opt(err);
    TESTCHECKRET(ret);

    std::ofstream outpcm("out.pcm", std::ios::binary);
    while (!pcm.eof())
    {
        pcm.read(reinterpret_cast<char*>(src[0]), srcsize);
        int size = swr.convert(dst, dstlinesize, (const uint8_t * *)(src), 44100, err);
        // 拷贝音频数据
        for (int i = 0; i < size; ++i) // 每个样本
        {
            for (int j = 0; j < channel; ++j) // 每个通道
            {
                outpcm.write(reinterpret_cast<const char*>(dst[j] + persize * i), persize);
            }
        }
    }

    ret = swr.unlock_opt(err);
    TESTCHECKRET(ret);

    // 清理
    if (src != nullptr)
    {
        av_freep(&src[0]);
    }
    av_freep(&src);
    if (dst != nullptr)
    {
        av_freep(&dst[0]);
    }
    av_freep(&dst);
}

// 采集桌面
void test_desktop()
{
    bool ret = false;
    std::string err;
    CDemux demux;
    CDecode decode;

    ret = demux.device_register_all(err);
    TESTCHECKRET(ret);
    ret = demux.set_input_format("gdigrab", err); //采集桌面
    TESTCHECKRET(ret);
    ret = demux.set_dic_opt("framerate", "15", err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxDesktopCB, &decode, err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = demux.set_input("desktop", err);
    TESTCHECKRET(ret);
    ret = demux.openinput(err);
    TESTCHECKRET(ret);

    g_vindex = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, err);
    std::cout << err << std::endl;
    g_aindex = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, err);
    std::cout << err << std::endl;

    ret = decode.set_dec_callback(DecVideoFrameCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = decode.copy_param(demux.get_steam_par(g_vindex, err), err);
    TESTCHECKRET(ret);
    ret = decode.codec_open(err);
    TESTCHECKRET(ret);

    ret = demux.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = demux.stopdemux(err);
    TESTCHECKRET(ret);
}

// 采集系统声音
void test_systemsound()
{
    bool ret = false;
    std::string err;
    CDemux demux;
    CDecode decode;

    ret = demux.device_register_all(err);
    TESTCHECKRET(ret);
    ret = demux.set_input_format("dshow", err); //采集声卡
    TESTCHECKRET(ret);
    ret = demux.set_dic_opt("framerate", "15", err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxSystemSoundCB, &decode, err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = demux.set_input("audio=virtual-audio-capturer", err);
    TESTCHECKRET(ret);
    ret = demux.openinput(err);
    TESTCHECKRET(ret);

    g_vindex = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, err);
    std::cout << err << std::endl;
    g_aindex = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, err);
    std::cout << err << std::endl;

    ret = decode.set_dec_callback(DecAudioFrameCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = decode.copy_param(demux.get_steam_par(g_aindex, err), err);
    TESTCHECKRET(ret);
    ret = decode.codec_open(err);
    TESTCHECKRET(ret);

    ret = demux.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = demux.stopdemux(err);
    TESTCHECKRET(ret);
}

// 输出h264
void test_output_h264()
{
    bool ret = false;
    std::string err;
    CDemux demux;
    COutput output;

    ret = demux.set_input("in.flv", err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxPacketCB_save, &output, err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux, err);
    TESTCHECKRET(ret);

    ret = demux.openinput(err);
    TESTCHECKRET(ret);

    g_vindex = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, err);
    std::cout << err << std::endl;

    auto par = demux.get_steam_par(g_vindex, err);
    TESTCHECKRET(ret);

    ret = output.set_output("out.h264", err);
    TESTCHECKRET(ret);
    g_vindex_output = output.add_stream(par->codec_id, err);
    output.copy_param(g_vindex_output, par, err);
    TESTCHECKRET(ret);
    ret = output.open(err);
    TESTCHECKRET(ret);

    ret = demux.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = output.close(err);
    TESTCHECKRET(ret);

    ret = demux.stopdemux(err);
    TESTCHECKRET(ret);
}

// 输出aac
void test_output_aac()
{
    bool ret = false;
    std::string err;
    CDemux demux;
    COutput output;

    ret = demux.set_input("in.flv", err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxPacketCB_save, &output, err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux, err);
    TESTCHECKRET(ret);

    ret = demux.openinput(err);
    TESTCHECKRET(ret);

    g_aindex = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, err);
    std::cout << err << std::endl;

    auto par = demux.get_steam_par(g_aindex, err);
    TESTCHECKRET(ret);

    ret = output.set_output("out.aac", err);
    TESTCHECKRET(ret);
    g_aindex_output = output.add_stream(par->codec_id, err);
    output.copy_param(g_aindex_output, par, err);
    TESTCHECKRET(ret);
    ret = output.open(err);
    TESTCHECKRET(ret);

    ret = demux.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = output.close(err);
    TESTCHECKRET(ret);

    ret = demux.stopdemux(err);
    TESTCHECKRET(ret);
}

// 输出mp3
void test_output_mp3()
{
    bool ret = false;
    std::string err;
    CDemux demux;
    COutput output;

    ret = demux.set_input("in.mkv", err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxPacketCB_save, &output, err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux, err);
    TESTCHECKRET(ret);

    ret = demux.openinput(err);
    TESTCHECKRET(ret);

    g_aindex = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, err);
    std::cout << err << std::endl;

    auto par = demux.get_steam_par(g_aindex, err);
    TESTCHECKRET(ret);

    ret = output.set_output("out.mp3", err);
    TESTCHECKRET(ret);
    g_aindex_output = output.add_stream(par->codec_id, err);
    output.copy_param(g_aindex_output, par, err);
    TESTCHECKRET(ret);
    ret = output.open(err);
    TESTCHECKRET(ret);

    ret = demux.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = output.close(err);
    TESTCHECKRET(ret);

    ret = demux.stopdemux(err);
    TESTCHECKRET(ret);
}

// 编码h264
void test_encode_h264()
{
    bool ret = false;
    std::string err;
    // out.yuv这个文件太大了，没有上传github，可以用解码的例子生成
    std::ifstream yuv("out.yuv", std::ios::binary);
    char buf[414720] = { 0 };
    CEncode encode;

    ret = encode.set_enc_callback(EncVideoFrameCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = encode.set_encodeid(AV_CODEC_ID_H264, err);
    TESTCHECKRET(ret);
    ret = encode.set_video_param(400000, 640, 432, { 1,25 }, { 25,1 }, 5, 0, AV_PIX_FMT_YUV420P, err);
    TESTCHECKRET(ret);

    auto frame = av_frame_alloc();
    frame->width = 640;
    frame->height = 432;
    frame->format = AV_PIX_FMT_YUV420P;
    av_frame_get_buffer(frame, 1);

    while (!yuv.eof())
    {
        yuv.read(buf, 414720);
        av_frame_make_writable(frame);
        memcpy(frame->data[0], buf, frame->linesize[0] * frame->height);
        memcpy(frame->data[1], buf + frame->linesize[0] * frame->height, frame->linesize[1] * frame->height / 2);
        memcpy(frame->data[2], buf + frame->linesize[0] * frame->height * 5 / 4, frame->linesize[2] * frame->height / 2);

        static int i = 0;
        frame->pts = i++;

        ret = encode.encode(frame, err);
        TESTCHECKRET(ret);
    }

    av_frame_free(&frame);

    ret = encode.close(err);
    TESTCHECKRET(ret);
}

// 编码mp3
void test_encode_mp3()
{
    bool ret = false;
    std::string err;
    // out.pcm这个文件太大了，没有上传github，可以用解码的例子生成
    std::ifstream pcm("out.pcm", std::ios::binary);
    char buf[10240] = { 0 };
    CEncode encode;
    int framesize = 0;

    ret = encode.set_enc_callback(EncAudioFrameCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = encode.set_encodeid(AV_CODEC_ID_MP3, err);
    TESTCHECKRET(ret);
    ret = encode.set_audio_param(64000, 44100, AV_CH_LAYOUT_STEREO, 2, AV_SAMPLE_FMT_FLTP, framesize, err);
    TESTCHECKRET(ret);

    auto frame = av_frame_alloc();
    frame->nb_samples = framesize;
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    auto size = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));
    av_frame_get_buffer(frame, 0);

    while (!pcm.eof())
    {
        pcm.read(buf, framesize * size * av_get_channel_layout_nb_channels(frame->channel_layout));
        av_frame_make_writable(frame);

        for (int i = 0; i < frame->nb_samples; ++i)
        {
            memcpy(frame->data[0] + size * i, buf + size * (2 * i), size);
            memcpy(frame->data[1] + size * i, buf + size * (2 * i + 1), size);
        }

        ret = encode.encode(frame, err);
        TESTCHECKRET(ret);
    }

    av_frame_free(&frame);

    ret = encode.close(err);
    TESTCHECKRET(ret);
}

// 录屏
void test_screen_capture()
{
    bool ret = false;
    std::string err;
    CDemux demuxdesktop;
    CDecode decodedesktop;
    CEncode encodedesktop;
    COutput output;

    //采集桌面
    ret = demuxdesktop.device_register_all(err);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_input_format("gdigrab", err); //采集桌面
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_dic_opt("framerate", "10", err);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_demux_callback(DemuxDesktopCB, &decodedesktop, err);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_demux_status_callback(DemuxStatusCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_input("desktop", err);
    TESTCHECKRET(ret);
    ret = demuxdesktop.openinput(err);
    TESTCHECKRET(ret);

    g_vindex = demuxdesktop.get_steam_index(AVMEDIA_TYPE_VIDEO, err);
    std::cout << err << std::endl;

    ret = decodedesktop.set_dec_callback(DecVideoFrameCB, &encodedesktop, err);
    TESTCHECKRET(ret);
    ret = decodedesktop.copy_param(demuxdesktop.get_steam_par(g_vindex, err), err);
    TESTCHECKRET(ret);
    ret = decodedesktop.codec_open(err);
    TESTCHECKRET(ret);

    // 编码h264
    ret = encodedesktop.set_enc_callback(EncVideoFrameCB, &output, err);
    TESTCHECKRET(ret);
    ret = encodedesktop.set_encodeid(AV_CODEC_ID_H264, err);
    TESTCHECKRET(ret);
    ret = encodedesktop.set_video_param(40000000, 1920, 1080, { 1,10 }, { 10,1 }, 5, 0, AV_PIX_FMT_YUV420P, err);
    TESTCHECKRET(ret);

    // 输出
    ret = output.set_output("capture.mp4", err);
    TESTCHECKRET(ret);
    g_vindex_output = output.add_stream(AV_CODEC_ID_H264, err);
    TESTCHECKRET(ret);
    ret = output.copy_param(g_vindex_output, encodedesktop.get_codectx(err), err);
    TESTCHECKRET(ret);

    // 开始
    ret = output.open(err);
    TESTCHECKRET(ret);
    ret = demuxdesktop.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    // 结束
    ret = demuxdesktop.stopdemux(err);
    TESTCHECKRET(ret);
    ret = encodedesktop.close(err);
    TESTCHECKRET(ret);
    ret = output.close(err);
    TESTCHECKRET(ret);
}

// 录音
void test_record()
{
    bool ret = false;
    std::string err;
    CDemux demuxsound;
    CDecode decodesound;
    CEncode encodesound;
    COutput output;

    // 采集系统声音
    ret = demuxsound.device_register_all(err);
    TESTCHECKRET(ret);
    ret = demuxsound.set_input_format("dshow", err); //采集声卡
    TESTCHECKRET(ret);
    ret = demuxsound.set_dic_opt("framerate", "15", err);
    TESTCHECKRET(ret);
    ret = demuxsound.set_demux_callback(DemuxSystemSoundCB, &decodesound, err);
    TESTCHECKRET(ret);
    ret = demuxsound.set_demux_status_callback(DemuxStatusCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = demuxsound.set_input("audio=virtual-audio-capturer", err);
    TESTCHECKRET(ret);
    ret = demuxsound.openinput(err);
    TESTCHECKRET(ret);

    g_aindex = demuxsound.get_steam_index(AVMEDIA_TYPE_AUDIO, err);
    std::cout << err << std::endl;

    ret = decodesound.set_dec_callback(DecAudioFrameCB, &encodesound, err);
    TESTCHECKRET(ret);
    ret = decodesound.copy_param(demuxsound.get_steam_par(g_aindex, err), err);
    TESTCHECKRET(ret);
    ret = decodesound.codec_open(err);
    TESTCHECKRET(ret);

    // 编码
    ret = encodesound.set_enc_callback(EncAudioFrameCB, &output, err);
    TESTCHECKRET(ret);
    ret = encodesound.set_encodeid(AV_CODEC_ID_AAC, err);
    TESTCHECKRET(ret);
    ret = encodesound.set_audio_param(64000, 44100, AV_CH_LAYOUT_STEREO, 2, AV_SAMPLE_FMT_FLTP, g_framesize, err);
    TESTCHECKRET(ret);
    std::cout << "framesize : " << g_framesize << std::endl;
    std::cout << "one framesize : " << av_samples_get_buffer_size(nullptr, 2, g_framesize, AV_SAMPLE_FMT_FLTP, 1) << std::endl;
    std::cin.get();

    // 输出
    ret = output.set_output("record.aac", err);
    TESTCHECKRET(ret);
    g_aindex_output = output.add_stream(AV_CODEC_ID_AAC, err);
    TESTCHECKRET(ret);
    ret = output.copy_param(g_aindex_output, encodesound.get_codectx(err), err);
    TESTCHECKRET(ret);

    // 开始
    ret = output.open(err);
    TESTCHECKRET(ret);
    ret = demuxsound.begindemux(err);
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    // 结束
    ret = demuxsound.stopdemux(err);
    TESTCHECKRET(ret);
    ret = encodesound.close(err);
    TESTCHECKRET(ret);
    ret = output.close(err);
    TESTCHECKRET(ret);
}

int main()
{
    test_demux();
    //test_decode_h264();
    //test_decode_aac();
    //test_decode_mp3();
    //test_sws();
    //test_swr();
    //test_desktop();
    //test_systemsound();
    //test_output_h264();
    //test_output_aac();
    //test_output_mp3();
    //test_encode_h264();
    //test_encode_mp3();
    //test_screen_capture();
    //test_record();
    return 0;
}