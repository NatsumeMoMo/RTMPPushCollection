//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-31.
//

#include "AudioResampler.h"
#include "dlog.h"

AudioResampler::AudioResampler() {

}

AudioResampler::~AudioResampler() {
    closeResample();
}

int AudioResampler::InitResampler(const AudioResampleParams &params)
{
    int ret = 0;
    params_ = params;
    /* fifo of output sample format */
    src_channels_ = av_get_channel_layout_nb_channels(params_.src_channel_layout);
    dst_channels_ = av_get_channel_layout_nb_channels(params_.dst_channel_layout);
    audio_fifo_ = av_audio_fifo_alloc(params_.dst_sample_fmt, dst_channels_, 1);
    if (!audio_fifo_) {
        LogError("%s av_audio_fifo_alloc failed", logtag_.c_str());
        return -1;
    }

    /* 判断是否真的需要重采样 */
    if(params_.src_sample_fmt == params_.dst_sample_fmt &&
        params_.src_sample_rate == params_.dst_sample_rate &&
        params_.src_channel_layout == params_.dst_channel_layout) {
        LogInfo("%s no resample needed, just use audio fifo",
        logtag_.c_str());
        is_fifo_only_ = true;
        return 0;
    }

    /* 分配SwrContext */
    swrctx_ = swr_alloc();

    if(!swrctx_) {
        LogError("%s swr_alloc failed", logtag_.c_str());
        return -1;
    }

    /* 设置 SwrContext 各种 in/out 参数 */
    ret |= av_opt_set_int(swrctx_, "in_channel_layout", params_.src_channel_layout, 0);
    ret |= av_opt_set_int(swrctx_, "in_sample_rate", params_.src_sample_rate, 0);
    ret |= av_opt_set_int(swrctx_, "in_sample_fmt", params_.src_sample_fmt, 0);

    ret |= av_opt_set_int(swrctx_, "out_channel_layout", params_.dst_channel_layout, 0);
    ret |= av_opt_set_int(swrctx_, "out_sample_rate", params_.dst_sample_rate, 0);
    ret |= av_opt_set_int(swrctx_, "out_sample_fmt", params_.dst_sample_fmt, 0);
    if (ret != 0) {
        LogError("av_opt_set_int() failed");
        return -1;
    }

    /* swr_init() 初始化 */
    ret = swr_init(swrctx_);
    if (ret < 0) {
        LogError("%s  failed to initialize the resampling context.", logtag_.c_str());
        return ret;
    }

    /* 计算 dst_nb_samples 的初始值 (例如 1024 输入对应多少输出) */
    int src_nb_samples = 1024;
    max_dst_nb_samples_ = dst_nb_samples_ = av_rescale_rnd(src_nb_samples, params_.dst_sample_rate,
                                                            params_.src_sample_rate, AV_ROUND_UP);
    /* 初始化输出缓冲区 (这里分配一个 resampled_data_ 用于存放重采样后数据) */
    if(initResampledData() < 0)
        return AVERROR(ENOMEM);
    is_inited_ = true;
    LogInfo("%s init done", logtag_.c_str());
    return 0;
}

int AudioResampler::closeResample()
{
    if(swrctx_)
        swr_free(&swrctx_);
    if(audio_fifo_) {
        av_audio_fifo_free(audio_fifo_);
        audio_fifo_ = nullptr;
    }
    if(resampled_data_)
        av_freep(&resampled_data_[0]);
    av_freep(&resampled_data_);
    return 0;
}

/* 初始化输出缓冲. 给内部临时的输出 buffer（resampled_data_）分配内存，用于 swr_convert() 产出的 PCM 数据放置. */
int AudioResampler::initResampledData() {
    if(resampled_data_)
        av_freep(&resampled_data_[0]);
    av_freep(&resampled_data_);
    int ret = av_samples_alloc_array_and_samples(&resampled_data_, &dst_linesize_, dst_channels_,
                                                dst_nb_samples_, params_.dst_sample_fmt, 0);
    if (ret < 0) {
        LogError("%s av_samples_alloc_array_and_samples failed", logtag_.c_str());
    }
    return ret;
}


int AudioResampler::SendResampleFrame(AVFrame *frame)
{
    std::lock_guard<std::mutex> lock(mtx_);
    /* 计算当前输入的 nb_samples，以及获取 input data 指针 */
    int src_nb_samples = 0;
    uint8_t **src_data = nullptr;
    if(frame)
    {
        src_nb_samples = frame->nb_samples;
        src_data = frame->extended_data;
        if(start_pts_ == AV_NOPTS_VALUE && frame->pts != AV_NOPTS_VALUE)
        {
            start_pts_ = frame->pts;
            cur_pts_ = frame->pts;
        }
    }
    else {
        /* 说明没有更多的输入(EOF)，此时 is_flushed = true */
        is_flushed_ = true;
    }

    /* 如果只用 FIFO，不需要重采样，直接写入 FIFO 然后 return */
    if(is_fifo_only_) {
        return src_data ? av_audio_fifo_write(audio_fifo_, (void**)src_data, src_nb_samples) : 0;
    }

    /* 如果需要重采样，则计算 dst_nb_samples */
    int delay = swr_get_delay(swrctx_, params_.src_sample_rate);
    dst_nb_samples_ = av_rescale_rnd(delay + src_nb_samples, params_.dst_sample_rate,
                                    params_.src_sample_rate, AV_ROUND_UP);

    /* 如果本次输出的样本数 > 当前已分配的最大样本数，需要重新分配 resampled_data_ */
    if(dst_nb_samples_ > max_dst_nb_samples_)
    {
        av_freep(&resampled_data_[0]);
        int ret = av_samples_alloc(resampled_data_, &dst_linesize_, dst_channels_, dst_nb_samples_,
                                    params_.dst_sample_fmt, 1);
        if(ret < 0) {
            LogError("av_samples_alloc failed");
            return 0;
        }

        max_dst_nb_samples_ = dst_nb_samples_;
    }

    /* 关键的重采样调用 swr_convert() */
    int nb_samples = swr_convert(swrctx_, resampled_data_, dst_nb_samples_,
                                (const uint8_t **)src_data, src_nb_samples);

    /* 计算输出字节数，用于调试或写文件 */
    int dst_bufsize = av_samples_get_buffer_size(&dst_linesize_, dst_channels_, nb_samples,
                                                params_.dst_sample_fmt, 1);

    /* dump */
    // static FILE* s_swr_fp = fopen("swr.pcm", "wb");
    // fwrite(resampled_data_[0], 1, dst_bufsize, s_swr_fp);
    // fflush(s_swr_fp);

    /* 最后把重采样得到的 pcm 数据写入 FIFO */
    return av_audio_fifo_write(audio_fifo_, (void**)resampled_data_, nb_samples);
}

/* 只是把裸 PCM 包装成一个 AVFrame 后，再调用 SendResampleFrame(AVFrame *frame)，本质也是同样流程. */
int AudioResampler::SendResampleFrame(uint8_t *data, int size)
{
    if(!data) {
        is_flushed_ = true;
        return 0;
    }
    auto frame = AVFramePtr(av_frame_alloc(), [](AVFrame *pFrame) {
        if(pFrame) av_frame_free(&pFrame);
    });

    frame->format = params_.src_sample_fmt;
    frame->channel_layout = params_.src_channel_layout;
    int ch = av_get_channel_layout_nb_channels(params_.src_channel_layout);
    frame->nb_samples = size / av_get_bytes_per_sample(params_.src_sample_fmt) / ch;
    avcodec_fill_audio_frame(frame.get(), ch, params_.src_sample_fmt, data, size, 0);
    return SendResampleFrame(frame.get());
}

AVFramePtr AudioResampler::ReceiveResampleFrame(int desired_size)
{
    std::lock_guard<std::mutex> lock(mtx_);
    desired_size = desired_size == 0 ? av_audio_fifo_size(audio_fifo_) : desired_size;
    if(av_audio_fifo_size(audio_fifo_) < desired_size || desired_size == 0)
        return {};
    return getOneFrame(desired_size);
}

/* 输出缓冲从 audio_fifo_ 中读取出来，封装到新的 AVFrame 并返回给调用者。 */
int AudioResampler::ReceiveResampledFrame(std::vector<AVFramePtr> &frames, int desired_size)
{
    std::lock_guard<std::mutex> lock(mtx_);
    int ret = 0;
    desired_size = desired_size == 0 ? av_audio_fifo_size(audio_fifo_) : desired_size;
    do
    {
        if(av_audio_fifo_size(audio_fifo_) < desired_size || desired_size == 0)
            break;
        auto frame = getOneFrame(desired_size);
        if(frame) {
            frames.push_back(frame);
        }
        else {
            ret = AVERROR(ENOMEM);
            break;
        }
    } while (true);

    /* 一旦 is_flushed 为 true，又会把返回值设置为 AVERROR_EOF 并插入一个空 Frame 作为结束标记 */
    if(is_flushed_) {
        ret = AVERROR_EOF;
        frames.push_back(AVFramePtr());
    }

    return ret;
}

AVFramePtr AudioResampler::allocOutFrame(const int nb_samples) {
    auto frame = AVFramePtr(av_frame_alloc(), [](AVFrame *pFrame) {
        if(pFrame) av_frame_free(&pFrame);
    });
    if(!frame)
    {
        LogError("av_frame_alloc failed");
        return {};
    }
    frame->nb_samples = nb_samples;
    frame->channel_layout = params_.dst_channel_layout;
    frame->format = params_.dst_sample_fmt;
    frame->sample_rate = params_.dst_sample_rate;
    int ret = av_frame_get_buffer(frame.get(), 0);
    if(ret < 0) {
        LogError("av_frame_get_buffer failed");
        return {};
    }
    return frame;
}

AVFramePtr AudioResampler::getOneFrame(const int desired_size) {
    auto frame = allocOutFrame(desired_size);
    if(frame) {
        int ret = av_audio_fifo_read(audio_fifo_, (void**)frame->data, desired_size);
        if(ret <= 0) {
            LogError("av_audio_fifo_read failed");
        }
        frame->pts = cur_pts_;
        cur_pts_ += desired_size;
        total_resampled_num_ += desired_size;
    }
    return frame;
}
