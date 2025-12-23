# XTranscoder

# XTranscoder - 基于FFmpeg的C++视频转码库

## 项目概述

XTranscoder是一个基于FFmpeg库开发的C++视频转码工具，提供完整的视频处理流水线。该项目封装了FFmpeg的核心功能，实现了视频文件的解封装、解码、编码和封装等完整流程，支持H.264/H.265编码格式转换、分辨率调整、码率控制等常见转码需求。

## 核心特性

- **完整转码流程**：支持从任意格式到任意格式的视频转码
- **分辨率调整**：支持视频缩放，可指定输出分辨率
- **编码格式转换**：支持H.264 ↔ H.265等编码格式互转
- **音视频处理**：视频重编码，音频保持原样或重编码
- **参数可配置**：码率、帧率、GOP大小等参数可自定义
- **线程安全**：所有核心操作都带有互斥锁保护
- **错误处理完善**：详细的错误日志和状态反馈机制

## 项目结构
XTranscoder/
├── main.cpp # 示例主程序
├── xfile_transcoder.h/.cpp # 文件转码器主类
├── xdemuxer.h/.cpp # 解封装器（读取输入文件）
├── xmuxer.h/.cpp # 封装器（写入输出文件）
├── xdecoder.h/.cpp # 解码器
├── xencoder.h/.cpp # 编码器
├── xcodec.h/.cpp # 编解码器基类
├── xavformat.h/.cpp # 格式处理基类
└── README.md # 项目说明文档

text

## 快速开始

### 环境要求

- **FFmpeg 5.x** 或更高版本（需要开发库）
- **C++11** 兼容的编译器
- **Windows**：Visual Studio 2017+
- **Linux/macOS**：GCC 7+/Clang 5+

### Windows编译（Visual Studio）

1. 下载并安装FFmpeg开发包
2. 配置项目属性：
   - 包含目录：添加FFmpeg的include路径
   - 库目录：添加FFmpeg的lib路径
   - 链接器输入：添加以下库：
     ```
     avformat.lib
     avcodec.lib
     avutil.lib
     swscale.lib
     ```

### Linux/macOS编译

```bash
# 安装依赖
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev

# 编译
g++ -std=c++11 -I/usr/local/include -L/usr/local/lib \
    main.cpp \
    xfile_transcoder.cpp \
    xdemuxer.cpp xmuxer.cpp \
    xdecoder.cpp xencoder.cpp \
    xcodec.cpp xavformat.cpp \
    -lavformat -lavcodec -lavutil -lswscale -lpthread \
    -o xtranscoder
使用示例
基本转码
cpp
#include "xfile_transcoder.h"
#include <iostream>

int main() {
    XFileTranscoder trans;
    
    // 将视频转码为H.265格式，并调整分辨率到800x600
    bool success = trans.Transcode(
        "input.mp4",           // 输入文件
        "output.mp4",          // 输出文件
        800, 600,              // 输出分辨率
        AV_CODEC_ID_HEVC,      // 输出编码格式 (H.265)
        2000,                  // 码率 (2000 kbps)
        25                     // 帧率 (25 fps)
    );
    
    if (success) {
        std::cout << "转码成功！" << std::endl;
    } else {
        std::cout << "转码失败！" << std::endl;
    }
    
    return 0;
}
API说明
cpp
bool Transcode(
    const std::string& input_file,      // 输入文件路径
    const std::string& output_file,     // 输出文件路径
    int output_width,                   // 输出视频宽度（0表示保持原样）
    int output_height,                  // 输出视频高度（0表示保持原样）
    AVCodecID output_codec_id = AV_CODEC_ID_H264,  // 输出编码格式
    int bitrate_kbps = 2000,            // 输出码率（千比特/秒）
    int fps = 25                         // 输出帧率
);
支持的编码格式
cpp
AV_CODEC_ID_H264        // H.264/AVC
AV_CODEC_ID_HEVC        // H.265/HEVC
AV_CODEC_ID_MPEG2VIDEO  // MPEG-2
AV_CODEC_ID_VP9         // VP9
// 更多格式请参考FFmpeg文档
配置示例
示例1：高清H.264转码
cpp
// 将任意视频转为1080p H.264
trans.Transcode("input.avi", "output.mp4", 1920, 1080, 
                AV_CODEC_ID_H264, 5000, 30);
示例2：H.265高效压缩
cpp
// 转为H.265，节省存储空间
trans.Transcode("input.mp4", "output_h265.mp4", 0, 0, 
                AV_CODEC_ID_HEVC, 2000, 25);
示例3：仅调整分辨率
cpp
// 保持原编码格式，只调整分辨率
trans.Transcode("input.mp4", "output_720p.mp4", 1280, 720);
核心组件详解
1. XCodec - 编解码器基类
提供统一的编解码接口，封装FFmpeg的AVCodecContext，支持：

编解码器创建与配置

参数设置（分辨率、帧率、码率等）

帧/包发送与接收的状态机管理

2. XDemuxer - 解封装器
负责读取多媒体容器，支持：

自动探测容器格式

分离音视频流

参数提取与传递

3. XMuxer - 封装器
负责写入多媒体容器，支持：

输出格式自动选择

音视频流复用

文件头/尾写入

4. XFileTranscoder - 转码流水线
协调各组件工作，实现：

解码→处理→编码完整流程

时间戳转换与同步

错误恢复与资源管理

性能优化建议
批量处理：主程序中已包含循环示例，适合批量转码

分辨率匹配：避免不必要的缩放，提高处理速度

码率适中：过高的码率不会提升质量，只会增加文件大小

硬件加速：未来可扩展支持GPU编码（NVENC/QSV/VAAPI）

故障排除
常见问题
"Failed to allocate output context"

检查输出文件路径是否有效

确认FFmpeg版本支持目标格式

"No video/audio stream found"

输入文件可能损坏或不包含期望的流

纯视频/音频文件需相应处理

时间戳不同步

确保正确设置了time_base和framerate

检查解码器/编码器的时间基转换

内存泄漏

项目使用RAII模式管理资源，确保调用Close()方法释放资源

许可证
本项目基于MIT许可证开源。

贡献指南
欢迎提交Issue和Pull Request来改进项目。

更新日志
v1.0.0
初始版本发布

支持基本视频转码功能

支持H.264/H.265编码转换

支持分辨率调整

项目作者：XTranscoder开发团队
最后更新：2025年