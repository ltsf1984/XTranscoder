// xencode.cpp
#include "xencoder.h"
#include <iostream>
#include <fstream>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>    // 编解码核心功能
#include <libavutil/avutil.h>      // 工具函数
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")


XEncoder::SendResult XEncoder::SendFrame(AVFrame* frame)
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!context_) return SendResult::Failed;

	// 允许 frame == nullptr
	int ret = avcodec_send_frame(context_, frame);
	if (ret == 0)
	{
		//成功发送普通帧 或 
		//成功提交 flush 请求（frame == nullptr）
		return SendResult::Success;
	}
	if (ret == AVERROR(EAGAIN))
	{
		//内部输出缓冲区已满 - 需要先调用 avcodec_receive_packet()
		//读取输出数据包，然后重新尝试发送输入
		return SendResult::NeedDrain;
	}
	if (ret == AVERROR_EOF)
	{
		//编码器已被刷新，无法再向其发送新帧
		return SendResult::Ended;
	}
	if (ret < 0) 
	{
		//发送帧失败，无法再向其发送新帧
		char buff[1024]{ 0 };

		av_strerror(ret, buff, sizeof(buff));
		std::cerr << buff << std::endl;
		return SendResult::Failed;
	}
	return SendResult::Failed;	// unreachable
}

XEncoder::ReceiveResult XEncoder::ReceivePacket(AVPacket* packet)
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!context_) return ReceiveResult::Failed;
	int ret = avcodec_receive_packet(context_, packet);

	if (ret == 0) 
	{
		//成功获取到编码后的数据包
		return ReceiveResult::Success;
	}
	if (ret == AVERROR(EAGAIN))
	{
		//内部输入缓冲区已空 - 需要先调用 avcodec_send_frame()
		//发送更多输入帧，然后重新尝试读取数据
		return ReceiveResult::NeedFeed;
	}

	if (ret == AVERROR_EOF)
	{
		//编码器已被完全刷新，并且不会有更多的输出数据包
		//停止接收数据，正常结束编码流程
		return ReceiveResult::Ended;
	}
	if (ret < 0)
	{
		//接收数据包失败，无法再向其发送新帧
		char buff[1024]{ 0 };

		av_strerror(ret, buff, sizeof(buff));
		std::cerr << buff << std::endl;
		return ReceiveResult::Failed;
	}

	return ReceiveResult::Failed;	// unreachable
}
