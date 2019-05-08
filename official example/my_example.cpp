#include <iostream>
#include <fstream>

//#define NOVIDEO
#define SWSCALE
//#define NOAUDIO
//#define AVIO

#ifdef __cplusplus

extern "C"
{

#endif

// FFmpeg 头文件
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libavutil/file.h> // av_file_map
#include <libavutil/imgutils.h> // av_image_alloc


#ifdef __cplusplus

}
// C++中使用av_err2str宏
char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str(errnum) \
    av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)

#endif

// 自定义参数，传递内存buf和大小
typedef struct __BUFER_DATA__
{
    uint8_t* buf;
    size_t size;
}Bufdata;

// 自定义文件读操作
int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    Bufdata* bd = static_cast<Bufdata*>(opaque);
    buf_size = FFMIN(buf_size, bd->size);
    if (buf_size == 0)
    {
        return AVERROR_EOF;
    }

    memcpy(buf, bd->buf, buf_size);
    bd->buf += buf_size;
    bd->size -= buf_size;

    return buf_size;
}

int main(int argc, char* argv[])
{
    AVFormatContext* fmt_ctx = nullptr;
    AVDictionaryEntry* dic = nullptr;
    AVCodecContext *vcodectx = nullptr, *acodectx = nullptr;
    AVCodecParameters *vcodecpar = nullptr, *acodecpar = nullptr;
    AVCodec *vcodec = nullptr, *acodec = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;
    std::ofstream out_yuv, out_pcm, out_bgr;
    const char* in = "in.flv";
    int vindex = -1, aindex = -1;
    int ret = 0;
    // avio
    uint8_t *buf = nullptr, *aviobuf = nullptr;
    size_t size = 0;
    Bufdata bd = { 0 };
    AVIOContext* avioctx = nullptr;
    // swscale
    SwsContext* swsctx = nullptr;
    uint8_t* pointers[4] = { 0 };
    int linesizes[4] = { 0 };

    out_yuv.open("out.yuv", std::ios::binary | std::ios::trunc);
    out_pcm.open("out.pcm", std::ios::binary | std::ios::trunc);
    out_bgr.open("out.bgr", std::ios::binary | std::ios::trunc);
    if (!out_yuv.is_open() || !out_pcm.is_open() || !out_bgr.is_open())
    {
        std::cerr << "创建/打开输出文件失败" << std::endl;
        goto END;
    }

    // 日志
    av_log_set_level(AV_LOG_INFO);

    // 打开输入
#ifdef AVIO
    // 内存映射
    ret = av_file_map("in.flv", &buf, &size, 0, 0);
    if (ret < 0)
    {
        std::cerr << "av_file_map err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    fmt_ctx = avformat_alloc_context();
    if (fmt_ctx == nullptr)
    {
        std::cerr << "avformat_alloc_context err" << std::endl;
        goto END;
    }
    aviobuf = static_cast<uint8_t*>(av_malloc(1024));
    if (aviobuf == nullptr)
    {
        std::cerr << "av_malloc err" << std::endl;
        goto END;
    }
    bd.buf = buf;
    bd.size = size;
    avioctx = avio_alloc_context(aviobuf, 1024, 0, &bd, read_packet, nullptr, nullptr);    if (avioctx == nullptr)    {        std::cerr << "avio_alloc_context err" << std::endl;
        goto END;    }    fmt_ctx->pb = avioctx;
    ret = avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr);
    if (ret < 0)
    {
        std::cerr << "avformat_open_input err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
#else
    ret = avformat_open_input(&fmt_ctx, in, nullptr, nullptr);
    if (ret < 0)
    {
        std::cerr << "avformat_open_input err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
#endif // AVIO

    std::cerr << "get metadata : " << std::endl;
    while ((dic = av_dict_get(fmt_ctx->metadata, "", dic, AV_DICT_IGNORE_SUFFIX)) != nullptr)
    {
        std::cerr << dic->key << " : " << dic->value << std::endl;
    }

    // 查找流信息，对输入进行预处理
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0)
    {
        std::cerr << "avformat_find_stream_info err ： " << av_err2str(ret) << std::endl;
        goto END;
    }

    // 打印输入信息
    av_dump_format(fmt_ctx, 0, fmt_ctx->url, 0);

    //查找流
    for (int i = 0; i < fmt_ctx->nb_streams; ++i)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            vindex = i;
        }   
        else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            aindex = i;
        }    
    }
    if (vindex == -1)
    {
        std::cerr << "找不到视频流" << std::endl;
    }
    if (aindex == -1)
    {
        std::cerr << "找不到音频流" << std::endl;
    }

    //查找解码器    
    vcodecpar = fmt_ctx->streams[vindex]->codecpar;
    vcodec = avcodec_find_decoder(vcodecpar->codec_id);
    if (vcodec == nullptr)
    {
        std::cerr << "avcodec_find_decoder err"  << std::endl;
        goto END;
    }
    acodecpar = fmt_ctx->streams[aindex]->codecpar;
    acodec = avcodec_find_decoder(acodecpar->codec_id);
    if (acodec == nullptr)
    {
        std::cerr << "avcodec_find_decoder err" << std::endl;
        goto END;
    }

    //打开解码器
    vcodectx = avcodec_alloc_context3(vcodec);
    ret = avcodec_parameters_to_context(vcodectx, vcodecpar);// 参数拷贝
    if (ret < 0)
    {
        std::cerr << "avcodec_parameters_to_context err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    ret = avcodec_open2(vcodectx, vcodec, nullptr);
    if (ret < 0)
    {
        std::cerr << "avcodec_open2 err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    acodectx = avcodec_alloc_context3(acodec);
    ret = avcodec_parameters_to_context(acodectx, acodecpar);// 参数拷贝
    if (ret < 0)
    {
        std::cerr << "avcodec_parameters_to_context err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    ret = avcodec_open2(acodectx, acodec, nullptr);
    if (ret < 0)
    {
        std::cerr << "avcodec_open2 err ： " << av_err2str(ret) << std::endl;
        goto END;
    }

    // 创建AVPacket
    pkt = av_packet_alloc();
    if (pkt == nullptr)
    {
        std::cerr << "av_packet_alloc err" << std::endl;
        goto END;
    }
    av_init_packet(pkt);

    // 创建AVFrame
    frame = av_frame_alloc();
    if (frame == nullptr) 
    {
        std::cerr << "av_frame_alloc err" << std::endl;
        goto END;
    }

#ifdef SWSCALE
    // 创建转换上下文
    swsctx = sws_getContext(vcodectx->width, vcodectx->height, AV_PIX_FMT_YUV420P, 320, 240, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (swsctx == nullptr)
    {
        std::cerr << "sws_getContext err" << std::endl;
        goto END;
    }
    // 分配内存空间
    // ffmpe里很多汇编优化，它一次读取或写入的数据可能比你想象中的要多（某些对齐要求），所以ffmpeg操作的内存区域，一般都应该用av_malloc分配，这个函数通常分配的内存会比你要求的多，就是为了应付这些场景
    ret = av_image_alloc(pointers, linesizes, 320, 240, AV_PIX_FMT_RGB24, 16);
    if (ret < 0)
    {
        std::cerr << "av_image_alloc err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
#endif

    // 从输入读取数据
    while (av_read_frame(fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == vindex)
        {
#ifndef NOVIDEO
            // 解码视频帧
            ret = avcodec_send_packet(vcodectx, pkt);
            if (ret < 0)
            {
                std::cerr << "avcodec_send_packet err ： " << av_err2str(ret) << std::endl;
                break;
            }
            while (ret >= 0) 
            {
                ret = avcodec_receive_frame(vcodectx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0) 
                {
                    std::cerr << "avcodec_receive_frame err ： " << av_err2str(ret) << std::endl;
                    break;
                }
                else
                {
                    // 得到解码数据
                    if (frame->format == AV_PIX_FMT_YUV420P)
                    {
                        out_yuv.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
                        out_yuv.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
                        out_yuv.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
#ifdef SWSCALE
                        // 视频帧格式转换
                        ret = sws_scale(swsctx, frame->data, frame->linesize, 0, frame->height, pointers, linesizes);
                        if (ret <= 0)
                        {
                            std::cerr << "sws_scale err ： " << av_err2str(ret) << std::endl;
                            break;
                        }
                        // 翻转
                        pointers[0] += linesizes[0] * (ret - 1);
                        linesizes[0] *= -1;
                        out_bgr.write(reinterpret_cast<const char*>(pointers[0]), linesizes[0] * ret);
                        
#endif
                    }
                }
            }
#endif // NOVIDEO
        }
        else if (pkt->stream_index == aindex)
        {
#ifndef NOAUDIO
            // 解码音频帧
            ret = avcodec_send_packet(acodectx, pkt);
            if (ret < 0)
            {
                std::cerr << "avcodec_send_packet err ： " << av_err2str(ret) << std::endl;
                break;
            }
            while (ret >= 0)
            {
                ret = avcodec_receive_frame(acodectx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    std::cerr << "avcodec_receive_frame err ： " << av_err2str(ret) << std::endl;
                    break;
                }
                else
                {   
                    // 得到解码数据
                    if (frame->format == AV_SAMPLE_FMT_FLTP)
                    { 
                        /// 参考了https://www.cnblogs.com/my_life/articles/6841859.html
                        // 计算一个planar的有效大小，很关键！
                        auto size = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format)) * frame->nb_samples;
                        for (int i = 0; i < size; i += 4)
                        {
                            for (int j = 0; j < frame->channels; ++j)
                            {
                                out_pcm.write(reinterpret_cast<const char*>(frame->data[j] + i), 4);
                            }
                        }
                    }
                }
            }
#endif // NOAUDIO
        }

        // 复位data和size
        av_packet_unref(pkt);
    }

END:
    std::cerr << "end..." << std::endl;
    std::cin.get();

    // 关闭文件
    out_yuv.close();
    out_pcm.close();
    out_bgr.close();

    // 释放资源
    av_freep(&pointers[0]);
    sws_freeContext(swsctx);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&vcodectx);
    avcodec_free_context(&acodectx);
    avformat_close_input(&fmt_ctx);

    // 内部缓冲区可能已经改变，并且是不等于之前的aviobuf
    if (avioctx != nullptr) 
    {
        av_freep(&avioctx->buffer);
        av_freep(&avioctx);
    }
    av_file_unmap(buf, size);
    return 0;
}