#include "CDecode.h"
#include <iostream>
#include <fstream>

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
            static std::ofstream video("out.yuv", std::ios::binary | std::ios::trunc);
            static int i = 0;
            if (++i > 10)
                return;
            video.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
            video.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
            video.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
        }
    }
}

int main(int argc, char* argv[])
{
    std::string err;
    bool ret = false;
    CDecode decode;
    ret = decode.set_input("in.flv", err);
    ret = decode.set_dec_callback(DecFrameCB, nullptr, err);
    ret = decode.set_dec_status_callback(DecStatusCB, nullptr, err);

    int i = 0;
    while (i++ < 10)
    {
        ret = decode.begindecode(err);

        std::cout << "input to stop decoding." << std::endl;
        getchar();

        ret = decode.stopdecode(err);
    }

    return 0;
}