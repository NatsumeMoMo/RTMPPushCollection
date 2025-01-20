#include "AACEncoder.h"
#include <iostream>

AACEncoder::AACEncoder() {

}

AACEncoder::~AACEncoder()
{
    if(codec_ctx_)
        avcodec_free_context(&codec_ctx_);

    if(frame_)
        av_frame_free(&frame_);
}

RET_CODE AACEncoder::Init(const Properties properties)
{
    sampleRate_ = properties.GetProperty("sample_rate", 48000);
    bitrate_ = properties.GetProperty("bitrate", 128 * 1024);
    channels_ = properties.GetProperty("channels", 2);
    channelLayout_ = properties.GetProperty("channel_layout", (int)av_get_default_channel_layout(channels_));

    type_ = AudioCodec::AAC;
    codec_ = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_AAC);
    if(!codec_) {
        std::cerr << "AAC No encoder found" << std::endl;
        return RET_ERR_MISMATCH_CODE;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if(!codec_ctx_) {
        std::cerr << "AAC: Could not allocate audio codec context" << std::endl;
        return RET_ERR_OUTOFMEMORY;
    }

    /* Set params */
    codec_ctx_->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_ctx_->codec_id = AV_CODEC_ID_AAC;
    codec_ctx_->profile = FF_PROFILE_AAC_LOW;

    codec_ctx_->channels = channels_;
    codec_ctx_->channel_layout = channelLayout_;
    codec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec_ctx_->sample_rate = sampleRate_;
    codec_ctx_->bit_rate = bitrate_;
    codec_ctx_->thread_count = 1;

    codec_ctx_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    if(avcodec_open2(codec_ctx_, codec_, NULL) < 0) {
        std::cerr << "AAC: Could not open codec" << std::endl;
        return RET_FAIL;
    }

    frame_byte_size_ = av_get_bytes_per_sample(codec_ctx_->sample_fmt);
    frame_ = av_frame_alloc();
    frame_->nb_samples = codec_ctx_->frame_size;
    std::cout << "Frame Sample Size: " << frame_->nb_samples << std::endl;
    frame_->format = codec_ctx_->sample_fmt;
    frame_->channel_layout = codec_ctx_->channel_layout;
    frame_->sample_rate = codec_ctx_->sample_rate;
    // 分配data buf
    int ret = av_frame_get_buffer(frame_, 0);

    return RET_OK;
}

AVPacket * AACEncoder::Encode(AVFrame *frame, int64_t pts, const int flush) {
    // 如果 flush != 0，表示用户想要flush编码器
    // 这里可以特殊处理：送 null frame 来 flush，也可以直接忽略
    // 示例仅演示正常编码流程，不演示 flush
    if (!frame && flush == 0) {
        return nullptr;
    }

    // 设置帧的显示时间戳(pts)，也可不设，这里演示写法
    if (frame) {
        frame->pts = pts;
    }

    // 1) 送原始数据给编码器
    int ret = avcodec_send_frame(codec_ctx_, frame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // 需要先 receive 再send，本示例简单处理
            std::cerr << "AACEncoder::Encode: avcodec_send_frame() returns EAGAIN." << std::endl;
        } else {
            std::cerr << "AACEncoder::Encode: avcodec_send_frame() failed." << std::endl;
        }
        return nullptr;
    }

    // 2) 从编码器获取编码好的 AVPacket
    AVPacket* pkt = av_packet_alloc();
    ret = avcodec_receive_packet(codec_ctx_, pkt);
    if (ret < 0) {
        // EAGAIN 或者 EOF，说明编码器还没输出数据
        av_packet_free(&pkt);
        return nullptr;
    }

    // 返回成功拿到的pkt
    return pkt;
}

AVPacket * AACEncoder::Encode(uint8_t *pcm, uint32_t size) {
    if (!pcm || size == 0) {
        return nullptr;
    }

    // frame_->nb_samples * 每采样大小 * 通道数 => 一帧所需的总字节数
    // 对于交错存储(s16等)的情况，frame_byte_size_ = av_get_bytes_per_sample(sample_fmt)，
    // 整帧大小 = frame_->nb_samples * frame_byte_size_ * channels_
    int frameDataSize = frame_->nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * codec_ctx_->channels;
    if (size < (uint32_t)frameDataSize) {
        // 不足一帧的数据，示例中简单返回nullptr，可以自己做缓冲
        return nullptr;
    }

    // 将PCM数据拷贝到frame_->data中
    // 如果sample_fmt = FLTP(FFmpeg常用的float planar模式)，则需要分平面拷贝
    // 如果是AV_SAMPLE_FMT_S16交错模式，则直接拷贝即可
    // 下面以常见的交错S16LE做演示(你在Init里指定的是 AV_SAMPLE_FMT_FLTP，所以请注意计划拷贝方式)
    // 这里仅做演示，以plane == 1举例
    // 实际需根据 sample_fmt 和 channels 分平面/交错来写
    if (codec_ctx_->sample_fmt == AV_SAMPLE_FMT_S16) {
        memcpy(frame_->data[0], pcm, frameDataSize);
    } else if (codec_ctx_->sample_fmt == AV_SAMPLE_FMT_FLTP) {
        // 需要拆分为多平面
        // frame_->data[0]: 存储所有通道的第0个采样, 第1个采样...  (左声道)
        // frame_->data[1]: (右声道)
        int nb_samples = frame_->nb_samples;
        int16_t* in = (int16_t*)pcm;
        for (int i = 0; i < nb_samples; i++) {
            // 假设2通道
            ((float*)frame_->data[0])[i] = (float)in[2*i]     / 32767.0f; // 左声道
            ((float*)frame_->data[1])[i] = (float)in[2*i + 1] / 32767.0f; // 右声道
        }
    } else {
        // 其他格式可以再行扩展，这里仅演示
        return nullptr;
    }

    // 送到编码器
    AVPacket* pkt = nullptr;
    int ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            std::cerr << "AACEncoder::Encode(pcm): EAGAIN, need receive first." << std::endl;
        } else {
            std::cerr << "AACEncoder::Encode(pcm): send_frame failed." << std::endl;
        }
        return nullptr;
    }

    pkt = av_packet_alloc();
    ret = avcodec_receive_packet(codec_ctx_, pkt);
    if (ret < 0) {
        av_packet_free(&pkt);
        return nullptr;
    }

    return pkt;
}

uint8_t AACEncoder::GetSampleRateIndex(int sampleRate) {
    uint8_t freqIdx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (sampleRate)
    {
        case 96000: freqIdx = 0; break;
        case 88200: freqIdx = 1; break;
        case 64000: freqIdx = 2; break;
        case 48000: freqIdx = 3; break;
        case 44100: freqIdx = 4; break;
        case 32000: freqIdx = 5; break;
        case 24000: freqIdx = 6; break;
        case 22050: freqIdx = 7; break;
        case 16000: freqIdx = 8; break;
        case 12000: freqIdx = 9; break;
        case 11025: freqIdx = 10; break;
        case 8000: freqIdx = 11; break;
        case 7350: freqIdx = 12; break;
        default:
            std::cerr << "can't support sample_rate" << std::endl;
        freqIdx = 4;
        break;
    }

    return freqIdx;
}


void AACEncoder::getAdtsHeader(uint8_t *adts_header, int aac_length) {
    uint8_t freqIdx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (codec_ctx_->sample_rate)
    {
        case 96000: freqIdx = 0; break;
        case 88200: freqIdx = 1; break;
        case 64000: freqIdx = 2; break;
        case 48000: freqIdx = 3; break;
        case 44100: freqIdx = 4; break;
        case 32000: freqIdx = 5; break;
        case 24000: freqIdx = 6; break;
        case 22050: freqIdx = 7; break;
        case 16000: freqIdx = 8; break;
        case 12000: freqIdx = 9; break;
        case 11025: freqIdx = 10; break;
        case 8000: freqIdx = 11; break;
        case 7350: freqIdx = 12; break;
        default:
            std::cerr << "can't support sample_rate" << std::endl;
        freqIdx = 4;
        break;
    }
    uint8_t ch_cfg = codec_ctx_->channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((codec_ctx_->profile) << 6) + (freqIdx << 2) + (ch_cfg >> 2);
    adts_header[3] = (((ch_cfg & 3) << 6) + (frame_length  >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
}

