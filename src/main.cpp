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

#define INFILE	"in.flv"
#define RTMP	"rtmp://192.168.140.128/live/test"

int CheckErr(int ret)
{
	char buf[1024] = { 0 };
	av_strerror(ret, buf, sizeof(buf));
	cerr << buf << endl;
	return ret;
}

int file2rtmp()
{
	int ret = 0;
	//封装上下文
	AVFormatContext* ictx = nullptr;
	AVFormatContext* octx = nullptr;
	const char* iurl = INFILE;
	const char* ourl = RTMP;

	//打开文件，解封文件头
	ret = avformat_open_input(&ictx, iurl, nullptr, nullptr);
	if (ret != 0)
		return CheckErr(ret);
	cerr << "open file " << iurl << "success." << endl;

	//获取音视频流信息,h264 flv
	ret = avformat_find_stream_info(ictx, nullptr);
	if (ret != 0)
		return CheckErr(ret);

	//打印媒体信息
	av_dump_format(ictx, 0, iurl, 0);

	//////////////////////////////

	//输出流
	ret = avformat_alloc_output_context2(&octx, nullptr, "flv", ourl);
	if (ret != 0)
		CheckErr(ret);
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
			return CheckErr(ret);
		ostream->codecpar->codec_tag = 0;//标记不需要重新编解码
	}
	av_dump_format(octx, 0, ourl, 1);

	//////////////////////////////

	//推流
	ret = avformat_network_init();
	if (ret != 0)
		return CheckErr(ret);

	ret = avio_open(&octx->pb, ourl, AVIO_FLAG_WRITE);
	if (ret < 0)
		return CheckErr(ret);

	//写入头信息
	ret = avformat_write_header(octx, nullptr);
	if (ret < 0)
		return CheckErr(ret);

	//推流每一帧数据
	AVPacket pkt;
	int64_t starttime = av_gettime();
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
			return CheckErr(ret);
	}

	return 0;
}

int main()
{
	file2rtmp();
	system("pause");
	return 0;
}