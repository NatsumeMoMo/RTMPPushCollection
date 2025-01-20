//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-26.
//

#include "VideoCapture.h"
#include "dlog.h"
#include "TimeUtil.h"

VideoCapture::VideoCapture() {

}

VideoCapture::~VideoCapture() {
    if(yuvBuf_)
        delete[] yuvBuf_;
}

RET_CODE VideoCapture::Init(const Properties &props) {
    video_test_ = props.GetProperty("video_test", 0);
    yuvInName_ = props.GetProperty("input_yuv_name", "720x480_25fps_420p.yuv");
    x_ = props.GetProperty("x", 0);
    y_ = props.GetProperty("y", 0);
    width_ = props.GetProperty("width", 1920);
    height_ = props.GetProperty("height", 1080);
    pixelFormat_ = props.GetProperty("pixel_format", 0);
    fps_ = props.GetProperty("fps", 25);
    frame_duration_ = 1000 / fps_;

    if(openYuvFile(yuvInName_.c_str()) != 0) {
        LogError("openYuvFile %s failed", yuvInName_.c_str());
        return RET_FAIL;
    }

    return RET_OK;
}

void VideoCapture::Loop() {
    yuvBufSize_ = width_ * height_ * 1.5;
    yuvBuf_ = new uint8_t[yuvBufSize_];

    totalDuration_ = 0;
    startTime_ = TimesUtil::GetTimeMillisecond();
    unsigned int videoTimestamp = 0; // 毫秒
    auto start_time = std::chrono::steady_clock::now();
    while(true)
    {
        if(request_exit_)
            break;

        if(readYuvFile(yuvBuf_, yuvBufSize_) == 0)
        {
            if(callback_)
            {
                callback_(yuvBuf_, yuvBufSize_, videoTimestamp);
            }
        }

        // 根据fps估算本帧持续时间
        unsigned int frameDurationMs = 1000 / fps_;
        videoTimestamp += frameDurationMs;

        // 准实时：等待到下一帧时间
        auto expected_time = start_time + std::chrono::milliseconds(videoTimestamp);
        auto now = std::chrono::steady_clock::now();
        if (expected_time > now) {
            std::this_thread::sleep_until(expected_time);
        }
    }

    closeYuvFile();
}

int VideoCapture::openYuvFile(const char* filename)
{
    errno_t err = fopen_s(&yuvfd_, filename, "rb");
    if(err != 0)
    {
        return -1;
    }
    return 0;
}

int VideoCapture::readYuvFile(uint8_t* buf, int32_t size)
{
    int64_t currentTime = TimesUtil::GetTimeMillisecond();
    int64_t diff = currentTime - startTime_;
    if((int64_t)totalDuration_ > diff)
    {
        return -1;
    }

    size_t ret = fread(buf, 1, size, yuvfd_);
    if(ret != size)
    {
        ret = fseek(yuvfd_, 0, SEEK_SET);
        ret = fread(buf, 1, size, yuvfd_);
        if(ret != size)
        {
            return -1;
        }
    }
    LogDebug("yuv_total_duration_:%lldms, %lldms", (int64_t)totalDuration_, diff);
    totalDuration_ += frame_duration_;
    return 0;

}

int VideoCapture::closeYuvFile()
{
    if(yuvfd_)
        fclose(yuvfd_);
    return 0;
}
