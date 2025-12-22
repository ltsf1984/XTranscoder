#pragma once

#include "xavformat.h"

struct AVCodecContext;
struct AVPacket;

class XDemuxer :
    public XAvFormat
{
public:
    bool Open(std::string file);
    // 参数拷贝（解封装器数据流中参数 -> 解码器上下文中参数）
    // 输入流 -> 解码器
    bool CopyPara(int stream_index, AVCodecContext* dec_ctx);
    bool Read(AVPacket* pkt);
    bool Close();
};

