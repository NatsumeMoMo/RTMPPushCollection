//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-26.
//

#ifndef RTMPBASE_H
#define RTMPBASE_H

#include <string>

#include "rtmp.h"

enum RTMP_BASE_TYPE
{
    RTMP_BASE_TYPE_UNKNOW,
    RTMP_BASE_TYPE_PLAY,
    RTMP_BASE_TYPE_PUSH
};

class RTMPBase
{
public:
    RTMPBase();
    RTMPBase(RTMP_BASE_TYPE rtmp_obj_type);
    RTMPBase(RTMP_BASE_TYPE rtmp_obj_type, std::string& url);
    RTMPBase(std::string& url, bool is_recv_audio, bool is_recv_video); //此构造函数默认构造player
    ~RTMPBase();

    bool Connect();
    bool Connect(std::string url);
    void SetConnectUrl(std::string& url) { url_ = url; }
    void Disconnect();
    bool IsConnected();

    bool SetReceiveAudio(bool enable) { enable_audio_ = enable; }
    bool SetReceiveVideo(bool enable) { enable_video_ = enable; }
    static uint32_t GetSampleRateByFreqIdx(uint8_t freqIdx);

private:
    bool InitRtmp();
    RTMP_BASE_TYPE rtmp_obj_type_;

public:
    RTMP* rtmp_;
    std::string url_;
    bool enable_audio_; // 是否打开视频
    bool enable_video_; // 是否打开音频
};


#endif //RTMPBASE_H
