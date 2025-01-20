/*************************************************************
* File: main.cpp
 * Description:
 *   1) 从本地读取 YUV420P 文件
 *   2) 使用 H264Encoder 编码成 H.264
 *   3) 使用 CRTMPStream 推流到 RTMP 服务器
 *   4) 尽量优化低延迟
 *
 * Compile and run:
 *   g++ -std=c++11 main.cpp H264Encoder.cpp RTMPStream.cpp -lavcodec -lavutil -lavformat -lz ...
 *   ./a.out
 *************************************************************/


#include "RTMPStream.h"



int main(int argc, char* argv[])
{
    // ==== 1. 读取参数及文件名 ====
    // 例如： ./a.out rtmp://localhost/live/livestream input_640x360.yuv 640 360

    CRTMPStream rtmpSender;

    bool bRet = rtmpSender.Connect("rtmp://localhost/live/livestream");

    rtmpSender.SendYUVFile("720x480_25fps_420p.yuv");
}