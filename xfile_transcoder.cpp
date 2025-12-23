//xfile_transcoder.cpp
#include "xfile_transcoder.h"
#include "xencoder.h"
#include "xdecoder.h"
#include <iostream>
#include <fstream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avio.h>        // I/O相关标志
#include <libavutil/error.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

XFileTranscoder::~XFileTranscoder() {
	Cleanup();
}

bool XFileTranscoder::Transcode(
	const std::string& input_file,
	const std::string& output_file,
	int output_width, int output_height,
	AVCodecID output_codec_id,
	int bitrate_kbps,
	int fps
)
{
	// 创建视频解封装器对象
	demuxer_ = new XDemuxer();
	// 创建视频封装器对象
	muxer_ = new XMuxer();

	// 打开解封装器
	if (!demuxer_->Open(input_file))
	{
		std::cerr << "Error: Cannot open demuxer in '" << input_file << "'" << std::endl;
		return false;
	}

	// 输入视频、音频信息
	av_dump_format(demuxer_->GetAVFormatContext(), demuxer_->video_index(), nullptr, 0);

	// 设置视频解码器
	video_decoder_ = SetupDecoder(demuxer_->video_index());
	if (!video_decoder_)
	{
		std::cerr << "Error: setup viedo decoder failed!" << std::endl;
		return false;
	}

	// 设置音频频解码器
	audio_decoder_ = SetupDecoder(demuxer_->audio_index());
	if (!audio_decoder_)
	{
		std::cerr << "Warning: setup audio decoder failed!" << std::endl;
		//return false;
	}

	// 设置视频编码器
	video_encoder_ = SetupVideoEncoder(
		demuxer_->video_index(),
		output_width, output_height,
		output_codec_id,
		bitrate_kbps,
		fps
	);
	if (!video_encoder_)
	{
		std::cerr << "Error: setup video encoder failed!" << std::endl;
		return false;
	}

	// 设置音频编码器
	audio_encoder_ = SetupAudioEncoder(demuxer_->audio_index());
	if (!audio_encoder_)
	{
		std::cerr << "Warning: setup audio encoder failed!" << std::endl;
		//return false;
	}

	// 打开封装器
	if (!muxer_->Open(output_file,
		video_encoder_ ? video_encoder_->GetContext() : nullptr,
		audio_encoder_ ? audio_encoder_->GetContext() : nullptr))
	{
		std::cerr << "Error: muxer open failed!" << std::endl;
		return false;
	}

	if (!muxer_->WriteHeader())
	{
		std::cerr << "Error: Write header failed" << std::endl;
		return false;
	}

	AVPacket* pkt = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();
	// 初始化缩放帧（即使不用，也分配，避免 nullptr 检查）
	scaled_video_frame_ = av_frame_alloc();

	XDecoder* decoder = nullptr;
	XEncoder* encoder = nullptr;
	AVStream* out_stream = nullptr;
	AVStream* in_stream = nullptr;
	int stream_index = 0;

	video_frame_counter_ = 0;
	audio_frame_counter_ = 0;
	bool is_successed = true;
	// 主循环
	while (true)
	{
		av_packet_unref(pkt);
		if (!demuxer_->Read(pkt))
		{
			break;
		}

		if (pkt->stream_index == demuxer_->video_index())
		{
			decoder = video_decoder_;
			encoder = video_encoder_;
		}
		else if (pkt->stream_index == demuxer_->audio_index())
		{
			decoder = audio_decoder_;
			encoder = audio_encoder_;
		}

		stream_index = pkt->stream_index;
		in_stream = demuxer_->GetAVFormatContext()->streams[stream_index];
		out_stream = muxer_->GetAVFormatContext()->streams[stream_index];

		auto send_ret = decoder->SendPacket(pkt);
		if (send_ret == XDecoder::SendResult::Failed)
		{
			is_successed = false;
			goto cleanup;
		}
		if (send_ret == XDecoder::SendResult::Ended)
		{
			break;
		}

		while (true)
		{
			av_frame_unref(frame);
			auto recv_ret = decoder->ReceiveFrame(frame);
			if (recv_ret == XDecoder::ReceiveResult::Failed)
			{
				is_successed = false;
				goto cleanup;
			}
			if (recv_ret == XDecoder::ReceiveResult::NeedFeed ||
				recv_ret == XDecoder::ReceiveResult::Ended)
			{
				break;
			}

			// 关键步骤1：时间戳转换（输入时间基 → 编码器时间基）
			int64_t pts = frame->best_effort_timestamp; // FFmpeg 推荐的时间戳
			if (pts != AV_NOPTS_VALUE) {
				frame->pts = av_rescale_q(pts,
					in_stream->time_base,
					encoder->GetContext()->time_base);
			}
			else {
				// 如果没有原始时间戳，使用帧计数器
				if (stream_index == demuxer_->video_index())
				{
					frame->pts = video_frame_counter_++;
				}
				else if (stream_index == demuxer_->audio_index())
				{
					frame->pts = audio_frame_counter_++;
				}
			}
			// 重要的：设置pict_type，让编码器自动决定
			frame->pict_type = AV_PICTURE_TYPE_NONE;

			// 帧缩放处理
			AVFrame* frame_to_encode = frame;
			if (stream_index == demuxer_->video_index() && sws_video_ctx_)
			{
				int dst_width = video_encoder_->GetContext()->width;
				int dst_height = video_encoder_->GetContext()->height;
				AVPixelFormat dst_pix_fmt = video_encoder_->GetContext()->pix_fmt;

				if (scaled_video_frame_->width != dst_width ||
					scaled_video_frame_->height != dst_height ||
					scaled_video_frame_->format != dst_pix_fmt)
				{
					// 先释放旧缓冲区
					av_frame_unref(scaled_video_frame_);

					// 设置新参数
					scaled_video_frame_->width = dst_width;
					scaled_video_frame_->height = dst_height;
					scaled_video_frame_->format = dst_pix_fmt;

					if (av_frame_get_buffer(scaled_video_frame_, 0) < 0)
					{
						std::cerr << "Error: av_frame_get_buffer for scaled frame failed!" << std::endl;
						is_successed = false;
						goto cleanup;
					}
				}

				// 执行缩放
				int ret = sws_scale(sws_video_ctx_,
					frame->data, frame->linesize, 0, frame->height,
					scaled_video_frame_->data, scaled_video_frame_->linesize);
				if (ret < 0) {
					std::cerr << "Error: sws_scale failed!" << std::endl;
					is_successed = false;
					goto cleanup;
				}

				scaled_video_frame_->pts = frame->pts;
				scaled_video_frame_->pkt_dts = frame->pkt_dts;
				scaled_video_frame_->best_effort_timestamp = frame->best_effort_timestamp;
				scaled_video_frame_->key_frame = frame->key_frame;
				scaled_video_frame_->pict_type = AV_PICTURE_TYPE_NONE;

				frame_to_encode = scaled_video_frame_;
			}

			auto send_ret = encoder->SendFrame(frame_to_encode);
			if (send_ret == XEncoder::SendResult::Failed)
			{
				is_successed = false;
				goto cleanup;
			}
			if (send_ret == XEncoder::SendResult::Ended)
			{
				break;
			}

			while (true)
			{
				av_packet_unref(pkt);
				auto recv_ret = encoder->ReceivePacket(pkt);
				if (recv_ret == XEncoder::ReceiveResult::Failed)
				{
					is_successed = false;
					goto cleanup;
				}
				if (recv_ret == XEncoder::ReceiveResult::Ended ||
					recv_ret == XEncoder::ReceiveResult::NeedFeed)
				{
					break;
				}

				// 关键步骤2：转换时间基
				if (!out_stream) break;

				pkt->pts = av_rescale_q(pkt->pts,
					encoder->GetContext()->time_base,
					out_stream->time_base);
				pkt->dts = av_rescale_q(pkt->dts,
					encoder->GetContext()->time_base,
					out_stream->time_base);

				pkt->stream_index = stream_index;
				if (!muxer_->Write(pkt))
				{
					is_successed = false;
					goto cleanup;
				}
			}
		}
	}

	// 刷新解码器，编码器
	FlushDecoder();
	FlushEncoder();

	if (!muxer_->WriteTrailer())
	{
		is_successed = false;
		goto cleanup;
	}

cleanup:
	av_packet_free(&pkt);
	av_frame_free(&frame);

	// 清理资源
	Cleanup();
	return is_successed;

}

XDecoder* XFileTranscoder::SetupDecoder(int stream_index)
{
	if (stream_index < 0) return nullptr;

	XDecoder* decoder = new XDecoder();
	// 创建解码器
	AVCodecID codec_id = demuxer_->GetAVFormatContext()->streams[stream_index]->codecpar->codec_id;
	if (!decoder->Create(codec_id, false))
	{
		std::cerr << "Error: decoder create failed!" << std::endl;
		return nullptr;
	}

	// 复制参数到解码器上下文
	if (!demuxer_->CopyPara(stream_index, decoder->GetContext()))
	{
		std::cerr << "Error: copy codec paramter failed!" << std::endl;
		return nullptr;
	}

	// 打开解码器
	if (!decoder->Open()) {
		std::cerr << "Error: Failed to open decoder!" << std::endl;
		return nullptr;
	}

	return decoder;
}

// 音频保持不变，所以直接使用了解码器中的参数，也是输入流中的参数
// 如果需要更改，则增加参数或重载一个接口函数，来对音频编码器设置
XEncoder* XFileTranscoder::SetupAudioEncoder(int stream_index)
{
	if (stream_index < 0) return nullptr;
	XEncoder* encoder = new XEncoder();
	// 创建编码器
	AVCodecID codec_id = audio_decoder_->GetContext()->codec_id;
	if (!encoder->Create(codec_id))
	{
		std::cerr << "Error: encoder create failed!" << std::endl;
		delete encoder;
		return nullptr;
	}

	// 音频保持不变，直接使用了解码器中的参数
	encoder->GetContext()->sample_fmt = audio_decoder_->GetContext()->sample_fmt;
	encoder->GetContext()->sample_rate = audio_decoder_->GetContext()->sample_rate;
	encoder->GetContext()->ch_layout = audio_decoder_->GetContext()->ch_layout;
	encoder->GetContext()->bit_rate = audio_decoder_->GetContext()->bit_rate;

	if (!encoder->Open())
	{
		std::cerr << "Error: encoder open failed!" << std::endl;
		delete encoder;
		return nullptr;
	}

	return encoder;
}

XEncoder* XFileTranscoder::SetupVideoEncoder(
	int stream_index,
	int width, int height,
	AVCodecID codec_id,
	int bitrate_kbps,
	int fps
)
{
	if (!video_decoder_)
	{
		std::cerr << "Error: video decoder is not initialized!" << std::endl;
		return nullptr;
	}

	// 默认使用原始尺寸，如果设置了宽、高就使用设置值
	int src_width = video_decoder_->GetContext()->width;
	int src_height = video_decoder_->GetContext()->height;
	AVPixelFormat pix_fmt = video_decoder_->GetContext()->pix_fmt;
	if (width <= 0) width = src_width;
	if (height <= 0) height = src_height;


	XEncoder* encoder = new XEncoder();
	// 创建编码器
	if (!encoder->Create(codec_id))
	{
		std::cerr << "Error: encoder create failed!" << std::endl;
		delete encoder;
		return nullptr;
	}

	encoder->SetVideoParam(width, height, pix_fmt);
	encoder->SetTimeBase(1, fps);
	encoder->SetFrameRate(fps, 1);

	if (!encoder->Open())
	{
		std::cerr << "Error: encoder open failed!" << std::endl;
		delete encoder;
		return nullptr;
	}

	// 设置缩放上下文
	if (sws_video_ctx_)
	{
		sws_freeContext(sws_video_ctx_);
		sws_video_ctx_ = nullptr;
	}

	// 如果原宽、高和目标宽、高不相等则进行缩放
	if (width != src_width || height != src_height)
	{
		sws_video_ctx_ = sws_getContext(
			src_width, src_height, pix_fmt,
			width, height, pix_fmt,
			SWS_BICUBIC,
			nullptr, nullptr, nullptr);
		if (!sws_video_ctx_)
		{
			std::cerr << "Error: Failed to create scaling context!" << std::endl;
			delete encoder;
			return nullptr;
		}
	}

	return encoder;
}

bool XFileTranscoder::FlushDecoder()
{
	XDecoder* decoder = nullptr;
	XEncoder* encoder = nullptr;
	AVPacket* pkt = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();
	bool is_successed = true;

	int nb_streams = demuxer_->GetAVFormatContext()->nb_streams;
	for (int i = 0; i < nb_streams; i++)
	{
		if (i == demuxer_->video_index())
		{
			decoder = video_decoder_;
			encoder = video_encoder_;
		}
		else if (i == demuxer_->audio_index())
		{
			decoder = audio_decoder_;
			encoder = audio_encoder_;
		}

		AVStream* in_stream = demuxer_->GetAVFormatContext()->streams[i];
		AVStream* out_stream = muxer_->GetAVFormatContext()->streams[i];

		auto send_ret = decoder->SendPacket(nullptr);
		if (send_ret == XDecoder::SendResult::Failed)
		{
			is_successed = false;
			goto cleanup;
		}
		if (send_ret == XDecoder::SendResult::Ended)
		{
			break;
		}

		while (true)
		{
			av_frame_unref(frame);
			auto recv_ret = decoder->ReceiveFrame(frame);
			if (recv_ret == XDecoder::ReceiveResult::Failed)
			{
				is_successed = false;
				goto cleanup;
			}
			if (recv_ret == XDecoder::ReceiveResult::NeedFeed ||
				recv_ret == XDecoder::ReceiveResult::Ended)
			{
				break;
			}



			//时间基 输入流 -> 编码器
			auto pts = frame->best_effort_timestamp;
			if (pts != AV_NOPTS_VALUE)
			{
				frame->pts = av_rescale_q(pts,
					in_stream->time_base,
					encoder->GetContext()->time_base);
			}
			else {
				if (i == demuxer_->video_index())
				{
					frame->pts = video_frame_counter_++;
				}
				else if (i == demuxer_->audio_index())
				{
					frame->pts = audio_frame_counter_++;
				}
			}

			auto send_ret = encoder->SendFrame(frame);
			if (send_ret == XEncoder::SendResult::Failed)
			{
				is_successed = false;
				goto cleanup;
			}
			if (send_ret == XEncoder::SendResult::Ended)
			{
				break;
			}

			while (true)
			{
				av_packet_unref(pkt);
				auto recv_ret = encoder->ReceivePacket(pkt);
				if (recv_ret == XEncoder::ReceiveResult::Failed)
				{
					is_successed = false;
					goto cleanup;
				}
				if (recv_ret == XEncoder::ReceiveResult::NeedFeed ||
					recv_ret == XEncoder::ReceiveResult::Ended)
				{
					break;
				}

				pkt->pts = av_rescale_q(pkt->pts,
					encoder->GetContext()->time_base,
					out_stream->time_base);
				pkt->dts = av_rescale_q(pkt->dts,
					encoder->GetContext()->time_base,
					out_stream->time_base);

				//接收到的pkt中，pkt->stream_index总为0
				//写入前应更改为对应的流索引，不然会写入其它视频流，导致出错
				pkt->stream_index = i;
				if (!muxer_->Write(pkt))
				{
					is_successed = false;
					goto cleanup;
				}
			}
		}
	}
cleanup:
	av_frame_free(&frame);
	av_packet_free(&pkt);

	return is_successed;
}

bool XFileTranscoder::FlushEncoder()
{
	XEncoder* encoder = nullptr;
	bool is_successed = true;
	AVPacket* pkt = av_packet_alloc();

	int nb_streams = muxer_->GetAVFormatContext()->nb_streams;
	for (int i = 0; i < nb_streams; i++)
	{
		if (i == muxer_->video_index())
		{
			encoder = video_encoder_;
		}
		else if (i == muxer_->audio_index())
		{
			encoder = audio_encoder_;
		}

		if (!encoder) continue;
		AVStream* out_stream = muxer_->GetAVFormatContext()->streams[i];

		auto send_ret = encoder->SendFrame(nullptr);
		if (send_ret == XEncoder::SendResult::Failed)
		{
			is_successed = false;
			goto cleanup;
		}
		if (send_ret == XEncoder::SendResult::Ended)
		{
			break;
		}

		while (true)
		{
			av_packet_unref(pkt);
			auto recv_ret = encoder->ReceivePacket(pkt);
			if (recv_ret == XEncoder::ReceiveResult::Failed)
			{
				is_successed = false;
				goto cleanup;
			}
			if (recv_ret == XEncoder::ReceiveResult::Ended)
			{
				break;
			}
			if (recv_ret == XEncoder::ReceiveResult::NeedFeed)
			{
				/*encoder->SendFrame(nullptr);
				continue;*/
				break;
			}

			pkt->pts = av_rescale_q(pkt->pts,
				encoder->GetContext()->time_base,
				out_stream->time_base);
			pkt->dts = av_rescale_q(pkt->dts,
				encoder->GetContext()->time_base,
				out_stream->time_base);

			//接收到的pkt中，pkt->stream_index总为0
			//写入前应更改为对应的流索引，不然会写入其它视频流，导致出错
			pkt->stream_index = i;
			if (!muxer_->Write(pkt))
			{
				is_successed = false;
				goto cleanup;
			}
		}

	}

cleanup:
	av_packet_free(&pkt);

	return is_successed;;
}

void XFileTranscoder::Cleanup()
{
	// 关闭并清理编码器，解码器
	if (video_encoder_)
	{
		video_encoder_->Close();
		video_encoder_ = nullptr;
	}
	if (audio_encoder_)
	{
		audio_encoder_->Close();
		audio_encoder_ = nullptr;
	}
	if (video_decoder_)
	{
		video_decoder_->Close();
		video_decoder_ = nullptr;
	}
	if (audio_decoder_)
	{
		audio_decoder_->Close();
		audio_decoder_ = nullptr;
	}

	// 关闭并清理封装器，解封装器
	if (muxer_)
	{
		muxer_->Close();
		muxer_ = nullptr;
	}
	if (demuxer_)
	{
		demuxer_->Close();
		demuxer_ = nullptr;
	}
}
