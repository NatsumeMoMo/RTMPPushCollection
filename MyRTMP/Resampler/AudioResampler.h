//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-31.
//

#ifndef AUDIORESAMPLER_H
#define AUDIORESAMPLER_H

#include <string>
#include <memory>
#include <vector>
#include <mutex>

extern "C"
{
#include "libavutil/audio_fifo.h"
#include "libavutil/opt.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"
};

using AVFramePtr = std::shared_ptr<AVFrame>;

typedef struct AudioResampleParams {
    enum AVSampleFormat src_sample_fmt;
    enum AVSampleFormat dst_sample_fmt;
    int src_sample_rate = 0;
    int dst_sample_rate = 0;
    uint64_t src_channel_layout = 0;
    uint64_t dst_channel_layout = 0;
    std::string logtag = "audioResample";
}AudioResampleParams;

class AudioResampler
{
public:
    AudioResampler();
    ~AudioResampler();
    int InitResampler(const AudioResampleParams& params);
    int closeResample();
    int initResampledData();
    int SendResampleFrame(AVFrame* frame);
    int SendResampleFrame(uint8_t* data, int size);
    AVFramePtr ReceiveResampleFrame(int desired_size = 0);
    int ReceiveResampledFrame(std::vector<AVFramePtr>& frames, int desired_size);
    AVFramePtr allocOutFrame(const int nb_samples);
    AVFramePtr getOneFrame(const int desired_size);


public:
    int GetFifoCurSize() {
        std::lock_guard<std::mutex> lock(mtx_);
        return av_audio_fifo_size(audio_fifo_);
    }

    double GetFifoCurSizeInMs() {
        std::lock_guard<std::mutex> lock(mtx_);
        return av_audio_fifo_size(audio_fifo_) * 1000.0 / params_.dst_sample_rate;
    }

    int64_t GetStartPts() const { return start_pts_; }

    int64_t GetCurPts() {
        std::lock_guard<std::mutex> lock(mtx_);
        return cur_pts_;
    }

    bool IsInited() const { return is_inited_; }

private:
    AudioResampler(const AudioResampler&) = delete;
    AudioResampler& operator=(const AudioResampler&) = delete;

private:
    std::mutex mtx_;
    SwrContext* swrctx_ = nullptr;
    AudioResampleParams params_;
    bool is_fifo_only_ = false;
    bool is_flushed_ = false;
    AVAudioFifo* audio_fifo_ = nullptr;
    int64_t start_pts_ = AV_NOPTS_VALUE;
    int64_t cur_pts_ = AV_NOPTS_VALUE;
    uint8_t** resampled_data_ = nullptr;
    int resampled_data_size_ = 8192;
    int src_channels_ = 2;
    int dst_channels_ = 2;
    int total_resampled_num_ = 0;
    std::string logtag_;
    bool is_inited_ = false;
    int dst_nb_samples_ = 1024;
    int max_dst_nb_samples_ = 1024;
    int dst_linesize_ = 0;
};



#endif //AUDIORESAMPLER_H
