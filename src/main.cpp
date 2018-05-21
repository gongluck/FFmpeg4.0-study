#include <iostream>
using namespace std;

extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/time.h"
}
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")

char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str(errnum) \
    av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)

#define INFILE	"in.flv"
#define RTMP	"rtmp://192.168.140.128/live/test"
#define RTSP	"rtsp://184.72.239.149/vod/mp4://BigBuckBunny_175k.mov"

int file2rtmp()
{
	int ret = 0;
	//封装上下文
	AVFormatContext* ictx = nullptr;
	AVFormatContext* octx = nullptr;
	const char* iurl = INFILE;
	const char* ourl = RTMP;
	int64_t starttime;

	ret = avformat_network_init();
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	//打开文件，解封文件头
	ret = avformat_open_input(&ictx, iurl, nullptr, nullptr);
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}
	cerr << "open file " << iurl << " success." << endl;

	//获取音视频流信息,h264 flv
	ret = avformat_find_stream_info(ictx, nullptr);
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	//打印媒体信息
	av_dump_format(ictx, 0, iurl, 0);

	//////////////////////////////

	//输出流
	ret = avformat_alloc_output_context2(&octx, av_guess_format(nullptr, INFILE, nullptr), nullptr, ourl);
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}
	cout << "octx create success." << endl;

	//配置输出流
	for (int i = 0; i < ictx->nb_streams; ++i)
	{
		//创建流
		AVStream* ostream = avformat_new_stream(octx, avcodec_find_encoder(ictx->streams[i]->codecpar->codec_id));
		if (ostream == nullptr)
			goto END;
		//复制配置信息
		ret = avcodec_parameters_copy(ostream->codecpar, ictx->streams[i]->codecpar);
		if (ret != 0)
		{
			cout << av_err2str(ret) << endl;
			goto END;
		}
		ostream->codecpar->codec_tag = 0;//标记不需要重新编解码
	}
	av_dump_format(octx, 0, ourl, 1);

	//////////////////////////////

	//推流
	ret = avio_open2(&octx->pb, ourl, AVIO_FLAG_WRITE, nullptr, nullptr);
	if (ret < 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	//写入头信息
	ret = avformat_write_header(octx, nullptr);
	if (ret < 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	//推流每一帧数据
	AVPacket pkt;
	starttime = av_gettime();
	while (av_read_frame(ictx, &pkt) == 0)
	{
		//计算转换pts dts
		AVRational itime = ictx->streams[pkt.stream_index]->time_base;
		AVRational otime = octx->streams[pkt.stream_index]->time_base;
		pkt.pts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q_rnd(pkt.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.pos = -1;

		if (ictx->streams[pkt.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			int64_t nowtime = av_gettime() - starttime;
			int64_t dts = pkt.dts * av_q2d(octx->streams[pkt.stream_index]->time_base) * 1000 * 1000;
			if(dts > nowtime)
				av_usleep(dts- nowtime);
		}
		
		ret = av_interleaved_write_frame(octx, &pkt);
		av_packet_unref(&pkt);
		if (ret < 0)
		{
			cout << av_err2str(ret) << endl;
			goto END;
		}
	}
	ret = av_write_trailer(octx);//写文件尾
	if (ret < 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

END:
	if (ictx != nullptr)
		avformat_close_input(&ictx);
	if (octx != nullptr)
	{
		avio_close(octx->pb);
		avformat_free_context(octx);
	}
	ret = avformat_network_deinit();
	if (ret != 0)
		cout << av_err2str(ret) << endl;
	return 0;
}

int rtsp2rtmp()
{
	int ret = 0;
	//封装上下文
	AVFormatContext* ictx = nullptr;
	AVFormatContext* octx = nullptr;
	const char* iurl = RTSP;
	const char* ourl = RTMP;
	int64_t starttime;

	//打开文件，解封文件头
	ret = avformat_open_input(&ictx, iurl, nullptr, nullptr);
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}
	cerr << "open file " << iurl << " success." << endl;

	//获取音视频流信息,h264 flv
	ret = avformat_find_stream_info(ictx, nullptr);
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	//打印媒体信息
	av_dump_format(ictx, 0, iurl, 0);

	//////////////////////////////

	//输出流
	ret = avformat_alloc_output_context2(&octx, nullptr, "flv", ourl);
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}
	cerr << "octx create success." << endl;

	//配置输出流
	for (int i = 0; i < ictx->nb_streams; ++i)
	{
		//创建流
		AVStream* ostream = avformat_new_stream(octx, avcodec_find_encoder(ictx->streams[i]->codecpar->codec_id));
		if (ostream == nullptr)
			return -1;
		//复制配置信息
		ret = avcodec_parameters_copy(ostream->codecpar, ictx->streams[i]->codecpar);
		if (ret != 0)
		{
			cout << av_err2str(ret) << endl;
			goto END;
		}
		ostream->codecpar->codec_tag = 0;//标记不需要重新编解码
	}
	av_dump_format(octx, 0, ourl, 1);

	//////////////////////////////

	//推流
	ret = avformat_network_init();
	if (ret != 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	ret = avio_open(&octx->pb, ourl, AVIO_FLAG_WRITE);
	if (ret < 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	//写入头信息
	ret = avformat_write_header(octx, nullptr);
	if (ret < 0)
	{
		cout << av_err2str(ret) << endl;
		goto END;
	}

	//推流每一帧数据
	AVPacket pkt;
	starttime = av_gettime();
	while (av_read_frame(ictx, &pkt) == 0)
	{
		//计算转换pts dts
		AVRational itime = ictx->streams[pkt.stream_index]->time_base;
		AVRational otime = octx->streams[pkt.stream_index]->time_base;
		pkt.pts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q_rnd(pkt.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.pos = -1;

		ret = av_interleaved_write_frame(octx, &pkt);
		av_packet_unref(&pkt);
		if (ret < 0)
			cout << av_err2str(ret) << endl;//不用退出
	}

END:
	return 0;
}

int main()
{
	while(true)
		file2rtmp();
	//rtsp2rtmp();
	system("pause");
	return 0;
}