//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-27.
//

#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H
#include "commonlooper.h"


class AudioCapture : public CommonLooper
{
public:
    AudioCapture();
    virtual ~AudioCapture();

    /**
     * @brief Init
     * @param "audio_test": 缺省为0，为1时数据读取本地文件进行播放
     *        "input_pcm_name": 测试模式时读取的文件路径
     *        "sample_rate": 采样率
     *        "channels": 采样通道
     *        "sample_fmt": 采样格式
     * @return
     */

    RET_CODE Init(const Properties& props);

    void Loop() override;

    void setCallback(CAPTURECALLBACK cb) { callback_ = cb; }


private:
    // PCM file只是用来测试, 写死为s16格式 2通道 采样率48Khz
    // 1帧1024采样点持续的时间21.333333333333333333333333333333ms
    int openPcmFile(const char* filename);
    //int readPcmFile(uint8_t* buf, int32_t samples);
    int readPcmFile();
    int closePcmFile();
    std::string pcmInName_;
    FILE* pcmfd_ = nullptr;
    CAPTURECALLBACK callback_;
    uint8_t* pcmBuf_;
    int32_t pcmBufSize_;
    int64_t startTime_ = 0;    // 起始时间
    double totalDuration_ = 0;        // PCM读取累计的时间
    const int PCM_BUF_MAX_SIZE = 32768;
    std::vector<uint8_t> pcmBuffer;

private:
    int audio_test_ = 0;
    int sample_rate_ = 48000;
};



#endif //AUDIOCAPTURE_H
