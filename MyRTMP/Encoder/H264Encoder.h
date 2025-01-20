#pragma once

#include <string>

#include "mediabase.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}


class H264Encoder
{
public:
    H264Encoder();
    ~H264Encoder();

    int Init(const Properties& properties);
    int Encode(uint8_t* data, int in_samples, uint8_t* out, int& out_size);
    int Encode(AVFrame* frame, uint8_t* out, int& out_size);
    AVPacket* Encode(uint8_t* yuv, uint64_t pts = 0, const int flush = 0);

public:
    int getSPS(uint8_t* sps, int& sps_len);
    int getPPS(uint8_t* pps, int& pps_len);
    inline int getWidth() { return codec_ctx_->width; }
    inline int getHeight() { return codec_ctx_->height; }
    double getFrameRate() { return codec_ctx_->framerate.num / codec_ctx_->framerate.den; }
    inline int64_t getBitRate() { return codec_ctx_->bit_rate; }
    inline uint8_t* getSPSData() { return (uint8_t*)sps_.c_str(); }
    inline int getSPSSize() { return sps_.size(); }
    inline uint8_t* getPPSData() { return (uint8_t*)pps_.c_str(); }
    inline int getPPSSize() { return pps_.size(); }
    AVCodecContext* getCodecCtx() { return codec_ctx_; }

private:
    int count;
    int data_size_;
    int framecnt_;

    /* 初始化参数 */
    std::string codecName_;
    int width_;
    int height_;
    int fps_;
    int bFrames_;                       // B帧数量
    int bitrate_;
    int gop_;
    bool annexb_;                       // 默认不带 startcode
    std::string profile_;
    std::string levelId_;

    std::string sps_;
    std::string pps_;

    /* data */
    AVFrame* frame_ = nullptr;
    uint8_t* picBuf_ = nullptr;
    AVPacket pkt_;

    /* Encoder Msg */
    AVCodec* codec_ = nullptr;
    AVDictionary* param_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;

    int64_t pts_ = 0;
};