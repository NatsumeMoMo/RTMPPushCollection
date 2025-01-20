//
// Created by JasonWang on 24-12-26.
//

#ifndef VIDEOCAPTURE_H
#define VIDEOCAPTURE_H

#include "commonlooper.h"
#include "mediabase.h"

class VideoCapture : public CommonLooper
{
public:
    VideoCapture();
    virtual ~VideoCapture();

    /**
     * @brief Init
     * @param "x", x起始位置，缺省为0
     *          "y", y起始位置，缺省为0
     *          "width", 宽度，缺省为屏幕宽带
     *          "height", 高度，缺省为屏幕高度
     *          "format", 像素格式，AVPixelFormat对应的值，缺省为AV_PIX_FMT_YUV420P
     *          "fps", 帧数，缺省为25
     * @return
     */

    RET_CODE Init(const Properties& props);

    void Loop() override;

    void setCallback(CAPTURECALLBACK cb) { callback_ = cb; }

private:
    // 本地文件测试
    int openYuvFile(const char *filename);
    int readYuvFile(uint8_t* buf, int32_t size);
    int closeYuvFile();
    int64_t startTime_;
    double totalDuration_;
    FILE* yuvfd_ = nullptr;
    uint8_t* yuvBuf_;
    int yuvBufSize_;
    CAPTURECALLBACK callback_;

private:
    int video_test_ = 0;
    std::string yuvInName_;
    int x_;
    int y_;
    int width_;
    int height_;
    int pixelFormat_;
    int fps_;
    double frame_duration_;
};



#endif //VIDEOCAPTURE_H
