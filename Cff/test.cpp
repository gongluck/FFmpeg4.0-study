#include "CDemux.h"
#include "CDecode.h"
#include "CSws.h"
#include "CSwr.h"
#include <iostream>
#include <fstream>

CSws g_sws;
uint8_t* g_pointers[4] = { 0 };
int g_linesizes[4] = { 0 };

CSwr g_swr;
uint8_t** g_data = nullptr;
int g_linesize = 0;
int64_t g_layout = AV_CH_LAYOUT_STEREO;
int g_rate = 44100;
enum AVSampleFormat g_fmt = AV_SAMPLE_FMT_DBLP;

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

CDemux demux;
CDecode decode;
CDecode decode2;
int vindex = -1;
int aindex = -1;

void DecStatusCB(CDemux::STATUS status, std::string err, void* param)
{
    std::cout << std::this_thread::get_id() << " got a status " << status << " " << err << std::endl;
}

void DemuxPacketCB(const AVPacket* packet, int64_t timestamp, void* param)
{
    std::cout << std::this_thread::get_id() << 
        " got a packet , index : " << packet->stream_index <<
        " timestamp : " << timestamp << std::endl;
    std::string err;
    bool ret;
    if (packet->stream_index == vindex)
    {
        ret = decode.decode(packet, err);
        if (!ret)
        {
            std::cout << "deocde v err : " << err << std::endl;
        }
    }
    else if (packet->stream_index == aindex)
    {
        ret = decode2.decode(packet, err);
        if (!ret)
        {
            std::cout << "deocde a err : " << err << std::endl;
        }
    }
    if (timestamp > 10)
    {
        demux.seek(5, packet->stream_index, AVSEEK_FLAG_ANY, err);
    }
}

void DecFrameCB(const AVFrame* frame, void* param)
{
    //std::cout << std::this_thread::get_id() << " got a frame."  << std::endl;
    std::string err;
    CDecode* dec = static_cast<CDecode*>(param);

    if (frame->format == AV_PIX_FMT_D3D11)
    {
        std::cout << "AV_PIX_FMT_D3D11 frame." << std::endl;
    }
    else if (frame->format == AV_PIX_FMT_DXVA2_VLD)
    {
        std::cout << "AV_PIX_FMT_DXVA2_VLD frame." << std::endl;
    }
    else if (frame->format == AV_PIX_FMT_NV12)
    {
        std::cout << "AV_PIX_FMT_NV12 frame." << std::endl;
    }
    else if (frame->format == AV_PIX_FMT_BGRA)
    {
        static std::ofstream video("out.bgra", std::ios::binary | std::ios::trunc);
        video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
    }
    else if (frame->format == AV_PIX_FMT_YUV420P)
    {
        static bool bret = false;
        static std::ofstream video("out.rgb", std::ios::binary | std::ios::trunc);
        static int i = 0;
        if (i++ == 0)
        {
            bret = g_sws.set_src_opt(static_cast<AVPixelFormat>(frame->format), frame->width, frame->height, err);
            TESTCHECKRET2(bret);
            bret = g_sws.set_dst_opt(AV_PIX_FMT_RGB24, 320, 240, err);
            TESTCHECKRET2(bret);
            bret = g_sws.lock_opt(err);
            TESTCHECKRET2(bret);
        }

        /*
        video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
        video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
        video.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
        */

        // 将输出翻转
        g_pointers[0] += g_linesizes[0] * (240 - 1);
        g_linesizes[0] *= -1;
        // 转换
        int ret = g_sws.scale(frame->data, frame->linesize, 0, frame->height, g_pointers, g_linesizes, err);
        // 还原指针，以便拷贝数据
        g_linesizes[0] *= -1;
        g_pointers[0] -= g_linesizes[0] * (240 - 1);
        video.write(reinterpret_cast<const char*>(g_pointers[0]), g_linesizes[0] * ret);
    }
    else if (frame->format == AV_SAMPLE_FMT_S16)
    {
        static std::ofstream audio("outs16.pcm", std::ios::binary | std::ios::trunc);
        // 非平面格式，就直接拷贝data[0]
        audio.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0]);
    }
    else if (frame->format == AV_SAMPLE_FMT_FLTP)
    {
        static bool bret = false;
        static std::ofstream audio("out.pcm", std::ios::binary | std::ios::trunc);
        static int i = 0;
        if (i++ == 0)
        {
            bret = g_swr.set_src_opt(frame->channel_layout, frame->sample_rate, static_cast<AVSampleFormat>(frame->format), err);
            TESTCHECKRET2(bret);
            bret = g_swr.set_dst_opt(g_layout, g_rate, g_fmt, err);
            TESTCHECKRET2(bret);
            bret = g_swr.lock_opt(err);
            TESTCHECKRET2(bret);
        }

        // 返回每个通道(channel)的样本数(samples)
        int ret = g_swr.convert(g_data, g_linesize, (const uint8_t * *)frame->data, frame->nb_samples, err);
        // 获取样本格式对应的每个样本大小(Byte)
        auto size = av_get_bytes_per_sample(g_fmt);
        // 拷贝音频数据
        for (int i = 0; i < ret; ++i) // 每个样本
        {
            for (int j = 0; j < av_get_channel_layout_nb_channels(g_layout)/* 获取布局对应的通道数 */; ++j) // 每个通道
            {
                audio.write(reinterpret_cast<const char*>(g_data[j] + size * i), size);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    std::string err;
    bool ret = false;

    // 分配图像数据内存
    int size = av_image_alloc(g_pointers, g_linesizes, 320, 240, AV_PIX_FMT_RGB24, 1);

    // 分配音频数据内存
    size = av_samples_alloc_array_and_samples(&g_data, &g_linesize, av_get_channel_layout_nb_channels(g_layout), g_rate, g_fmt, 0);
    
    //demux.device_register_all(err);
    //TESTCHECKRET(ret);
    //ret = demux.set_input_format("gdigrab", err); //采集摄像头
    //ret = demux.set_input_format("dshow", err); //采集声卡
    //TESTCHECKRET(ret);
    //ret = demux.set_dic_opt("framerate", "15", err); //"audio=virtual-audio-capturer"这个不设置帧率得到的音频有问题
    //TESTCHECKRET(ret);
    ret = demux.set_demux_callback(DemuxPacketCB, nullptr, err);
    TESTCHECKRET(ret);
    ret = demux.set_demux_status_callback(DecStatusCB, nullptr, err);
    TESTCHECKRET(ret);

    //ret = demux.set_input("desktop", err);
    //ret = demux.set_input("audio=virtual-audio-capturer", err);
    ret = demux.set_input("in.flv", err);
    //ret = demux.set_input("rtmp://localhost/live/test", err);
    TESTCHECKRET(ret);
    ret = decode.set_dec_callback(DecFrameCB, &decode, err);
    TESTCHECKRET(ret);
    
    //ret = decode.set_hwdec_type(AV_HWDEVICE_TYPE_DXVA2, true, err);
    //TESTCHECKRET(ret);

    ret = decode2.set_dec_callback(DecFrameCB, &decode2, err);
    TESTCHECKRET(ret);

    int i = 0;
    while (i++ < 5)
    {
        ret = demux.openinput(err);
        TESTCHECKRET(ret);
        vindex = demux.get_steam_index(AVMEDIA_TYPE_VIDEO, err);
        if (vindex != -1)
        {
            ret = decode.copy_param(demux.get_steam_par(vindex, err), err);
            TESTCHECKRET(ret);
            ret = decode.codec_open(err);
            TESTCHECKRET(ret);
        }
        aindex = demux.get_steam_index(AVMEDIA_TYPE_AUDIO, err);
        if (aindex != -1)
        {
            ret = decode2.copy_param(demux.get_steam_par(aindex, err), err);
            TESTCHECKRET(ret);
            ret = decode.codec_open(err);
            TESTCHECKRET(ret);
        }
        ret = demux.begindemux(err);
        TESTCHECKRET(ret);

        std::cout << "input to stop decoding." << std::endl;
        getchar();

        ret = demux.stopdemux(err);
        TESTCHECKRET(ret);
    }

    ret = g_sws.unlock_opt(err);
    TESTCHECKRET(ret);
    // 清理图像数据内存
    if (g_pointers)
    {
        av_freep(&g_pointers[0]);
    }
    av_freep(&g_pointers);

    ret = g_swr.unlock_opt(err);
    TESTCHECKRET(ret);
    // 清理音频数据内存
    if (g_data)
    {
        av_freep(&g_data[0]);
    }
    av_freep(&g_data);

    return 0;
}