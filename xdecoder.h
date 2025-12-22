//xdecoder.h
#pragma once
#include "xcodec.h"

struct AVFrame;
struct AVPacket;

class XDecoder : public XCodec {
public:
    SendResult SendPacket(AVPacket* packet);
    ReceiveResult ReceiveFrame(AVFrame* frame);
};