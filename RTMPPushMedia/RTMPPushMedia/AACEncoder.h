#pragma once
#include "mediabase.h"
#include "codecs.h"


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
};


class AACEncoder
{
public:
    AACEncoder();
    ~AACEncoder();

    RET_CODE Init(const Properties properties);
    AVPacket* Encode(AVFrame* frame, int64_t pts, const int flush = 0);
    AVPacket* Encode(uint8_t* pcm, uint32_t size);

public:
    inline uint32_t getRate() { return codec_ctx_->sample_rate ? codec_ctx_->sample_rate : 8000; }
    inline int getSampleRate() { return codec_ctx_->sample_rate; }
    inline int64_t getBitRate() { return codec_ctx_->bit_rate; }
    inline uint64_t getChannelLayout() { return codec_ctx_->channel_layout; }
    inline uint32_t getFrameSampleSize() { return codec_ctx_->frame_size; }
    inline uint32_t getSampleFormat() { return codec_ctx_->sample_fmt; }
    inline uint32_t getFrameByteSize() { return frame_byte_size_; }
    inline int getProfile() { return codec_ctx_->profile; }
    inline int getChannels() { return codec_ctx_->channels; }
    void getAdtsHeader(uint8_t* adts_header, int aac_length);
    AVCodecContext* getCodecCtx() { return codec_ctx_; }

private:
    int sampleRate_;                        // 默认48000
    int channels_;
    int bitrate_;                           // 默认 out_samplerate * 3
    int channelLayout_;                     // 默认 AV_CH_LAYOUT_STEREO

    AVCodec* codec_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;

    AudioCodec::Type type_;
    int frame_byte_size_;                   // 一帧的输入byte size

};