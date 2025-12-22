// xencode.h
#pragma once
#include "xcodec.h"
#include <vector>
#include <functional>

extern "C" {
#include <libavcodec/codec_id.h>
}

struct AVFrame;
struct AVPacket;
struct AVCodecParameters;

class XEncoder :
    public XCodec
{
public:
    //·¢ËÍ  
    SendResult SendFrame(AVFrame* frame);
    ReceiveResult ReceivePacket(AVPacket* packet);
};

