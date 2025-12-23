#include <iostream>
#include <string>
#include "xfile_transcoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

}

int main() {
    XFileTranscoder trans = XFileTranscoder();
    for (int i = 0; i < 1; i++)
    {
        trans.Transcode("400x300_25.h264aac.mp4", "800x600_25.h265aac.mp4", 800, 600, AV_CODEC_ID_HEVC);
        //trans.Transcode("400x300_25.h265aac.mp4", "400x300_25.h264aac.mp4", 400, 300, AV_CODEC_ID_H264);
        //trans.Transcode("400x300_25.h265.mp4", "400x300_25.h264.mp4", 400, 300, AV_CODEC_ID_H264);
    }
    return 0;
}

