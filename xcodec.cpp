// xcodec.cpp
#include "xcodec.h"
#include <iostream>
#include <fstream>
using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>    // 编解码核心功能
#include <libavutil/avutil.h>      // 工具函数
#include <libavutil/opt.h>    // 主要头文件
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996) // 禁用 C4996 警告
#endif

bool XCodec::Create(AVCodecID codec_id, bool is_encoder)
{
	//查找编码或解码器
	is_encoder_ = is_encoder;
	const AVCodec* codec = nullptr;
	if (is_encoder)
	{
		codec = avcodec_find_encoder(codec_id);
	}			
	else
	{
		codec = avcodec_find_decoder(codec_id);
	}
	if (!codec) return false;
	
	//创建上下文
	context_ = avcodec_alloc_context3(codec);
	if (!context_) return false;
	return true;
}

bool XCodec::Open() 
{
	std::unique_lock<std::mutex> lock(mtx_);
	if (!context_) return false;

	if (is_encoder_)
	{
		if(context_->codec_type == AVMEDIA_TYPE_VIDEO)	// 视频编码器
		{
			// 视频编码器必须设置的3项参数
			if (context_->width <= 0 || context_->height <= 0) {
				std::cerr << "Invalid video width/height!" << std::endl;
				return false;
			}
			if (context_->pix_fmt == AV_PIX_FMT_NONE) {
				std::cerr << "Invalid video pixel format!" << std::endl;
				return false;
			}
		}
		else if(context_->codec_type == AVMEDIA_TYPE_AUDIO)	// 音频编码器
		{
			if (context_->sample_rate <= 0)
			{
				std::cerr << "Invalid audio sample rate!" << std::endl;
				return false;
			}
			if (context_->ch_layout.nb_channels <= 0)
			{
				std::cerr << "Invalid audio channel layout!" << std::endl;
				return false;
			}
			if (context_->sample_fmt <= AV_SAMPLE_FMT_NONE)
			{
				std::cerr << "Invalid audio sample format!" << std::endl;
				return false;
			}
			
		}
		else if (context_->codec_type == AVMEDIA_TYPE_UNKNOWN)	//未知编码器
		{
			std::cerr << "Unknown codec type! Make sure codec is properly set." << std::endl;
			return false;
		}
	}

	// 解码器不需要预验证，FFmpeg 会在解码时自动设置
	int ret = avcodec_open2(context_, nullptr, nullptr);
	if (ret < 0) {
		char errbuf[256];
		av_strerror(ret, errbuf, sizeof(errbuf));
		std::cerr << "avcodec_open2 failed: " << errbuf << std::endl;
		return false;
	}
	return true;
}

bool XCodec::Close()
{
	if (!context_) return false;
	avcodec_free_context(&context_);
	return true;
}

bool XCodec::SetVideoParam(int width, int height, AVPixelFormat pix_fmt)
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!context_) return false;
	if (width <= 0 || height <= 0) return false;
	if (pix_fmt == AV_PIX_FMT_NONE) return false;

	if (!context_->codec) {
		std::cerr << "Error: video params should be set before Open()!" << std::endl;
	}

	context_->width = width;
	context_->height = height;
	context_->pix_fmt = pix_fmt;
	return true;
}

bool XCodec::SetTimeBase(int num, int den) 
{
	std::lock_guard<std::mutex> lock(mtx_);
	// 基本校验
	if (num <= 0 || den <= 0) {
		std::cerr << "Invalid time base: " << num << "/" << den << std::endl;
		return false;
	}

	if (!context_) {
		std::cerr << "Context not created yet!" << std::endl;
		return false;
	}

	// 检查是否已打开（time_base 必须在 avcodec_open2() 前设置）
	if (!context_->codec) {
		std::cerr << "Error: time_base should be set before Open()!" << std::endl;
		// 仍允许设置，但可能无效
	}

	context_->time_base = { num, den };
	return true;
}

bool XCodec::SetTimeBase(AVRational time_base)
{
	return SetTimeBase(time_base.num, time_base.den);
}

bool XCodec::SetFrameRate(int num, int den)
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!context_) return false;
	if (num <= 0 || den <= 0) return false;
	context_->framerate = {num, den};
	return true;
}


bool XCodec::SetFrameRate(AVRational framerate)
{
	return SetFrameRate(framerate.num, framerate.den);
}

bool XCodec::SetBitRate(int64_t bit_rate) {
	std::lock_guard<std::mutex> lock(mtx_);
	if (!context_) return false;
	if (bit_rate <= 0) return false;
	context_->bit_rate = bit_rate;
	return true;
}

bool XCodec::SetGopSize(int gop_size) {
	std::lock_guard<std::mutex> lock(mtx_);
	if (!context_) return false;
	context_->gop_size = gop_size;
	return true;
}

bool XCodec::SetOpt(const std::string& key, const std::string& value)
{
	std::lock_guard<std::mutex> lock(mtx_);
	int ret = av_opt_set(context_->priv_data, key.c_str(), value.c_str(), 0);
	if (ret == 0) return true;
	return false;
}

bool XCodec::SetOpt(const std::string& key, int value)
{
	std::lock_guard<std::mutex> lock(mtx_);
	int ret = av_opt_set_int(context_->priv_data, key.c_str(), value, 0);
	if (ret == 0) return true;
	return false;
}

AVFrame* XCodec::CreateFrame()
{
	std::lock_guard<std::mutex> lock(mtx_);
	AVFrame* frame = av_frame_alloc();
	if (!frame) return nullptr;

	frame->width = context_->width;
	frame->height = context_->height;
	frame->format = context_->pix_fmt;

	int ret = av_frame_get_buffer(frame, 32);
	if (ret < 0)
	{
		av_frame_free(&frame);
		cout << "av_frame_get_buffer failed!" << endl;
		return nullptr;
	}
	return frame;
}

