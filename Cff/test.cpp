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
#include <libavutil/time.h>
}


//#define SEEK

int g_vindex = -1;
int g_aindex = -1;
int g_vindex_output = -1;
int g_aindex_output = -1;
int g_framesize = 1024;

#define TESTCHECKRET(ret)\
if(ret < 0)\
{\
    std::cerr << av_err2str(ret) << std::endl;\
}

void DemuxStatusCB(CDemux::STATUS status, int err, void* param)
{
    std::cout << std::this_thread::get_id() << " got a status " << status << " " << av_err2str(err) << std::endl;
}

void DemuxPacketCB(const AVPacket* packet, AVRational timebase, void* param)
{
    /*auto timestamp = av_rescale_q_rnd(packet->pts, timebase, { 1, 1 }, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    std::cout << std::this_thread::get_id() <<
        " got a packet , index : " << packet->stream_index <<
        " timestamp : " << timestamp << std::endl;*/

#ifdef SEEK
    CDemux* demux = static_cast<CDemux*>(param);
    if (demux != nullptr && timestamp > 10)
    {
        TESTCHECKRET(demux->seek(5, packet->stream_index, AVSEEK_FLAG_ANY));
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

void DemuxPacketCB_save(const AVPacket* packet, AVRational timebase, void* param)
{
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
        TESTCHECKRET(output->write_frame(pkt));
    }
}

void DemuxDesktopCB(const AVPacket* packet, AVRational timebase, void* param)
{
    /*auto timestamp = av_rescale_q_rnd(packet->pts, timebase, { 1, 1 }, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    std::cout << std::this_thread::get_id() <<
        " got a packet , index : " << packet->stream_index <<
        " timestamp : " << timestamp << std::endl;*/

    CDecode* decode = static_cast<CDecode*>(param);
    std::string err;
    if (decode != nullptr)
    {
        TESTCHECKRET(decode->decode(packet));
    }
}

void DemuxSystemSoundCB(const AVPacket* packet, AVRational timebase, void* param)
{
    /*auto timestamp = av_rescale_q_rnd(packet->pts, timebase, { 1, 1 }, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    std::cout << std::this_thread::get_id() <<
        " got a packet , index : " << packet->stream_index <<
        " timestamp : " << timestamp << std::endl;*/

    CDecode* decode = static_cast<CDecode*>(param);
    std::string err;
    if (decode != nullptr)
    {
        int ret = decode->decode(packet);
        TESTCHECKRET(ret);
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
            static bool binit = false;
            static CSws sws;
            static AVFrame f = { 0 };
            if (!binit)
            {
                f.width = 1920;
                f.height = 1080;
                f.format = AV_PIX_FMT_YUV420P;
                av_frame_get_buffer(&f, 1);

                TESTCHECKRET(sws.set_src_opt(static_cast<AVPixelFormat>(frame->format), frame->width, frame->height));
                TESTCHECKRET(sws.set_dst_opt(AV_PIX_FMT_YUV420P, 1920, 1080));
                TESTCHECKRET(sws.lock_opt());

                binit = true;
            }
            
            av_frame_make_writable(&f);
            
            int lines = sws.scale(frame->data, frame->linesize, 0, frame->height, f.data, f.linesize);
            //std::cout << "sws " << lines << " lines." << std::endl;
            static int64_t start = frame->pts;
            f.pts = frame->pts - start;
            f.pkt_dts = frame->pkt_dts - start;
            f.pkt_duration = frame->pkt_duration;
            f.best_effort_timestamp = frame->best_effort_timestamp - start;
            TESTCHECKRET(enc->encode(&f));
            return;
        }
        else
        {
            //TESTCHECKRET(enc->encode(frame));
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
        video.write(reinterpret_cast<const char*>(frame->data[0]), static_cast<long long>(frame->linesize[0]) * frame->height);
        video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
    }
    else if (frame->format == AV_PIX_FMT_YUV420P)
    {
        std::cout << "got a yuv420p " << frame->width << "x" << frame->height << std::endl;
        static std::ofstream video("out.yuv", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), static_cast<long long>(frame->linesize[0]) * frame->height);
        video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
        video.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
    }
    else if (frame->format == AV_PIX_FMT_BGRA)
    {
        std::cout << "got a bgra" << std::endl;
        static std::ofstream video("out.bgra", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), static_cast<long long>(frame->linesize[0]) * frame->height);
    }
}

void DecAudioFrameCB(const AVFrame * frame, void* param)
{
    //std::cout << "got dec frame in DecAudioFrameCB..." << std::endl;
    std::string err;
    CEncode* enc = static_cast<CEncode*>(param);
    if (enc != nullptr)
    {
        if (frame->format != AV_SAMPLE_FMT_FLTP)
        {
            static AVFrame fltp_frame = { 0 };
            static AVFrame ff = { 0 };
            static CSwr swr;//static是修复杂音的关键!数据有缓存!
            static bool binit = false;
            if (!binit)
            {
                fltp_frame.nb_samples = 44100;
                fltp_frame.format = AV_SAMPLE_FMT_FLTP;
                fltp_frame.channel_layout = AV_CH_LAYOUT_STEREO;
                fltp_frame.sample_rate = 44100;
                av_frame_get_buffer(&fltp_frame, 1);

                ff.nb_samples = g_framesize;
                ff.format = AV_SAMPLE_FMT_FLTP;
                ff.channel_layout = AV_CH_LAYOUT_STEREO;
                av_frame_get_buffer(&ff, 1);

                TESTCHECKRET(swr.set_src_opt(AV_CH_LAYOUT_STEREO, frame->sample_rate, static_cast<AVSampleFormat>(frame->format)));
                TESTCHECKRET(swr.set_dst_opt(AV_CH_LAYOUT_STEREO, fltp_frame.sample_rate, static_cast<AVSampleFormat>(fltp_frame.format)));
                TESTCHECKRET(swr.lock_opt());

                binit = true;
            }
            
            fltp_frame.nb_samples = 44100;
            av_frame_make_writable(&fltp_frame);
            
            int samples = swr.convert(reinterpret_cast<uint8_t**>(&fltp_frame.data), fltp_frame.nb_samples, (const uint8_t**)(frame->data), frame->nb_samples);
            //std::cout << "convert samples : " << samples << std::endl;
            if (samples <= 0)
            {
                std::cout << "convert samples : " << samples << std::endl;
            }
            fltp_frame.nb_samples = samples;

            static int64_t start = av_gettime();
            static int64_t lastpts = -1;

            // AAC输入大小有要求g_framesize
            static AVAudioFifo* fifo = av_audio_fifo_alloc(static_cast<AVSampleFormat>(fltp_frame.format), av_get_channel_layout_nb_channels(fltp_frame.channel_layout), fltp_frame.sample_rate * 2);
            av_audio_fifo_write(fifo, reinterpret_cast<void**>(fltp_frame.data), fltp_frame.nb_samples);
            while (av_audio_fifo_size(fifo) >= g_framesize)
            {
                auto pts = av_gettime() - start;
                if (pts <= lastpts)
                {
                    pts = lastpts + 1;
                }
                av_frame_make_writable(&ff);
                av_audio_fifo_read(fifo, reinterpret_cast<void**>(ff.data), g_framesize);
                ff.pts = pts;
                ff.pkt_dts = ff.pts;
                ff.pkt_duration = av_rescale_q(g_framesize, { 1, 44100 }, { 1, AV_TIME_BASE });
                ff.pkt_pos = -1;
                lastpts = pts;
                TESTCHECKRET(enc->encode(&ff));
            }
            return;
        }
        else
        {
            TESTCHECKRET(enc->encode(frame));
            return;
        }
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
        AVRational timebase = { 0 };
        out->get_timebase(g_vindex_output, timebase);
        const_cast<AVPacket*>(packet)->stream_index = g_vindex_output;
        const_cast<AVPacket*>(packet)->pts = av_rescale_q(packet->pts, { 1, AV_TIME_BASE }, timebase);
        const_cast<AVPacket*>(packet)->dts = av_rescale_q(packet->dts, { 1, AV_TIME_BASE }, timebase);
        const_cast<AVPacket*>(packet)->duration = av_rescale_q(packet->duration, { 1, AV_TIME_BASE }, timebase);
        out->write_frame(const_cast<AVPacket*>(packet));
    }
    else
    {
        static std::ofstream h264("encode.h264", std::ios::binary);
        h264.write(reinterpret_cast<char*>(packet->data), packet->size);
    }
}

void EncAudioFrameCB(const AVPacket * packet, void* param)
{
    //std::cout << "got enc frame in EncAudioFrameCB..." << std::endl;
    std::string err;
    COutput* out = static_cast<COutput*>(param);
    if (out != nullptr)
    {
        AVRational timebase = { 0 };
        out->get_timebase(g_aindex_output, timebase);
        const_cast<AVPacket*>(packet)->stream_index = g_aindex_output;
        const_cast<AVPacket*>(packet)->pts = av_rescale_q(packet->pts, { 1, AV_TIME_BASE }, timebase);
        const_cast<AVPacket*>(packet)->dts = av_rescale_q(packet->dts, { 1, AV_TIME_BASE }, timebase);
        const_cast<AVPacket*>(packet)->duration = av_rescale_q(packet->duration, { 1, AV_TIME_BASE }, timebase);
        //std::cout << "pts : " << 1.0 * packet->pts * timebase.num / timebase.den << std::endl;
        out->write_frame(const_cast<AVPacket*>(packet));
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
    int ret = 0;
    CDemux demux;

    ret = demux.set_input("in.flv");
    //ret = demux.set_input("in.h264");
    //ret = demux.set_input("in.aac");
    TESTCHECKRET(ret);

    ret = demux.set_demux_callback(DemuxPacketCB, &demux);
    TESTCHECKRET(ret);

    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux);
    TESTCHECKRET(ret);

    ret = demux.openinput();
    TESTCHECKRET(ret);

    ret = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, g_vindex);
    TESTCHECKRET(ret);
    std::cout << "v : " << g_vindex << std::endl;
    ret = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, g_aindex);
    TESTCHECKRET(ret);
    std::cout << "a : " << g_aindex << std::endl;

    ret = demux.set_bsf_name(g_vindex, "h264_mp4toannexb");
    TESTCHECKRET(ret);
    ret = demux.set_bsf_name(g_aindex, "aac_adtstoasc"); // unuseable
    TESTCHECKRET(ret);

    ret = demux.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = demux.stopdemux();
    TESTCHECKRET(ret);
}

// 解码h264
void test_decode_h264()
{
    int ret = 0;
    std::ifstream h264("in.h264", std::ios::binary);
    char buf[1024] = { 0 };
    CDecode decode;

    ret = decode.set_dec_callback(DecVideoFrameCB, nullptr);
    TESTCHECKRET(ret);
    //ret = decode.set_hwdec_type(AV_HWDEVICE_TYPE_DXVA2, true);
    //TESTCHECKRET(ret);
    ret = decode.set_codeid(AV_CODEC_ID_H264);
    TESTCHECKRET(ret);
    ret = decode.codec_open();
    TESTCHECKRET(ret);

    while (!h264.eof())
    {
        h264.read(buf, sizeof(buf));
        ret = decode.decode(buf, sizeof(buf));
        TESTCHECKRET(ret);
    }
}

// 解码aac
void test_decode_aac()
{
    int ret = 0;
    std::ifstream aac("in.aac", std::ios::binary);
    char buf[1024] = { 0 };
    CDecode decode;

    ret = decode.set_dec_callback(DecAudioFrameCB, nullptr);
    TESTCHECKRET(ret);
    ret = decode.set_codeid(AV_CODEC_ID_AAC);
    TESTCHECKRET(ret);
    ret = decode.codec_open();
    TESTCHECKRET(ret);

    while (!aac.eof())
    {
        aac.read(buf, sizeof(buf));
        ret = decode.decode(buf, sizeof(buf));
        TESTCHECKRET(ret);
    }
}

// 解码mp3
void test_decode_mp3()
{
    int ret = 0;
    std::ifstream mp3("in.mp3", std::ios::binary);
    char buf[1024] = { 0 };
    CDecode decode;

    ret = decode.set_dec_callback(DecAudioFrameCB, nullptr);
    TESTCHECKRET(ret);
    ret = decode.set_codeid(AV_CODEC_ID_MP3);
    TESTCHECKRET(ret);
    ret = decode.codec_open();
    TESTCHECKRET(ret);

    while (!mp3.eof())
    {
        mp3.read(buf, sizeof(buf));
        ret = decode.decode(buf, sizeof(buf));
        TESTCHECKRET(ret);
    }
}

// 视频帧转换
void test_sws()
{
    int ret = 0;
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

    ret = sws.set_src_opt(AV_PIX_FMT_YUV420P, 640, 432);
    TESTCHECKRET(ret);
    ret = sws.set_dst_opt(AV_PIX_FMT_BGR24, 320, 240);
    TESTCHECKRET(ret);
    ret = sws.lock_opt();
    TESTCHECKRET(ret);
    int size = sws.scale(src, srclinesize, 0, 432, dst, dstlinesize);
    std::cout << "sws " << size << " line" << std::endl;
    std::ofstream bgr("out.bgr", std::ios::binary);
    bgr.write(reinterpret_cast<char*>(dst[0]), dstsize);
    ret = sws.unlock_opt();
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
    int ret = 0;
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

    ret = swr.set_src_opt(AV_CH_LAYOUT_STEREO, 44100, AV_SAMPLE_FMT_S16);
    TESTCHECKRET(ret);
    ret = swr.set_dst_opt(AV_CH_LAYOUT_STEREO, 48000, AV_SAMPLE_FMT_S16P);
    TESTCHECKRET(ret);
    ret = swr.lock_opt();
    TESTCHECKRET(ret);

    std::ofstream outpcm("out.pcm", std::ios::binary);
    while (!pcm.eof())
    {
        pcm.read(reinterpret_cast<char*>(src[0]), srcsize);
        int size = swr.convert(dst, dstlinesize, (const uint8_t * *)(src), 44100);
        // 拷贝音频数据
        for (int i = 0; i < size; ++i) // 每个样本
        {
            for (int j = 0; j < channel; ++j) // 每个通道
            {
                outpcm.write(reinterpret_cast<const char*>(dst[j] + persize * i), persize);
            }
        }
    }

    ret = swr.unlock_opt();
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
    int ret = 0;
    CDemux demux;
    CDecode decode;

    ret = demux.device_register_all();
    TESTCHECKRET(ret);
    ret = demux.set_input_format("gdigrab"); //采集桌面
    TESTCHECKRET(ret);
    ret = demux.set_dic_opt("framerate", "15");
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxDesktopCB, &decode);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, nullptr);
    TESTCHECKRET(ret);
    ret = demux.set_input("desktop");
    TESTCHECKRET(ret);
    ret = demux.openinput();
    TESTCHECKRET(ret);

    ret = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, g_vindex);
    std::cout << "g_vindex : " << g_vindex << std::endl;
    ret = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, g_aindex);
    std::cout << "g_aindex : " << g_aindex << std::endl;

    ret = decode.set_dec_callback(DecVideoFrameCB, nullptr);
    TESTCHECKRET(ret);

    const AVCodecParameters* par = nullptr;
    ret = demux.get_steam_par(g_vindex, par);
    TESTCHECKRET(ret);
    ret = decode.copy_param(par);
    TESTCHECKRET(ret);
    ret = decode.codec_open();
    TESTCHECKRET(ret);

    ret = demux.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = demux.stopdemux();
    TESTCHECKRET(ret);
}

// 采集系统声音
void test_systemsound()
{
    int ret = 0;
    CDemux demux;
    CDecode decode;

    ret = demux.device_register_all();
    TESTCHECKRET(ret);
    ret = demux.set_input_format("dshow"); //采集声卡
    TESTCHECKRET(ret);
    ret = demux.set_dic_opt("framerate", "15");
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxSystemSoundCB, &decode);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, nullptr);
    TESTCHECKRET(ret);
    ret = demux.set_input("audio=virtual-audio-capturer");
    TESTCHECKRET(ret);
    ret = demux.openinput();
    TESTCHECKRET(ret);

    ret = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, g_vindex);
    std::cout << "g_vindex : " << g_vindex << std::endl;
    ret = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, g_aindex);
    std::cout << "g_aindex : " << g_aindex << std::endl;

    ret = decode.set_dec_callback(DecAudioFrameCB, nullptr);
    TESTCHECKRET(ret);
    const AVCodecParameters* par = nullptr;
    ret = demux.get_steam_par(g_aindex, par);
    TESTCHECKRET(ret);
    ret = decode.copy_param(par);
    TESTCHECKRET(ret);
    ret = decode.codec_open();
    TESTCHECKRET(ret);

    ret = demux.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = demux.stopdemux();
    TESTCHECKRET(ret);
}

// 输出h264
void test_output_h264()
{
    int ret = 0;
    CDemux demux;
    COutput output;

    ret = demux.set_input("in.flv");
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxPacketCB_save, &output);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux);
    TESTCHECKRET(ret);

    ret = demux.openinput();
    TESTCHECKRET(ret);

    ret = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, g_vindex);
    TESTCHECKRET(ret);

    const AVCodecParameters* par = nullptr;
    ret = demux.get_steam_par(g_vindex, par);
    TESTCHECKRET(ret);

    ret = output.set_output("out.h264");
    TESTCHECKRET(ret);
    ret = output.add_stream(par->codec_id, g_vindex_output);
    output.copy_param(g_vindex_output, par);
    TESTCHECKRET(ret);
    ret = output.open();
    TESTCHECKRET(ret);

    ret = demux.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = output.close();
    TESTCHECKRET(ret);

    ret = demux.stopdemux();
    TESTCHECKRET(ret);
}

// 输出aac
void test_output_aac()
{
    int ret = 0;
    CDemux demux;
    COutput output;

    ret = demux.set_input("in.flv");
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxPacketCB_save, &output);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux);
    TESTCHECKRET(ret);

    ret = demux.openinput();
    TESTCHECKRET(ret);

    ret = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, g_aindex);
    TESTCHECKRET(ret);

    const AVCodecParameters* par = nullptr;
    ret = demux.get_steam_par(g_aindex, par);
    TESTCHECKRET(ret);

    ret = output.set_output("out.aac");
    TESTCHECKRET(ret);
    ret = output.add_stream(par->codec_id, g_aindex_output);
    output.copy_param(g_aindex_output, par);
    TESTCHECKRET(ret);
    ret = output.open();
    TESTCHECKRET(ret);

    ret = demux.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = output.close();
    TESTCHECKRET(ret);

    ret = demux.stopdemux();
    TESTCHECKRET(ret);
}

// 输出mp3
void test_output_mp3()
{
    int ret = 0;
    CDemux demux;
    COutput output;

    ret = demux.set_input("in.mkv");
    TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxPacketCB_save, &output);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DemuxStatusCB, &demux);
    TESTCHECKRET(ret);

    ret = demux.openinput();
    TESTCHECKRET(ret);

    ret = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, g_aindex);
    TESTCHECKRET(ret);

    const AVCodecParameters* par = nullptr;
    ret = demux.get_steam_par(g_aindex, par);
    TESTCHECKRET(ret);

    ret = output.set_output("out.mp3");
    TESTCHECKRET(ret);
    ret = output.add_stream(par->codec_id, g_aindex_output);
    output.copy_param(g_aindex_output, par);
    TESTCHECKRET(ret);
    ret = output.open();
    TESTCHECKRET(ret);

    ret = demux.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    ret = output.close();
    TESTCHECKRET(ret);

    ret = demux.stopdemux();
    TESTCHECKRET(ret);
}

// 编码h264
void test_encode_h264()
{
    int ret = 0;
    // out.yuv这个文件太大了，没有上传github，可以用解码的例子生成
    std::ifstream yuv("out.yuv", std::ios::binary);
    char buf[414720] = { 0 };
    CEncode encode;

    ret = encode.set_enc_callback(EncVideoFrameCB, nullptr);
    TESTCHECKRET(ret);
    ret = encode.set_encodeid(AV_CODEC_ID_H264);
    TESTCHECKRET(ret);
    ret = encode.set_video_param(400000, 640, 432, { 1,25 }, { 25,1 }, 5, 0, AV_PIX_FMT_YUV420P);
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

        ret = encode.encode(frame);
        TESTCHECKRET(ret);
    }

    av_frame_free(&frame);

    ret = encode.close();
    TESTCHECKRET(ret);
}

// 编码mp3
void test_encode_mp3()
{
    int ret = 0;
    // out.pcm这个文件太大了，没有上传github，可以用解码的例子生成
    std::ifstream pcm("out.pcm", std::ios::binary);
    char buf[10240] = { 0 };
    CEncode encode;
    int framesize = 0;

    ret = encode.set_enc_callback(EncAudioFrameCB, nullptr);
    TESTCHECKRET(ret);
    ret = encode.set_encodeid(AV_CODEC_ID_MP3);
    TESTCHECKRET(ret);
    ret = encode.set_audio_param(64000, 44100, AV_CH_LAYOUT_STEREO, 2, AV_SAMPLE_FMT_FLTP, framesize);
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

        ret = encode.encode(frame);
        TESTCHECKRET(ret);
    }

    av_frame_free(&frame);

    ret = encode.close();
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
    ret = demuxdesktop.device_register_all();
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_input_format("gdigrab"); //采集桌面
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_dic_opt("framerate", "30");
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_demux_callback(DemuxDesktopCB, &decodedesktop);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_demux_status_callback(DemuxStatusCB, nullptr);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_input("desktop");
    TESTCHECKRET(ret);
    ret = demuxdesktop.openinput();
    TESTCHECKRET(ret);

    g_vindex = demuxdesktop.get_steam_index(AVMEDIA_TYPE_VIDEO, g_vindex);
    std::cout << err << std::endl;

    ret = decodedesktop.set_dec_callback(DecVideoFrameCB, &encodedesktop);
    TESTCHECKRET(ret);
    const AVCodecParameters* par = nullptr;
    demuxdesktop.get_steam_par(g_vindex, par);
    ret = decodedesktop.copy_param(par);
    TESTCHECKRET(ret);
    ret = decodedesktop.codec_open();
    TESTCHECKRET(ret);

    // 编码h264
    ret = encodedesktop.set_enc_callback(EncVideoFrameCB, &output);
    TESTCHECKRET(ret);
    ret = encodedesktop.set_encodeid(AV_CODEC_ID_H264);
    TESTCHECKRET(ret);
    ret = encodedesktop.set_video_param(8500000, 1920, 1080, { 1,30 }, { 30,1 }, 120, 60, AV_PIX_FMT_YUV420P);
    TESTCHECKRET(ret);

    // 输出
    ret = output.set_output("capture.mp4");
    TESTCHECKRET(ret);
    ret = output.add_stream(AV_CODEC_ID_H264, g_vindex_output);
    TESTCHECKRET(ret);
    const AVCodecContext* codectx = nullptr;
    ret = encodedesktop.get_codectx(codectx);
    TESTCHECKRET(ret);
    ret = output.copy_param(g_vindex_output, codectx);
    TESTCHECKRET(ret);

    // 开始
    ret = output.open();
    TESTCHECKRET(ret);
    ret = demuxdesktop.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    // 结束
    ret = demuxdesktop.stopdemux();
    TESTCHECKRET(ret);
    ret = encodedesktop.close();
    TESTCHECKRET(ret);
    ret = output.close();
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
    ret = demuxsound.device_register_all();
    TESTCHECKRET(ret);
    ret = demuxsound.set_input_format("dshow"); //采集声卡
    TESTCHECKRET(ret);
    ret = demuxsound.set_dic_opt("framerate", "15");
    TESTCHECKRET(ret);
    ret = demuxsound.set_demux_callback(DemuxSystemSoundCB, &decodesound);
    TESTCHECKRET(ret);
    ret = demuxsound.set_demux_status_callback(DemuxStatusCB, nullptr);
    TESTCHECKRET(ret);
    ret = demuxsound.set_input("audio=virtual-audio-capturer");
    TESTCHECKRET(ret);
    ret = demuxsound.openinput();
    TESTCHECKRET(ret);

    g_aindex = demuxsound.get_steam_index(AVMEDIA_TYPE_AUDIO, g_aindex);
    std::cout << err << std::endl;

    ret = decodesound.set_dec_callback(DecAudioFrameCB, &encodesound);
    TESTCHECKRET(ret);
    const AVCodecParameters* par = nullptr;
    demuxsound.get_steam_par(g_aindex, par);
    ret = decodesound.copy_param(par);
    TESTCHECKRET(ret);
    ret = decodesound.codec_open();
    TESTCHECKRET(ret);

    // 编码
    ret = encodesound.set_enc_callback(EncAudioFrameCB, &output);
    TESTCHECKRET(ret);
    ret = encodesound.set_encodeid(AV_CODEC_ID_AAC);
    TESTCHECKRET(ret);
    ret = encodesound.set_audio_param(128000, 44100, AV_CH_LAYOUT_STEREO, 2, AV_SAMPLE_FMT_FLTP, g_framesize);
    TESTCHECKRET(ret);
    std::cout << "framesize : " << g_framesize << std::endl;
    std::cout << "one framesize : " << av_samples_get_buffer_size(nullptr, 2, g_framesize, AV_SAMPLE_FMT_FLTP, 1) << std::endl;
    //std::cin.get();

    // 输出
    ret = output.set_output("record.aac");
    TESTCHECKRET(ret);
    ret = output.add_stream(AV_CODEC_ID_AAC, g_aindex_output);
    TESTCHECKRET(ret);
    const AVCodecContext* codectx = nullptr;
    ret = encodesound.get_codectx(codectx);
    TESTCHECKRET(ret);
    ret = output.copy_param(g_aindex_output, codectx);
    TESTCHECKRET(ret);

    // 开始
    ret = output.open();
    TESTCHECKRET(ret);
    ret = demuxsound.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    // 结束
    ret = demuxsound.stopdemux();
    TESTCHECKRET(ret);
    ret = encodesound.close();
    TESTCHECKRET(ret);
    ret = output.close();
    TESTCHECKRET(ret);
}

// 录屏录音
void test_capture_record()
{
    bool ret = false;
    std::string err;
    CDemux demuxdesktop;
    CDecode decodedesktop;
    CEncode encodedesktop;
    CDemux demuxsound;
    CDecode decodesound;
    CEncode encodesound;
    COutput output;

    //采集桌面
    ret = demuxdesktop.device_register_all();
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_input_format("gdigrab"); //采集桌面
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_dic_opt("framerate", "30");
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_demux_callback(DemuxDesktopCB, &decodedesktop);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_demux_status_callback(DemuxStatusCB, nullptr);
    TESTCHECKRET(ret);
    ret = demuxdesktop.set_input("desktop");
    TESTCHECKRET(ret);
    ret = demuxdesktop.openinput();
    TESTCHECKRET(ret);

    g_vindex = demuxdesktop.get_steam_index(AVMEDIA_TYPE_VIDEO, g_vindex);
    std::cout << err << std::endl;

    ret = decodedesktop.set_dec_callback(DecVideoFrameCB, &encodedesktop);
    TESTCHECKRET(ret);
    const AVCodecParameters* par = nullptr;
    demuxdesktop.get_steam_par(g_vindex, par);
    ret = decodedesktop.copy_param(par);
    TESTCHECKRET(ret);
    ret = decodedesktop.codec_open();
    TESTCHECKRET(ret);

    // 采集系统声音
    ret = demuxsound.device_register_all();
    TESTCHECKRET(ret);
    ret = demuxsound.set_input_format("dshow"); //采集声卡
    TESTCHECKRET(ret);
    ret = demuxsound.set_dic_opt("framerate", "15");
    TESTCHECKRET(ret);
    ret = demuxsound.set_demux_callback(DemuxSystemSoundCB, &decodesound);
    TESTCHECKRET(ret);
    ret = demuxsound.set_demux_status_callback(DemuxStatusCB, nullptr);
    TESTCHECKRET(ret);
    ret = demuxsound.set_input("audio=virtual-audio-capturer");
    TESTCHECKRET(ret);
    ret = demuxsound.openinput();
    TESTCHECKRET(ret);

    g_aindex = demuxsound.get_steam_index(AVMEDIA_TYPE_AUDIO, g_aindex);
    std::cout << err << std::endl;

    ret = decodesound.set_dec_callback(DecAudioFrameCB, &encodesound);
    TESTCHECKRET(ret);
    demuxdesktop.get_steam_par(g_aindex, par);
    ret = decodesound.copy_param(par);
    TESTCHECKRET(ret);
    ret = decodesound.codec_open();
    TESTCHECKRET(ret);

    // 编码h264
    ret = encodedesktop.set_enc_callback(EncVideoFrameCB, &output);
    TESTCHECKRET(ret);
    ret = encodedesktop.set_encodeid(AV_CODEC_ID_H264);
    TESTCHECKRET(ret);
    ret = encodedesktop.set_video_param(8500000, 1920, 1080, { 1,30 }, { 30,1 }, 150, 300, AV_PIX_FMT_YUV420P);
    TESTCHECKRET(ret);

    // 编码
    ret = encodesound.set_enc_callback(EncAudioFrameCB, &output);
    TESTCHECKRET(ret);
    ret = encodesound.set_encodeid(AV_CODEC_ID_AAC);
    TESTCHECKRET(ret);
    ret = encodesound.set_audio_param(128000, 44100, AV_CH_LAYOUT_STEREO, 2, AV_SAMPLE_FMT_FLTP, g_framesize);
    TESTCHECKRET(ret);
    std::cout << "framesize : " << g_framesize << std::endl;
    std::cout << "one framesize : " << av_samples_get_buffer_size(nullptr, 2, g_framesize, AV_SAMPLE_FMT_FLTP, 1) << std::endl;

    // 输出
    ret = output.set_output("capture.mp4");
    TESTCHECKRET(ret);
    ret = output.add_stream(AV_CODEC_ID_H264, g_vindex_output);
    TESTCHECKRET(ret);

    const AVCodecContext* codectx = nullptr;
    ret = encodesound.get_codectx(codectx);
    TESTCHECKRET(ret);
    ret = output.copy_param(g_vindex_output, codectx);
    TESTCHECKRET(ret);
    ret = output.add_stream(AV_CODEC_ID_AAC, g_aindex_output);
    TESTCHECKRET(ret);
    ret = encodesound.get_codectx(codectx);
    TESTCHECKRET(ret);
    ret = output.copy_param(g_aindex_output, codectx);
    TESTCHECKRET(ret);

    // 开始
    ret = output.open();
    TESTCHECKRET(ret);
    ret = demuxdesktop.begindemux();
    TESTCHECKRET(ret);
    ret = demuxsound.begindemux();
    TESTCHECKRET(ret);

    std::cout << "input to stop demuxing." << std::endl;
    std::cin.get();

    // 结束
    ret = demuxdesktop.stopdemux();
    TESTCHECKRET(ret);
    ret = demuxsound.stopdemux();
    TESTCHECKRET(ret);
    ret = encodedesktop.close();
    TESTCHECKRET(ret);
    ret = encodesound.close();
    TESTCHECKRET(ret);
    ret = output.close();
    TESTCHECKRET(ret);
}

int main()
{
    //av_log_set_level(AV_LOG_MAX_OFFSET);
    //test_demux();
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
    test_encode_mp3();
    //test_screen_capture();
    //test_record();
    //test_capture_record();
    return 0;
}