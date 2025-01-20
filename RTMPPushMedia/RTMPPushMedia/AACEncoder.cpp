#include "AACEncoder.h"
#include <iostream>

#pragma warning(disable:4996)

AACEncoder::AACEncoder() {

}

AACEncoder::~AACEncoder()
{
    if (codec_ctx_)
        avcodec_free_context(&codec_ctx_);

    if (frame_)
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
    if (!codec_) {
        std::cerr << "AAC No encoder found" << std::endl;
        return RET_ERR_MISMATCH_CODE;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
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
    if (avcodec_open2(codec_ctx_, codec_, NULL) < 0) {
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
    // ����data buf
    int ret = av_frame_get_buffer(frame_, 0);

    return RET_OK;
}

AVPacket* AACEncoder::Encode(AVFrame* frame, int64_t pts, const int flush) {
    // ��� flush != 0����ʾ�û���Ҫflush������
    // ����������⴦���� null frame �� flush��Ҳ����ֱ�Ӻ���
    // ʾ������ʾ�����������̣�����ʾ flush
    if (!frame && flush == 0) {
        return nullptr;
    }

    // ����֡����ʾʱ���(pts)��Ҳ�ɲ��裬������ʾд��
    if (frame) {
        frame->pts = pts;
    }

    // 1) ��ԭʼ���ݸ�������
    int ret = avcodec_send_frame(codec_ctx_, frame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // ��Ҫ�� receive ��send����ʾ���򵥴���
            std::cerr << "AACEncoder::Encode: avcodec_send_frame() returns EAGAIN." << std::endl;
        }
        else {
            std::cerr << "AACEncoder::Encode: avcodec_send_frame() failed." << std::endl;
        }
        return nullptr;
    }

    // 2) �ӱ�������ȡ����õ� AVPacket
    AVPacket* pkt = av_packet_alloc();
    ret = avcodec_receive_packet(codec_ctx_, pkt);
    if (ret < 0) {
        // EAGAIN ���� EOF��˵����������û�������
        av_packet_free(&pkt);
        return nullptr;
    }

    // ���سɹ��õ���pkt
    return pkt;
}

AVPacket* AACEncoder::Encode(uint8_t* pcm, uint32_t size) {
    if (!pcm || size == 0) {
        return nullptr;
    }

    // frame_->nb_samples * ÿ������С * ͨ���� => һ֡��������ֽ���
    // ���ڽ���洢(s16��)�������frame_byte_size_ = av_get_bytes_per_sample(sample_fmt)��
    // ��֡��С = frame_->nb_samples * frame_byte_size_ * channels_
    int frameDataSize = frame_->nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * codec_ctx_->channels;
    if (size < (uint32_t)frameDataSize) {
        // ����һ֡�����ݣ�ʾ���м򵥷���nullptr�������Լ�������
        return nullptr;
    }

    // ��PCM���ݿ�����frame_->data��
    // ���sample_fmt = FLTP(FFmpeg���õ�float planarģʽ)������Ҫ��ƽ�濽��
    // �����AV_SAMPLE_FMT_S16����ģʽ����ֱ�ӿ�������
    // �����Գ����Ľ���S16LE����ʾ(����Init��ָ������ AV_SAMPLE_FMT_FLTP��������ע��ƻ�������ʽ)
    // ���������ʾ����plane == 1����
    // ʵ������� sample_fmt �� channels ��ƽ��/������д
    if (codec_ctx_->sample_fmt == AV_SAMPLE_FMT_S16) {
        memcpy(frame_->data[0], pcm, frameDataSize);
    }
    else if (codec_ctx_->sample_fmt == AV_SAMPLE_FMT_FLTP) {
        // ��Ҫ���Ϊ��ƽ��
        // frame_->data[0]: �洢����ͨ���ĵ�0������, ��1������...  (������)
        // frame_->data[1]: (������)
        int nb_samples = frame_->nb_samples;
        int16_t* in = (int16_t*)pcm;
        for (int i = 0; i < nb_samples; i++) {
            // ����2ͨ��
            ((float*)frame_->data[0])[i] = (float)in[2 * i] / 32767.0f; // ������
            ((float*)frame_->data[1])[i] = (float)in[2 * i + 1] / 32767.0f; // ������
        }
    }
    else {
        // ������ʽ����������չ���������ʾ
        return nullptr;
    }

    // �͵�������
    AVPacket* pkt = nullptr;
    int ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            std::cerr << "AACEncoder::Encode(pcm): EAGAIN, need receive first." << std::endl;
        }
        else {
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


void AACEncoder::getAdtsHeader(uint8_t* adts_header, int aac_length) {
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
    adts_header[3] = (((ch_cfg & 3) << 6) + (frame_length >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
}

