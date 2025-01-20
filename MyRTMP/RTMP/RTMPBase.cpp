//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-26.
//

#include "RTMPBase.h"
#include "dlog.h"

#include <Winsock2.h>

RTMPBase::RTMPBase() : rtmp_obj_type_(RTMP_BASE_TYPE_UNKNOW), enable_audio_(true), enable_video_(true)
{
    InitRtmp();
}

RTMPBase::RTMPBase(RTMP_BASE_TYPE rtmp_obj_type) :
    rtmp_obj_type_(rtmp_obj_type),
    enable_audio_(true),
    enable_video_(true)
{
    InitRtmp();
}

RTMPBase::RTMPBase(RTMP_BASE_TYPE rtmp_obj_type, std::string& url) :
    rtmp_obj_type_(rtmp_obj_type),
    url_(url),
    enable_audio_(true),
    enable_video_(true)
{
    InitRtmp();
}

RTMPBase::RTMPBase(std::string& url, bool is_recv_audio, bool is_recv_video) :
    rtmp_obj_type_(RTMP_BASE_TYPE_PLAY),
    url_(url),
    enable_audio_(is_recv_audio),
    enable_video_(is_recv_video)
{
    InitRtmp();
}

RTMPBase::~RTMPBase()
{
    if(IsConnected()) Disconnect();
    RTMP_Free(rtmp_);
    rtmp_ = nullptr;
#ifdef WIN32
    WSACleanup();
#endif
}

bool RTMPBase::InitRtmp()
{
    bool ret = true;
#ifdef WIN32
    WORD version = MAKEWORD(1, 1);
    WSADATA wsaData;
    ret = (WSAStartup(version, &wsaData) == 0) ? true : false;
#endif
    rtmp_ = RTMP_Alloc();
    RTMP_Init(rtmp_);
    return ret;

}

bool RTMPBase::Connect()
{
    //断线重连必须执行次操作，才能重现连上（比较疑惑）
    RTMP_Free(rtmp_);
    rtmp_ = RTMP_Alloc();
    RTMP_Init(rtmp_);

    rtmp_->Link.timeout = 10;
    if(RTMP_SetupURL(rtmp_, (char*)url_.c_str()) < 0)
    {
        LogInfo("RTMP_SetupURL failed");
        return false;
    }

    rtmp_->Link.lFlags |= RTMP_LF_LIVE;
    RTMP_SetBufferMS(rtmp_, 3600 * 1000); // 1 hour
    if(rtmp_obj_type_ == RTMP_BASE_TYPE_PUSH) {
        RTMP_EnableWrite(rtmp_); /*设置可写,即发布流,这个函数必须在连接前使用,否则无效*/
    }

    if(!RTMP_Connect(rtmp_, nullptr)) {
        LogInfo("RTMP_Connect failed!");
        return false;
    }

    if(!RTMP_ConnectStream(rtmp_, 0)) {
        LogInfo("RTMP_ConnectStream failed");
        return false;
    }

    // 判断是否打开音视频, 默认打开
    if(rtmp_obj_type_ == RTMP_BASE_TYPE_PUSH) {
        if(!enable_audio_) {

            /* 复现的时候用的是从官网编译的库, 官网库里已经没有这个函数 */
            // RTMP_SendReceiveVideo(rtmp_, enable_video_);
        }

        if(!enable_audio_)
        {
            /* 同样没有这个函数 */
            // RTMP_SendReceiveAudio(rtmp_, enable_audio_);
        }
    }

    return true;
}

bool RTMPBase::Connect(std::string url)
{
    url_ = url;
    return Connect();
}

void RTMPBase::Disconnect()
{
    RTMP_Close(rtmp_);
}

bool RTMPBase::IsConnected()
{
    return RTMP_IsConnected(rtmp_);
}

uint32_t RTMPBase::GetSampleRateByFreqIdx(uint8_t freqIdx)
{
    uint32_t freq_idx_table[] = {
        96000, 88200, 64000,
        48000, 44100, 32000,
        24000, 22050, 16000,
        12000, 11025, 8000,
        7350
    };

    if(freqIdx < 13)
        return freq_idx_table[freqIdx];

    LogError("freq_idx:%d is error", freqIdx);
    return 44100;
}

