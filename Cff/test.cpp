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


void DecStatusCB(CDecode::STATUS status, std::string err, void* param)
{
    std::cout << std::this_thread::get_id() << " got a status " << status << std::endl;
}

void DecFrameCB(const AVFrame* frame, CDecode::FRAMETYPE frametype, void* param)
{
    //std::cout << std::this_thread::get_id() << " got a frame." << frametype << std::endl;
    if (frametype == CDecode::FRAMETYPE::VIDEO)
    {
        if (frame->format == AV_PIX_FMT_YUV420P)
        {
            static std::ofstream video("out.rgb", std::ios::binary | std::ios::trunc);
            static int i = 0;
            if (++i > 9)
                return; 

            /*
            video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
            video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
            video.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
            */

            std::string err;
            // 将输出翻转
            /*g_pointers[0] += g_linesizes[0] * (240 - 1);
            g_linesizes[0] *= -1;*/
            // 转换
            int ret = g_sws.scale(frame->data, frame->linesize, 0, frame->height, g_pointers, g_linesizes, err);
            // 还原指针，以便拷贝数据
            /*g_linesizes[0] *= -1;
            g_pointers[0] -= g_linesizes[0] * (240 - 1);*/
            video.write(reinterpret_cast<const char*>(g_pointers[0]), g_linesizes[0] * ret);
        }
    }
    else if (frametype == CDecode::FRAMETYPE::AUDIO)
    {
        if (frame->format == AV_SAMPLE_FMT_FLTP)
        {
            static std::ofstream audio("out.pcm", std::ios::binary | std::ios::trunc);
            std::string err;
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
}

int main(int argc, char* argv[])
{
    std::string err;
    bool ret = false;

    ret = g_sws.set_src_opt(AV_PIX_FMT_YUV420P, 576, 432, err);
    ret = g_sws.set_dst_opt(AV_PIX_FMT_RGB24, 320, 240, err);
    ret = g_sws.lock_opt(err);
    // 分配图像数据内存
    int size = av_image_alloc(g_pointers, g_linesizes, 320, 240, AV_PIX_FMT_RGB24, 1);

    ret = g_swr.set_src_opt(AV_CH_LAYOUT_STEREO, 48000, AV_SAMPLE_FMT_FLTP, err);
    ret = g_swr.set_dst_opt(g_layout, g_rate, g_fmt, err);
    ret = g_swr.lock_opt(err);
    // 分配音频数据内存
    size = av_samples_alloc_array_and_samples(&g_data, &g_linesize, av_get_channel_layout_nb_channels(g_layout), g_rate, g_fmt, 0);

    CDecode decode;
    ret = decode.set_input("in.flv", err);
    ret = decode.set_dec_callback(DecFrameCB, nullptr, err);
    ret = decode.set_dec_status_callback(DecStatusCB, nullptr, err);

    int i = 0;
    while (i++ < 1)
    {
        ret = decode.begindecode(err);

        std::cout << "input to stop decoding." << std::endl;
        getchar();

        ret = decode.stopdecode(err);
    }

    ret = g_sws.unlock_opt(err);
    // 清理图像数据内存
    if (g_pointers)
    {
        av_freep(&g_pointers[0]);
    }
    av_freep(&g_pointers);

    ret = g_swr.unlock_opt(err);
    // 清理音频数据内存
    if (g_data)
    {
        av_freep(&g_data[0]);
    }
    av_freep(&g_data);

    return 0;
}