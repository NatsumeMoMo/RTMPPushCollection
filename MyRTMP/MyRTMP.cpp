//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-26.
//

#include "PushWork.h"
#include "dlog.h"
#include <string>

#include <Winsock2.h>
#pragma comment(lib,"ws2_32.lib")
#include "rtmp.h"
#pragma comment(lib,"librtmp.lib")

using namespace std;
#undef main // 必须添加这一行, 因为SDL中有一个 #define main SDL_main 的宏, 不添加这一行则在编译时会找不到main函数
#define RTMP_URL "rtmp://localhost/live/livestream"

// ffplay rtmp://localhost/live/livestream
// ffplay -i rtmp://localhost/live/livestream -fflags nobuffer
// ffmpeg -re -i  1920x832_25fps.flv  -vcodec copy -acodec copy  -f flv -y rtmp://localhost/live/livestream

int InitSockets()
{
    WORD version;
    WSADATA wsaData;
    version = MAKEWORD(1, 1);
    return (WSAStartup(version, &wsaData) == 0);
}

int main()
{
    init_logger("rtmp_push.log", S_INFO);

    PushWork pushwork; // 可以实例化多个，同时推送多路流

    Properties properties;
    // 音频test模式
    properties.SetProperty("audio_test", 1);    // 音频测试模式
    properties.SetProperty("input_pcm_name", "48000_2_s16le.pcm");
    // 麦克风采样属性
    properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    properties.SetProperty("mic_sample_rate", 48000);
    properties.SetProperty("mic_channels", 2);
    // 音频编码属性
    properties.SetProperty("audio_sample_rate", 48000);
    properties.SetProperty("audio_bitrate", 64*1024);
    properties.SetProperty("audio_channels", 2);

    //视频test模式
    properties.SetProperty("video_test", 1);
    properties.SetProperty("input_yuv_name", "720x480_25fps_420p.yuv");
    // 桌面录制属性
    properties.SetProperty("desktop_x", 0);
    properties.SetProperty("desktop_y", 0);
    properties.SetProperty("desktop_width", 720); //测试模式时和yuv文件的宽度一致
    properties.SetProperty("desktop_height", 480);  //测试模式时和yuv文件的高度一致
    //    properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
    properties.SetProperty("desktop_fps", 25);//测试模式时和yuv文件的帧率一致
    // 视频编码属性
    properties.SetProperty("video_bitrate", 512*1024);  // 设置码率

    // 使用缺省的
    // rtmp推流
    properties.SetProperty("rtmp_url", RTMP_URL);//测试模式时和yuv文件的帧率一致
    properties.SetProperty("rtmp_debug", 1);
    if(pushwork.Init(properties) != RET_OK)
    {
        LogError("pushwork.Init failed");
        pushwork.DeInit();
        return 0;
    }
    pushwork.Loop();

    return 0;
}
