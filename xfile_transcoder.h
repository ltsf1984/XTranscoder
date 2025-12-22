//xfile_transcoder.h
#pragma once
#include <string>
#include "xdemuxer.h"
#include "xmuxer.h"

extern "C" {
#include <libavcodec/codec_id.h>
}

// 前向声明 FFmpeg 结构体（避免包含重型头文件）
struct AVFormatContext;
struct AVCodecContext;

class XEncoder;
class XDecoder;

/**
 * @brief 文件转码器：支持从任意容器格式转码为指定编码格式
 *
 * 使用示例：
 * @code
 * XFileTranscoder trans;
 * trans.Transcode("input.mp4", "output.mp4", AV_CODEC_ID_HEVC, 2000);
 * @endcode
 */
class XFileTranscoder {
public:
	// 构造/析构
	XFileTranscoder() = default;
	~XFileTranscoder();

	bool Transcode(
		const std::string& input_file,
		const std::string& output_file,
		int output_width, int output_height,
		AVCodecID output_codec_id = AV_CODEC_ID_H264,
		int bitrate_kbps = 2000,
		int fps = 25
	);

private:
	// 设置编解码器，后期用于整合各类编解码器
	XDecoder* SetupDecoder(int stream_index);
	XEncoder* SetupAudioEncoder(int stream_index);
	XEncoder* SetupVideoEncoder(
		int stream_index,
		int width, int height,
		AVCodecID codec_id,
		int bitrate_kbps,
		int fps
	);

	bool FlushDecoder();
	bool FlushEncoder();


	// 资源清理
	void Cleanup();

private:
	XDemuxer* demuxer_{ nullptr };
	XMuxer* muxer_{ nullptr };

private:
	//编码器，解码器，
	XEncoder* video_encoder_{ nullptr };
	XEncoder* audio_encoder_{ nullptr };
	XDecoder* video_decoder_{ nullptr };
	XDecoder* audio_decoder_{ nullptr };
	//视频流索引
	int video_frame_counter_{ 0 };
	int audio_frame_counter_{ 0 };
};