//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-27.
//

#include "AudioCapture.h"

#include <iostream>
#include "dlog.h"
#include "TimeUtil.h"

AudioCapture::AudioCapture() {
    pcmBuffer.resize(4096);
}

AudioCapture::~AudioCapture()
{
    Stop();
    if(pcmBuf_)
        delete[] pcmBuf_;
}

RET_CODE AudioCapture::Init(const Properties &props)
{
    audio_test_ = props.GetProperty("audio_test", 0);
    pcmInName_ = props.GetProperty("input_pcm_name", "buweishui_48000_2_s16le.pcm");
    sample_rate_ = props.GetProperty("sample_rate", 48000);
    pcmBuf_ = new uint8_t[PCM_BUF_MAX_SIZE];
    if(!pcmBuf_)
    {
        return RET_ERR_OUTOFMEMORY;
    }
    if(openPcmFile(pcmInName_.c_str()) != 0)
    {
        LogError("openPcmFile %s failed", pcmInName_.c_str());
        return RET_FAIL;
    }
    return RET_OK;
}

void AudioCapture::Loop()
{
    int sample = 1024;
    startTime_ = TimesUtil::GetTimeMillisecond();
    unsigned int audioTimestamp = 0;  // 递增单位: 毫秒
    auto start_time = std::chrono::steady_clock::now();
    // AAC一帧通常对应1024个采样点 => 1024 / sample_rate秒
    double frameDurationMs = 1024.0 / (double)sample_rate_ * 1000.0;
    while (1)
    {
        if(request_exit_)
            break;
        if(readPcmFile() == 0)
        {
            if(callback_) {
                callback_(pcmBuffer.data(), sample * 4, audioTimestamp);
            }
        }

        // 累加时间戳
        audioTimestamp += (unsigned int)(frameDurationMs + 0.5);

        // 准实时发送：等待下一帧
        auto expected_time = start_time + std::chrono::milliseconds(audioTimestamp);
        auto now = std::chrono::steady_clock::now();
        if (expected_time > now) {
            std::this_thread::sleep_until(expected_time);
        }
    }
}

int AudioCapture::openPcmFile(const char *filename)
{
    errno_t err =  fopen_s(&pcmfd_, filename, "rb");
    if (err != 0)
        return -1;
    return 0;
}


int AudioCapture::readPcmFile()
{
    size_t total_read = 0; // 累计读取的字节数
    while (total_read < 4096) {
        // 计算当前需要读取的字节数
        size_t bytes_to_read = 4096 - total_read;
        // 尝试读取
        size_t ret = fread(pcmBuffer.data() + total_read, 1, bytes_to_read, pcmfd_);

        if (ret == 0) {
            // 检查是否到达文件末尾
            if (feof(pcmfd_)) {
                std::cout << "[AudioThread] PCM file read end. Rewinding to start.\n";
                // 重置文件指针到文件开头
                if (fseek(pcmfd_, 0, SEEK_SET) != 0) {
                    std::cerr << "Failed to rewind PCM file.\n";
                    return -1; // 重置失败，返回错误
                }
                continue; // 重新尝试读取剩余的字节
            }
            else {
                // 读取失败，处理错误
                std::cerr << "Error reading PCM file.\n";
                return -1;
            }
        }

        total_read += ret; // 累加已读取的字节数
    }

    return 0; // 成功读取4096个字节
}

int AudioCapture::closePcmFile()
{
    if (pcmfd_)
        fclose(pcmfd_);
    return 0;
}
