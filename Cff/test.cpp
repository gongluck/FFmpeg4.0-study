#include "CDecode.h"
#include "CSws.h"
#include <iostream>
#include <fstream>

CSws g_sws;
uint8_t* g_pointers[4] = { 0 };
int g_linesizes[4] = { 0 };

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
            g_pointers[0] += g_linesizes[0] * (240 - 1);
            g_linesizes[0] *= -1;
            // 转换
            g_sws.scale(frame->data, frame->linesize, 0, frame->height, g_pointers, g_linesizes, err);
            // 还原指针，以便拷贝数据
            g_linesizes[0] *= -1;
            g_pointers[0] -= g_linesizes[0] * (240 - 1);
            video.write(reinterpret_cast<const char*>(g_pointers[0]), g_linesizes[0] * 240);
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
    int size = av_image_alloc(g_pointers, g_linesizes, 320, 240, AV_PIX_FMT_RGB24, 1);

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
    av_freep(&g_pointers[0]);

    return 0;
}