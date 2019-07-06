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
#include <iostream>
#include <fstream>

//#define SEEK

int g_vindex = -1;
int g_aindex = -1;

#define TESTCHECKRET(ret)\
if(!ret)\
{\
    std::cerr << err << std::endl;\
}

#define TESTCHECKRET2(ret)\
if(!ret)\
{\
    std::cerr << err << std::endl;\
    bret = false;\
    return;\
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
    else if(packet->stream_index == g_aindex)
    {
        // 没有打adts头，aac不能正常播放
        static std::ofstream out("out.aac", std::ios::binary | std::ios::trunc);
        if (out.is_open())
        {
            out.write(reinterpret_cast<char*>(const_cast<uint8_t*>(packet->data)), packet->size);
        }
    }
}

void DecFrameCB(const AVFrame* frame, void* param)
{
    std::string err;
    CDecode* dec = static_cast<CDecode*>(param);

    if (frame->format == AV_PIX_FMT_D3D11)
    {
        std::cout << "AV_PIX_FMT_D3D11 frame." << std::endl;
    }
    else if (frame->format == AV_PIX_FMT_DXVA2_VLD)
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
    else if (frame->format == AV_PIX_FMT_BGRA)
    {
        static std::ofstream video("out.bgra", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
    }
    else if (frame->format == AV_PIX_FMT_YUV420P)
    {
        std::cout << "got a yuv420p " << frame->width << "x" << frame->height << std::endl;
        static std::ofstream video("out.yuv", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
        video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
        video.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
    }
    else if (frame->format == AV_SAMPLE_FMT_S16)
    {
        static std::ofstream audio("outs16.pcm", std::ios::binary | std::ios::trunc);
        // 非平面格式，就直接拷贝data[0]
        audio.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0]);
    }
    else if (frame->format == AV_SAMPLE_FMT_FLTP)
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

    ret = decode.set_dec_callback(DecFrameCB, &decode, err);
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

    ret = decode.set_dec_callback(DecFrameCB, &decode, err);
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

    ret = decode.set_dec_callback(DecFrameCB, &decode, err);
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

    av_freep(&src[0]);
    av_freep(&dst[0]);
}

int main()
{
    //test_demux();
    //test_decode_h264();
    //test_decode_aac();
    //test_decode_mp3();
    //test_sws();
    return 0;
}