//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 25-1-2.
//

#ifndef RTMPPUSHER_H
#define RTMPPUSHER_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "mediabase.h"
#include "RTMP/RTMPBase.h"

enum {
    FLV_CODECID_H264 = 7,
    FLV_CODECID_AAC = 10,
};

class RTMPPusher
{
public:
    RTMPPusher();
    ~RTMPPusher();

    bool SendMetaData(RTMPMetadata* metadata);

    RTMPBase* GetRTMPBase() const { return rtmpBase_; }

    void EnqueueVideo(VideoRawMsg& vMsg);
    void EnqueueAudio(AudioRawMsg& aMsg);

private:
    void PushThreadFunc();

    bool SendH264Packet(unsigned char* data, int size, bool is_keyframe, unsigned int timestamp);
    bool SendAACPacket(unsigned char* data, unsigned int size, unsigned int nTimeStamp);
    int SendPacket(unsigned int packet_type, unsigned char* data, unsigned int size,
        unsigned int timestamp);

private:
    RTMPBase* rtmpBase_ = nullptr; // 相比原项目, 这里使用了组合而非多继承

    /* 视频数据队列 */
    std::queue<VideoRawMsg> videoQueue_;
    std::mutex videoMtx_;
    std::condition_variable videoCond_;

    /* 音频数据队列 */
    std::queue<AudioRawMsg> audioQueue_;
    std::mutex audioMtx_;
    std::condition_variable audioCond_;

    std::thread* thread_ = nullptr;
    bool running_;
};


char * put_byte(char *output, uint8_t nVal);

char * put_be16(char *output, uint16_t nVal);

char * put_be24(char *output, uint32_t nVal);

char * put_be32(char *output, uint32_t nVal);

char *  put_be64(char *output, uint64_t nVal);

char * put_amf_string(char *c, const char *str);

char * put_amf_double(char *c, double d);

#endif //RTMPPUSHER_H
