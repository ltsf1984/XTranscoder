//xdecoder.cpp
#include "xdecoder.h"
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

XCodec::SendResult XDecoder::SendPacket(AVPacket* packet) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!context_) return SendResult::Failed;

    int ret = avcodec_send_packet(context_, packet);
    if (ret == 0) 
    {
        return SendResult::Success;
    }

    if (ret == AVERROR(EAGAIN)) 
    {
        return SendResult::NeedDrain;
    }

    if (ret == AVERROR_EOF) 
    {
        return SendResult::Ended;
    }
    
    if(ret < 0)
    {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "avcodec_send_packet failed: " << errbuf << std::endl;
        return SendResult::Failed;
    }
    return SendResult::Failed;  // unreachable
}

XCodec::ReceiveResult XDecoder::ReceiveFrame(AVFrame* frame) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!context_ || !frame) return ReceiveResult::Failed;

    int ret = avcodec_receive_frame(context_, frame);
    if (ret == 0) 
    {
        return ReceiveResult::Success;
    }

    if (ret == AVERROR(EAGAIN)) 
    {
        return ReceiveResult::NeedFeed;
    }

    if (ret == AVERROR_EOF) 
    {
        return ReceiveResult::Ended;
    }

    if(ret < 0)
    {
        char errbuf[1024];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "avcodec_receive_frame failed: " << errbuf << std::endl;
        return ReceiveResult::Failed;
    }
    return ReceiveResult::Failed;   // unreachable
}