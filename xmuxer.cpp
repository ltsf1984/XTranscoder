#include <iostream>
#include "xmuxer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

bool XMuxer::Open(std::string file, AVCodecContext* video_enc_ctx, AVCodecContext* audio_enc_ctx)
{
	if (!video_enc_ctx && !audio_enc_ctx) return false;
    // 创建输出格式上下文
    if (avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, file.c_str()) < 0) {
        std::cerr << "Error: Failed to allocate output context" << std::endl;
        return false;
    }

	// 创建输出视频流
	if(video_enc_ctx)
	{
		AVStream* out_video_stream = avformat_new_stream(fmt_ctx_, nullptr);
		video_index_ = out_video_stream->index;
		CopyPara(video_index(), video_enc_ctx);
		// 输出视频流参数
		out_video_stream->time_base = video_enc_ctx->time_base;
		out_video_stream->avg_frame_rate = video_enc_ctx->framerate;
		out_video_stream->r_frame_rate = video_enc_ctx->framerate;
	}
	
	// 创建输出音频流
	if(audio_enc_ctx)
	{
		AVStream* out_audio_stream = avformat_new_stream(fmt_ctx_, nullptr);
		audio_index_ = out_audio_stream->index;
		CopyPara(audio_index(), audio_enc_ctx);
		// 输出音频流参数
		out_audio_stream->time_base = { 1, audio_enc_ctx->sample_rate };
	}

	int ret = avio_open(&fmt_ctx_->pb, file.c_str(), AVIO_FLAG_WRITE);
	if (ret < 0)
	{
		char buff[1024]{ 0 };
		av_strerror(ret, buff, sizeof(buff));
		std::cerr << buff << std::endl;
		return false;
	}
	return true;
}

// 这个两个参数要对应，
// stream_index 是视频流索引，那enc_ctx则是视频编码器上下文
// stream_index 是音频流索引，那enc_ctx则是音频编码器上下文
bool XMuxer::CopyPara(int stream_index, AVCodecContext* enc_ctx)
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (stream_index < 0 || enc_ctx == nullptr) return false;
	AVStream* stream = fmt_ctx_->streams[stream_index];
	if (avcodec_parameters_from_context(stream->codecpar, enc_ctx) < 0)
	{
		std::cerr << "Error: avcodec parameters from context failed!" << std::endl;
		return false;
	}

	return true;
}

bool XMuxer::WriteHeader()
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_) return false;
	if (avformat_write_header(fmt_ctx_, nullptr) < 0)
	{
		std::cerr << "Error: Failed to write header！" << std::endl;
		return false;
	}
    return true;
}

bool XMuxer::Write(AVPacket* pkt)
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_) return false;
	int ret = av_interleaved_write_frame(fmt_ctx_, pkt);
	if (ret < 0)
	{
		char buff[1024]{ 0 };
		av_strerror(ret, buff, sizeof(buff));
		std::cerr << "Error: Failed to write！" << std::endl;
		std::cerr << buff << std::endl;
		return false;
	}
	return true;
}

bool XMuxer::WriteTrailer()
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_) return false;
	if (av_write_trailer(fmt_ctx_) < 0)
	{
		std::cerr << "Error: Failed to write trailer！" << std::endl;
		return false;
	}
	return true;
}

bool XMuxer::Close()
{
	if (!fmt_ctx_) return false;
	// 不需要avio_close(fmt_ctx_->pb);
	// avformat_free_context()会调用 avio_close()
	avformat_free_context(fmt_ctx_);
	fmt_ctx_ = nullptr;
	return true;
}
