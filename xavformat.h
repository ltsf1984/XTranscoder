#pragma once
#include <iostream>
#include <mutex>

extern "C" {
#include <libavcodec/codec_id.h>
}

struct AVFormatContext;

class XAvFormat
{
public:
	AVFormatContext* GetAVFormatContext() {
		std::lock_guard<std::mutex> lock(mtx_);
		return fmt_ctx_;
	}
	int audio_index() { return audio_index_; };
	int video_index() { return video_index_; };
	AVCodecID codec_id() { return codec_id_; };

protected:
	AVFormatContext* fmt_ctx_{ nullptr };	//封装、解封装上下文
	int audio_index_{ -1 };
	int video_index_{ -1 };
	AVCodecID codec_id_{ AV_CODEC_ID_NONE };
	std::mutex mtx_;
};

