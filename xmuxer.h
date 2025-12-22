#pragma once

#include <iostream>
#include "xavformat.h"

struct AVPacket;
struct AVCodecContext;

class XMuxer :
    public XAvFormat
{
public:
    bool Open(std::string file, AVCodecContext* video_enc_ctx, AVCodecContext* audio_enc_ctx);
    // 参数拷贝（ 编码器上下文中参数 -> 封装器数据流中参数）
    // 编码器 -> 输出流
    bool CopyPara(int stream_index, AVCodecContext* enc_ctx);
    bool WriteHeader();
    bool Write(AVPacket* pkt);
    bool WriteTrailer();
    bool Close();
};

