#include <iostream>
#include <fstream>

//#define NOVIDEO     //不解码视频
//#define NOSAVEYUV   //不保存YUV
#define SWSCALE     //视频帧转换,需禁用NOVIDEO
//#define NOAUDIO     //不解码音频
//#define NOSAVEPCM   //不保存PCM
#define RESAMPLE    //音频重采样,需禁用NOAUDIO    
//#define AVIO        //使用AVIO
#define ENCODE      //编码,需禁用NOVIDEO或者NOAUDIO
#define REMUX       //转封装
#define MUXING      //封装,需打开ENCODE

#ifdef __cplusplus

extern "C"
{

#endif

// FFmpeg 头文件
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h> // sws_cale
#include <libswresample/swresample.h> // swr_alloc_set_opts
#include <libavutil/file.h> // av_file_map
#include <libavutil/imgutils.h> // av_image_alloc
#include <libavutil/opt.h> // av_opt_set

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
    // DECODE
    AVFormatContext* fmt_ctx = nullptr;
    AVDictionaryEntry* dic = nullptr;
    AVCodecContext *vcodectx = nullptr, *acodectx = nullptr;
    AVCodecParameters *vcodecpar = nullptr, *acodecpar = nullptr;
    AVCodec *vcodec = nullptr, *acodec = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;
    uint8_t* pt[4] = { 0 };
    int lz[4] = { 0 };
    int s = 0;
    std::ofstream out_yuv, out_pcm, out_bgr, out_pcm2, out_h264, out_mp3;
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
    // resample
    SwrContext* swrctx = nullptr;
    int samplessize = 0;
    uint8_t * sambuf = nullptr;
    // ENCODE
    AVCodecContext *ovcodectx = nullptr, *oacodectx = nullptr;
    AVCodec *ovcodec = nullptr, *oacodec = nullptr;
    AVDictionary* param = nullptr;
    AVPacket* opkt = nullptr;
    // REMUX
    AVFormatContext* ofmt_ctx = nullptr;
    AVStream *ovstream = nullptr, *oastream = nullptr, *streamtmp = nullptr;
    // MUXING
    AVFormatContext* ofmt_ctx2 = nullptr;
    AVStream *ovstream2 = nullptr, *oastream2 = nullptr;

    out_yuv.open("out.yuv", std::ios::binary | std::ios::trunc);
    out_pcm.open("out.pcm", std::ios::binary | std::ios::trunc);
    out_bgr.open("out.bgr", std::ios::binary | std::ios::trunc);
    out_pcm2.open("out2.pcm", std::ios::binary | std::ios::trunc);
    out_h264.open("out.h264", std::ios::binary | std::ios::trunc);
    out_mp3.open("out.mp3", std::ios::binary | std::ios::trunc);
    if (!out_yuv.is_open() || !out_pcm.is_open() || !out_bgr.is_open() || !out_pcm2.is_open() || !out_h264.is_open() || !out_mp3.is_open())
    {
        std::cerr << "创建/打开输出文件失败" << std::endl;
        goto END;
    }

    // 日志
    av_log_set_level(AV_LOG_ERROR);

    // 打开输入
#ifdef AVIO
    // 内存映射
    ret = av_file_map("in.mkv", &buf, &size, 0, nullptr);
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
            vindex = vindex==-1 ? i : vindex;
        }
        else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            aindex = aindex == -1 ? i : aindex;
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
    if (vindex != -1)
    {
        vcodecpar = fmt_ctx->streams[vindex]->codecpar;
        vcodec = avcodec_find_decoder(vcodecpar->codec_id);
        if (vcodec == nullptr)
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
    }
    if (aindex != -1)
    {
        acodecpar = fmt_ctx->streams[aindex]->codecpar;
        acodec = avcodec_find_decoder(acodecpar->codec_id);
        if (acodec == nullptr)
        {
            std::cerr << "avcodec_find_decoder err" << std::endl;
            goto END;
        }
        //打开解码器
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
#endif // SWSCALE

#ifdef RESAMPLE
    // 创建转换上下文
    swrctx = swr_alloc_set_opts(NULL, av_get_default_channel_layout(acodectx->channels), AV_SAMPLE_FMT_S16, 
                acodectx->sample_rate, av_get_default_channel_layout(acodectx->channels), acodectx->sample_fmt, 
                acodectx->sample_rate, 0, NULL);
    if (swrctx == nullptr)
    {
        std::cerr << "swr_alloc_set_opts" << std::endl;
        goto END;
    }
    // 初始化转换上下文
    ret = swr_init(swrctx);
    if (ret < 0)
    {
        std::cerr << "swr_init err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    //计算1s的数据大小，使缓冲区足够大
    samplessize = av_samples_get_buffer_size(nullptr, acodectx->channels, acodectx->sample_rate, AV_SAMPLE_FMT_S16, 1); 
    if (samplessize < 0)
    {
        std::cerr << "av_samples_get_buffer_size err : " << av_err2str(samplessize) << std::endl;
        goto END;
    }
    sambuf = static_cast<uint8_t*>(av_mallocz(samplessize));
    if (sambuf == nullptr)
    {
        std::cerr << "av_mallocz err" << std::endl;
        goto END;
    }
#endif // RESAMPLE

#ifdef ENCODE
    //---ENCODEVIDEO
    // 查找编码器
    ovcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (ovcodec == nullptr)
    {
        std::cerr << "avcodec_find_encoder AV_CODEC_ID_H264 err" << std::endl;
        goto END;
    }
    ovcodectx = avcodec_alloc_context3(ovcodec);
    if (ovcodectx == nullptr)
    {
        std::cerr << "avcodec_alloc_context3 err" << std::endl;
        goto END;
    }
    // 设置参数
    ovcodectx->bit_rate = vcodectx->bit_rate==0 ? 850000 : vcodectx->bit_rate;
    ovcodectx->width = vcodectx->width;
    ovcodectx->height = vcodectx->height;
    ovcodectx->time_base = { 1, 25 };
    ovcodectx->framerate = vcodectx->framerate;
    ovcodectx->gop_size = vcodectx->gop_size;
    ovcodectx->max_b_frames = vcodectx->max_b_frames;
    ovcodectx->pix_fmt = vcodectx->pix_fmt;
    // --preset的参数主要调节编码速度和质量的平衡，有ultrafast、superfast、veryfast、faster、fast、medium、slow、slower、veryslow、placebo这10个选项，从快到慢。
    ret = av_dict_set(&param, "preset", "medium", 0);
    if (ret < 0)
    {
        std::cerr << "av_opt_set err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    ret = av_dict_set(&param, "tune", "zerolatency", 0);  //实现实时编码，有效降低输出大小
    if (ret < 0)
    {
        std::cerr << "av_opt_set err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    //ret = av_dict_set(&param, "profile", "main", 0);
    //if (ret < 0)
    //{
    //    std::cerr << "av_opt_set err ： " << av_err2str(ret) << std::endl;
    //    goto END;
    //}
    ret = avcodec_open2(ovcodectx, ovcodec, &param);
    if (ret < 0)
    {
        std::cerr << "avcodec_open2 err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    //ENCODEVIDEO---

    //---ENCODEAUDIO
    // 查找编码器
    oacodec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (oacodec == nullptr)
    {
        std::cerr << "avcodec_find_encoder AV_CODEC_ID_MP3 err" << std::endl;
        goto END;
    }
    oacodectx = avcodec_alloc_context3(oacodec);
    if (oacodectx == nullptr)
    {
        std::cerr << "avcodec_alloc_context3 err" << std::endl;
        goto END;
    }
    // 设置参数
    oacodectx->bit_rate = acodectx->bit_rate;
    oacodectx->sample_fmt = acodectx->sample_fmt;
    oacodectx->sample_rate = acodectx->sample_rate;
    oacodectx->channel_layout = acodectx->channel_layout;
    oacodectx->channels = acodectx->channels;
    ret = avcodec_open2(oacodectx, oacodec, nullptr);
    if (ret < 0)
    {
        std::cerr << "avcodec_open2 err ： " << av_err2str(ret) << std::endl;
        goto END;
    }
    //ENCODEAUDIO---

    opkt = av_packet_alloc();
    if (opkt == nullptr)
    {
        std::cerr << "av_packet_alloc err" << std::endl;
        goto END;
    }
    av_init_packet(opkt);
#endif // ENCODE

#ifdef REMUX
    // 创建输出
    ret = avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, "out.mp4");
    if (ret < 0)
    {
        std::cerr << "avformat_alloc_output_context2 err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    //创建流
    ovstream = avformat_new_stream(ofmt_ctx, nullptr);
    oastream = avformat_new_stream(ofmt_ctx, nullptr);
    if (ovstream == nullptr || oastream == nullptr)
    {
        std::cerr << "avformat_new_stream err" << std::endl;
        goto END;
    }
    //复制配置信息
    ret = avcodec_parameters_from_context(ovstream->codecpar, vcodectx);
    if (ret < 0)
    {
        std::cerr << "avcodec_parameters_from_context err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    ret = avcodec_parameters_from_context(oastream->codecpar, acodectx);
    if (ret < 0)
    {
        std::cerr << "avcodec_parameters_from_context err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    av_dump_format(ofmt_ctx, 0, ofmt_ctx->url, 1);
    // 标记不需要重新编解码
    ovstream->codecpar->codec_tag = 0;
    oastream->codecpar->codec_tag = 0;
    // 打开io
    if (!(ofmt_ctx->flags & AVFMT_NOFILE)) 
    {
        ret = avio_open(&ofmt_ctx->pb, "out.mp4", AVIO_FLAG_WRITE);
        if (ret < 0) 
        {
            std::cerr << "avio_open err : " << av_err2str(ret) << std::endl;
            goto END;
        }
    }
    // 写文件头
    ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0)
    {
        std::cerr << "avformat_write_header err : " << av_err2str(ret) << std::endl;
        goto END;
    }
#endif // REMUX

#ifdef MUXING
    // 创建输出
    ret = avformat_alloc_output_context2(&ofmt_ctx2, nullptr, nullptr, "out2.mp4");
    if (ret < 0)
    {
        std::cerr << "avformat_alloc_output_context2 err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    //创建流
    ovstream2 = avformat_new_stream(ofmt_ctx2, nullptr);
    oastream2 = avformat_new_stream(ofmt_ctx2, nullptr);
    if (ovstream2 == nullptr || oastream2 == nullptr)
    {
        std::cerr << "avformat_new_stream err" << std::endl;
        goto END;
    }
    //复制配置信息
    ret = avcodec_parameters_from_context(ovstream2->codecpar, ovcodectx);
    if (ret < 0)
    {
        std::cerr << "avcodec_parameters_from_context err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    ret = avcodec_parameters_from_context(oastream2->codecpar, oacodectx);
    if (ret < 0)
    {
        std::cerr << "avcodec_parameters_from_context err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    av_dump_format(ofmt_ctx2, 0, ofmt_ctx2->url, 1);
    // 标记不需要重新编解码
    ovstream2->codecpar->codec_tag = 0;
    oastream2->codecpar->codec_tag = 0;
    // 打开io
    if (!(ofmt_ctx2->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx2->pb, "out2.mp4", AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            std::cerr << "avio_open err : " << av_err2str(ret) << std::endl;
            goto END;
        }
    }
    // 写文件头
    ret = avformat_write_header(ofmt_ctx2, nullptr);
    if (ret < 0)
    {
        std::cerr << "avformat_write_header err : " << av_err2str(ret) << std::endl;
        goto END;
    }
#endif // MUXING

    // 申请保存解码帧的内存
    ret = av_image_alloc(pt, lz, vcodectx->width, vcodectx->height, vcodectx->pix_fmt, 1);
    if (ret < 0)
    {
        std::cerr << "av_image_alloc err : " << av_err2str(ret) << std::endl;
        goto END;
    }
    // 记录内存大小
    s = ret; 

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
#ifndef NOSAVEYUV
                        //out_yuv.write(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0] * frame->height);
                        //out_yuv.write(reinterpret_cast<const char*>(frame->data[1]), frame->linesize[1] * frame->height / 2);
                        //out_yuv.write(reinterpret_cast<const char*>(frame->data[2]), frame->linesize[2] * frame->height / 2);
                        // 这种方式可以自动去除画面右边多余数据
                        av_image_copy(pt, lz, 
                            (const uint8_t* *)frame->data, frame->linesize, 
                            static_cast<AVPixelFormat>(vcodectx->pix_fmt), frame->width, frame->height);
                        out_yuv.write(reinterpret_cast<const char*>(pt[0]), s);
#endif // NOSAVEYUV
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

#endif // SWSCALE
#ifdef ENCODE
                        ret = avcodec_send_frame(ovcodectx, frame);
                        if (ret < 0)
                        {
                            std::cerr << "avcodec_send_frame err ： " << av_err2str(ret) << std::endl;
                            break;
                        }
                        while (ret >= 0)
                        {
                            ret = avcodec_receive_packet(ovcodectx, opkt);
                            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            {
                                break;
                            }
                            else if (ret < 0)
                            {
                                std::cerr << "avcodec_receive_packet err ： " << av_err2str(ret) << std::endl;
                                break;
                            }
                            else
                            {
                                // 得到编码数据
                                out_h264.write(reinterpret_cast<const char*>(opkt->data), opkt->size);
#ifdef MUXING
                                opkt->pts = av_rescale_q_rnd(opkt->pts, fmt_ctx->streams[vindex]->time_base, ovstream2->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                                opkt->dts = av_rescale_q_rnd(opkt->dts, fmt_ctx->streams[vindex]->time_base, ovstream2->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                                opkt->duration = av_rescale_q(opkt->duration, fmt_ctx->streams[vindex]->time_base, ovstream2->time_base);
                                opkt->pos = -1;
                                opkt->stream_index = 0;
                                ret = av_interleaved_write_frame(ofmt_ctx2, opkt);
                                if (ret < 0)
                                {
                                    std::cerr << "av_interleaved_write_frame err ： " << av_err2str(ret) << std::endl;
                                }
#endif // MUXING
                                av_packet_unref(opkt);
                            }
                        }
#endif // ENCODE
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
#ifndef NOSAVEPCM
                        auto size = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));
                        for (int i = 0; i < frame->nb_samples; ++i)
                        {
                            for (int j = 0; j < frame->channels; ++j)
                            {
                                out_pcm.write(reinterpret_cast<const char*>(frame->data[j] + size * i), size);
                            }
                        }

#ifdef RESAMPLE
                        //转换，返回每个通道的样本数 
                        ret = swr_convert(swrctx, &sambuf, samplessize, (const uint8_t **)frame->data, frame->nb_samples);//转换，返回每个通道的样本数 
                        if (ret < 0)
                        {
                            std::cerr << "swr_convert err ： " << av_err2str(ret) << std::endl;
                            break;
                        }
                        out_pcm2.write(reinterpret_cast<const char*>(sambuf), 
                            av_samples_get_buffer_size(nullptr, frame->channels, ret, AV_SAMPLE_FMT_S16, 1));
#endif // RESAMPLE

#endif // NOSAVEPCM
#ifdef ENCODE
                        ret = avcodec_send_frame(oacodectx, frame);
                        if (ret < 0)
                        {
                            std::cerr << "avcodec_send_frame err ： " << av_err2str(ret) << std::endl;
                            break;
                        }
                        while (ret >= 0)
                        {
                            ret = avcodec_receive_packet(oacodectx, opkt);
                            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            {
                                break;
                            }
                            else if (ret < 0)
                            {
                                std::cerr << "avcodec_receive_packet err ： " << av_err2str(ret) << std::endl;
                                break;
                            }
                            else
                            {
                                // 得到编码数据
                                out_mp3.write(reinterpret_cast<const char*>(opkt->data), opkt->size);
#ifdef MUXING
                                opkt->pts = av_rescale_q_rnd(opkt->pts, fmt_ctx->streams[aindex]->time_base, oastream2->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                                opkt->dts = av_rescale_q_rnd(opkt->dts, fmt_ctx->streams[aindex]->time_base, oastream2->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                                opkt->duration = av_rescale_q(opkt->duration, fmt_ctx->streams[aindex]->time_base, oastream2->time_base);
                                opkt->pos = -1;
                                opkt->stream_index = 1;
                                ret = av_interleaved_write_frame(ofmt_ctx2, opkt);
                                if (ret < 0)
                                {
                                    std::cerr << "av_interleaved_write_frame err ： " << av_err2str(ret) << std::endl;
                                }
#endif // MUXING
                                av_packet_unref(opkt);
                            }
                        }
#endif // ENCODE
                    }
                }
            }
#endif // NOAUDIO
        }

#ifdef REMUX
        streamtmp = nullptr;
        if (pkt->stream_index == vindex)
        {
            streamtmp = ovstream;
            pkt->stream_index = 0;
        }
        else if (pkt->stream_index == aindex)
        {
            streamtmp = oastream;
            pkt->stream_index = 1;
        }

        if (streamtmp != nullptr)
        {
            pkt->pts = av_rescale_q_rnd(pkt->pts, fmt_ctx->streams[pkt->stream_index]->time_base, streamtmp->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt->dts = av_rescale_q_rnd(pkt->dts, fmt_ctx->streams[pkt->stream_index]->time_base, streamtmp->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt->duration = av_rescale_q(pkt->duration, fmt_ctx->streams[pkt->stream_index]->time_base, streamtmp->time_base);
            pkt->pos = -1;
            ret = av_interleaved_write_frame(ofmt_ctx, pkt);
            if (ret < 0)
            {
                std::cerr << "REMUX av_interleaved_write_frame err ： " << av_err2str(ret) << std::endl;
            }
        }
#endif // REMUX

        // 复位data和size
        av_packet_unref(pkt);
    }

END:
    std::cerr << "end..." << std::endl;
    std::cin.get();

#ifdef REMUX
    if (ofmt_ctx != nullptr)
    {
        // 写文件尾
        ret = av_write_trailer(ofmt_ctx);
        if (ret < 0)
        {
            std::cerr << "av_write_trailer err ： " << av_err2str(ret) << std::endl;
        }
    }
#endif // REMUX

#ifdef MUXING
    if (ofmt_ctx2 != nullptr)
    {
        // 写文件尾
        ret = av_write_trailer(ofmt_ctx2);
        if (ret < 0)
        {
            std::cerr << "av_write_trailer err ： " << av_err2str(ret) << std::endl;
        }
    }
#endif // MUXING

    // 关闭文件
    out_yuv.close();
    out_pcm.close();
    out_bgr.close();
    out_pcm2.close();
    out_h264.close();
    out_mp3.close();

    // 释放资源
    av_freep(&pointers[0]);
    av_freep(&pt[0]);
    sws_freeContext(swsctx);
    if (swrctx != nullptr)
    {
        swr_free(&swrctx);
        swrctx = nullptr;
    }
    av_free(sambuf);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&vcodectx);
    avcodec_free_context(&acodectx);
    avformat_close_input(&fmt_ctx);

    av_packet_free(&opkt);
    avcodec_free_context(&ovcodectx);
    avcodec_free_context(&oacodectx);

    // 关闭io
    if (ofmt_ctx != nullptr && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt_ctx->pb);
    }   
    avformat_free_context(ofmt_ctx);
    ofmt_ctx = nullptr;
    if (ofmt_ctx2 != nullptr && !(ofmt_ctx2->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt_ctx2->pb);
    }
    avformat_free_context(ofmt_ctx2);
    ofmt_ctx2 = nullptr;

    // 内部缓冲区可能已经改变，并且是不等于之前的aviobuf
    if (avioctx != nullptr)
    {
        av_freep(&avioctx->buffer);
        av_freep(&avioctx);
    }
    av_file_unmap(buf, size);
    return 0;
}