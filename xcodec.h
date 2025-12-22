// xcodec.h
#pragma once
#include <iostream>
#include <mutex>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
}

struct AVCodecContext;
struct AVFrame;
struct AVCodecParameters;

class XCodec
{
public:
	enum class SendResult {
		Success,     // 成功发送一个输入单元（packet 或 frame）
		NeedDrain,   // 输出缓冲区已满，需要先接收数据，排空输出缓冲区（pop frame/packet）
		Ended,       // 编解码器已处于结束状态（AVERROR_EOF），拒绝接收新输入，
					 //（是“出错状态”：告诉你“别再发了，我已经关了”。)
		Failed		// 永久性错误（参数错误、内存不足、编码器/解码器异常等），不可重试
	};

	enum class ReceiveResult {
		Success,     // 成功接收一个输出单元（packet 或 frame）
		NeedFeed,    // 输入缓冲区已空，需要先发送数据，填充输入缓冲区（push frame/packet）
		Ended,      // 编解码流程已正常结束（AVERROR_EOF），所有输出数据已取完，
					//（是“完成信号”：告诉你“任务已完成，数据都在这了”。）
		Failed      // 永久性错误（参数错误、内存不足、编码器/解码器异常等），不可重试
	};

public:
	// 创建上下文(true为编码，false为解码)
	bool Create(AVCodecID codec_id, bool is_encoder=true);

	// 打开上下文
	bool Open();
	bool Close();

	// 获取上下文（新增！）
	AVCodecContext* GetContext() {
		std::lock_guard<std::mutex> lock(mtx_);
		return context_;
	}

	// 参数设置（必须在 Open() 前调用）
	bool SetVideoParam(int width, int height, AVPixelFormat pix_fmt);
	bool SetTimeBase(int num, int den);
	bool SetTimeBase(AVRational time_base);
	
	//设置上下文参数
	// 直接设置编解码器上下文字段（更可靠）

	// 帧率虽非必须，但一定要设置，不会导致帧率计算值浮动，时长计算不准
	// 最终导致掉帧等问题
	bool SetFrameRate(int num, int den);
	bool SetFrameRate(AVRational framerate); 
	bool SetBitRate(int64_t bit_rate);
	bool SetGopSize(int gop_size);
	bool SetOpt(const std::string& key, const std::string& value);
	bool SetOpt(const std::string& key, int value);

	// 创建帧frame
	AVFrame* CreateFrame();

protected:
	AVCodecContext* context_{ nullptr };	//上下文
	std::mutex mtx_;
	bool is_encoder_{ true };

};

