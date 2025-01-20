//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 25-1-2.
//

#include "RTMPPusher.h"

#include <TimeUtil.h>
#include <iostream>

RTMPPusher::RTMPPusher() {
    rtmpBase_ = new RTMPBase(RTMP_BASE_TYPE_PUSH);
    running_ = true;
    thread_ = new std::thread(&RTMPPusher::PushThreadFunc, this);
}

RTMPPusher::~RTMPPusher() {
    running_ = false;
    if(thread_) {
        thread_->join();
        delete thread_;
        thread_ = nullptr;
    }

    if (rtmpBase_) {
        delete rtmpBase_;
        rtmpBase_ = nullptr;
    }
}


void RTMPPusher::PushThreadFunc()
{

    while (running_)
    {
        /* 1. 取一帧视频（如有） */
        {
            std::unique_lock<std::mutex> lock(videoMtx_);
            if(!videoQueue_.empty())
            {
                VideoRawMsg vMsg = std::move(videoQueue_.front());
                videoQueue_.pop();
                lock.unlock();

                SendH264Packet(vMsg.videoData.data(), vMsg.nSize, vMsg.bIsKeyFrame, vMsg.nTimestamp);
            }
            else {
                // 没有视频帧可取，先不阻塞
            }
        }

        /* 取一帧音频（如有） */
        {
            std::unique_lock<std::mutex> lock(audioMtx_);
            if(!audioQueue_.empty())
            {
                AudioRawMsg aMsg = std::move(audioQueue_.front());
                audioQueue_.pop();
                lock.unlock();

                SendAACPacket(aMsg.audioData.data(), aMsg.nSize, aMsg.nTimestamp);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "[PushThread] All done, exit.\n";
}


bool RTMPPusher::SendMetaData(RTMPMetadata *metadata) {
    if (metadata == nullptr)
    {
        return false;
    }
    char body[1024] = { 0 };

    char * p = (char *)body;
    p = put_byte(p, AMF_STRING);
    p = put_amf_string(p, "@setDataFrame");

    p = put_byte(p, AMF_STRING);
    p = put_amf_string(p, "onMetaData");

    p = put_byte(p, AMF_OBJECT);
    p = put_amf_string(p, "copyright");
    p = put_byte(p, AMF_STRING);
    p = put_amf_string(p, "firehood");

    /* Video */
    p = put_amf_string(p, "width");
    p = put_amf_double(p, metadata->nWidth);

    p = put_amf_string(p, "height");
    p = put_amf_double(p, metadata->nHeight);

    p = put_amf_string(p, "framerate");
    p = put_amf_double(p, metadata->nFrameRate);

    p = put_amf_string(p, "videodatarate");
    p = put_amf_double(p, metadata->nVideoDataRate);

    p = put_amf_string(p, "videocodecid");
    p = put_amf_double(p, FLV_CODECID_H264);

    /* Audio */
    if(metadata->bHasAudio)
    {
        p = put_amf_string(p, "hasAudio");
        p = put_byte(p, AMF_BOOLEAN);
        *p++ = metadata->bHasAudio ? 0x01 : 0x00;

        p = put_amf_string(p, "audiosamplerate");
        p = put_amf_double(p, metadata->nAudioSampleRate);

        p = put_amf_string(p, "audiosamplesize");
        p = put_amf_double(p, metadata->nAudioSampleSize);

        p = put_amf_string(p, "audiochannels");
        p = put_amf_double(p, metadata->nAudioChannels);

        p = put_amf_string(p, "audiocodecid");
        p = put_amf_double(p, FLV_CODECID_AAC);
    }
    p = put_amf_string(p, "");
    p = put_byte(p, AMF_OBJECT_END);

    SendPacket(RTMP_PACKET_TYPE_INFO, (unsigned char*)body, p - body, 0);

    /* Send AVC sequence header（SPS/PPS） <--> SendH264SequenceHeader(VideoSequenceHeaderMsg *seq_header) */

    if(metadata->bHasVideo)
    {
        int i = 0;
        body[i++] = 0x17;                   // 1:keyframe  7: AVC
        body[i++] = 0x00;                   // AVC Sequence header

        body[i++] = 0x00;
        body[i++] = 0x00;
        body[i++] = 0x00;                   // fill in 0

        /* AVCDecoderConfigurationVersion */
        body[i++] = 0x01;                   // ConfigurationVersion
        body[i++] = metadata->Sps[1];       // AVCProfileIndication
        body[i++] = metadata->Sps[2];       // Profile compatibility
        body[i++] = metadata->Sps[3];       // AVCLevel Indication
        body[i++] = 0xff;                   // LengthSizeMinusOne

        /* SPS nums */
        body[i++] = 0xE1;                   // &0x1f

        /* SPS data length */
        body[i++] = metadata->nSpsLen >> 8;
        body[i++] = metadata->nSpsLen & 0xFF;

        /* SPS data */
        memcpy(&body[i], metadata->Sps, metadata->nSpsLen);
        i = i + metadata->nSpsLen;

        /* PPS nums */
        body[i++] = 0x01;                   // &0x1f
        body[i++] = metadata->nPpsLen >> 8;
        body[i++] = metadata->nPpsLen & 0xFF;

        /* PPS data */
        memcpy(&body[i], metadata->Pps, metadata->nPpsLen);
        i = i + metadata->nPpsLen;
        SendPacket(RTMP_PACKET_TYPE_VIDEO, (unsigned char*)body, i, 0);
    }


    /* Send AAC sequence header */

    if(metadata->bHasAudio)
    {
        int i = 0;
        char audioBody[2] = {0};

        audioBody[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo)
        audioBody[i++] = 0x00; // AACPacketType: 0 (sequence header)

        // AudioSpecificConfig
        // 通常为2字节，可以从AACEncoder或音频源获取
        char aacSeqConfig[2];
        memcpy(aacSeqConfig, metadata->AudioSpecCfg, metadata->nAudioSpecCfgLen);
        // 构建完整的音频包
        char fullAudioBody[4];
        fullAudioBody[0] = 0xAF; // 配置
        fullAudioBody[1] = 0x00; // 序列头
        fullAudioBody[2] = aacSeqConfig[0];
        fullAudioBody[3] = aacSeqConfig[1];

        // 发送音频MetaData
        SendPacket(RTMP_PACKET_TYPE_AUDIO, (unsigned char*)fullAudioBody, 4, 0);
    }

    return true;
}

void RTMPPusher::EnqueueVideo(VideoRawMsg& vMsg)
{
    {
        std::unique_lock<std::mutex> lock(videoMtx_);
        videoQueue_.push(std::move(vMsg));
    }
    videoCond_.notify_one();
}

void RTMPPusher::EnqueueAudio(AudioRawMsg& aMsg)
{
    {
        std::unique_lock<std::mutex> lock(audioMtx_);
        audioQueue_.push(std::move(aMsg));
    }
    audioCond_.notify_one();
}

bool RTMPPusher::SendH264Packet(unsigned char *data, int size, bool is_keyframe, unsigned int timestamp) {
    if(data == nullptr && size < 11)
        return false;

    unsigned char* body = new unsigned char[size + 9];

    int i = 0;
    if(is_keyframe)
        body[i++] = 0x17; // 1:I frame   7:AVC
    else
        body[i++] = 0x27; // 2:P frame   7:AVC

    body[i++] = 0x01; // AVC NALU
    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;

    // NALU size
    body[i++] = size >> 24;
    body[i++] = size >> 16;
    body[i++] = size >> 8;
    body[i++] = size & 0xff;

    // NALU data
    memcpy(&body[i], data, size);

    bool ret = SendPacket(RTMP_PACKET_TYPE_VIDEO, body, i + size, timestamp);
    delete [] body;
    return ret;
}

bool RTMPPusher::SendAACPacket(unsigned char *data, unsigned int size, unsigned int nTimeStamp) {
    if(data == NULL || size == 0)
    {
        return false;
    }

    unsigned char *body = new unsigned char[size + 2];
    int i = 0;

    body[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo)
    body[i++] = 0x01; // AACPacketType: 1 (raw AAC frame)

    memcpy(&body[i], data, size);

    bool bRet = SendPacket(RTMP_PACKET_TYPE_AUDIO, body, i + size, nTimeStamp);

    delete[] body;

    return bRet;
}


int RTMPPusher::SendPacket(unsigned int packet_type, unsigned char *data, unsigned int size, unsigned int timestamp) {
    if(rtmpBase_->rtmp_ == nullptr)
        return false;

    RTMPPacket pkt;
    RTMPPacket_Reset(&pkt);
    RTMPPacket_Alloc(&pkt, size);

    pkt.m_packetType = packet_type;
    pkt.m_nChannel = 0x04;

    pkt.m_headerType = RTMP_PACKET_SIZE_LARGE;
    pkt.m_nTimeStamp = timestamp;
    pkt.m_nInfoField2 = rtmpBase_->rtmp_->m_stream_id;
    pkt.m_nBodySize = size;
    memcpy(pkt.m_body, data, size);

    int ret = RTMP_SendPacket(rtmpBase_->rtmp_, &pkt, 0);
    if(ret != 1)
        LogInfo("RTMP_SendPacket fail %d\n",ret);
    RTMPPacket_Free(&pkt);
    return ret;
}


char * put_byte(char *output, uint8_t nVal)
{
    output[0] = nVal;
    return output + 1;
}
char * put_be16(char *output, uint16_t nVal)
{
    output[1] = nVal & 0xff;
    output[0] = nVal >> 8;
    return output + 2;
}
char * put_be24(char *output, uint32_t nVal)
{
    output[2] = nVal & 0xff;
    output[1] = nVal >> 8;
    output[0] = nVal >> 16;
    return output + 3;
}
char * put_be32(char *output, uint32_t nVal)
{
    output[3] = nVal & 0xff;
    output[2] = nVal >> 8;
    output[1] = nVal >> 16;
    output[0] = nVal >> 24;
    return output + 4;
}
char *  put_be64(char *output, uint64_t nVal)
{
    output = put_be32(output, nVal >> 32);
    output = put_be32(output, nVal);
    return output;
}
char * put_amf_string(char *c, const char *str)
{
    uint16_t len = strlen(str);
    c = put_be16(c, len);
    memcpy(c, str, len);
    return c + len;
}
char * put_amf_double(char *c, double d)
{
    *c++ = AMF_NUMBER;  /* type: Number */
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&d;
        co = (unsigned char *)c;
        co[0] = ci[7];
        co[1] = ci[6];
        co[2] = ci[5];
        co[3] = ci[4];
        co[4] = ci[3];
        co[5] = ci[2];
        co[6] = ci[1];
        co[7] = ci[0];
    }
    return c + 8;
}