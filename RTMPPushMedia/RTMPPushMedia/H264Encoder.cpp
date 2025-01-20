#include "H264Encoder.h"

#include <iostream>

#pragma warning(disable:4996)

#pragma warning(disable:4576) // for (AVRational){ 1, fps_ };

H264Encoder::H264Encoder() {

}

H264Encoder::~H264Encoder() {
    if (codec_ctx_)
        avcodec_close(codec_ctx_);
    if (frame_)
        av_free(frame_);
    if (picBuf_)
        av_free(picBuf_);
    av_packet_unref(&pkt_);
}

int H264Encoder::Init(const Properties& properties) {
    width_ = properties.GetProperty("width", 0);
    if (width_ == 0 || width_ % 2 != 0) {
        // LogError("width : %d", width_);
        std::cerr << "width : " << width_ << std::endl;
        return RET_ERR_NOT_SUPPORT;
    }

    height_ = properties.GetProperty("height", 0);
    if (height_ == 0 || height_ % 2 != 0) {
        // LogError("height : %d", height_);
        std::cerr << "height : " << height_ << std::endl;
        return RET_ERR_NOT_SUPPORT;
    }

    fps_ = properties.GetProperty("fps", 25);
    bFrames_ = properties.GetProperty("b_frames", 0);
    bitrate_ = properties.GetProperty("bitrate", 500 * 1024);
    gop_ = properties.GetProperty("gop", fps_);

    codec_ = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_H264);
    if (codec_ == nullptr) {
        // LogError("avcodec_find_encoder failed");
        std::cerr << "avcodec_find_encoder failed" << std::endl;
    }

    count = 0;
    framecnt_ = 0;
    codec_ctx_ = avcodec_alloc_context3(codec_);

    /* ������С����ϵ��, ȡֵ��ΧΪ 0~51 */
    codec_ctx_->qmin = 10;
    codec_ctx_->qmax = 31;

    codec_ctx_->width = width_;
    codec_ctx_->height = height_;

    codec_ctx_->bit_rate = bitrate_;

    /* ÿһ��GOP�в���һ��I֡ */
    codec_ctx_->gop_size = gop_;

    /* ֡�ʵĻ�����λ��time_base.numΪʱ���߷��ӣ�time_base.denΪʱ���߷�ĸ��֡��=����/��ĸ */
    codec_ctx_->time_base = (AVRational){ 1, fps_ };

    codec_ctx_->framerate.num = fps_;
    codec_ctx_->framerate.den = 1;

    /* ͼ��ɫ�ʿռ�ĸ�ʽ, ����ʲô����ɫ�ʿռ�������һ�����ص� */
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    /* ������������������� */
    codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;

    /* Optional Param */
    // ������B֮֡��������ֵ�B֡��, ����Ϊ0��ʾ��ʹ��B֡, û�б�����ʱ. B֡Խ��, ѹ����Խ��
    codec_ctx_->max_b_frames = bFrames_;
    if (codec_ctx_->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param_, "preset", "ultrafast", 0);
        av_dict_set(&param_, "tune", "zerolatency", 0);
    }
    if (codec_ctx_->codec_id == AV_CODEC_ID_H265) {
        av_dict_set(&param_, "preset", "ultrafast", 0);
        av_dict_set(&param_, "tune", "zerolatency", 0);
    }
    codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // extradata���� sps pps

    /* ��ʼ������Ƶ��������AVCodecContext */
    if (avcodec_open2(codec_ctx_, codec_, &param_) < 0) {
        std::cout << "avcodec_open2 failed" << std::endl;
    }

    /* ��ȡ SPS PPS ��Ϣ */
    if (codec_ctx_->extradata)
    {
        // LogWarn("extradata size : %d", codec_ctx_->extradata_size);
        std::cout << "extradata size : " << codec_ctx_->extradata_size << std::endl;
        // ��һ��ΪSPS 7
        // �ڶ���ΪPPS 8

        uint8_t* sps = codec_ctx_->extradata + 4; // ֱ������StartCode������
        int sps_len = 0;
        uint8_t* pps = nullptr;
        int pps_len = 0;
        uint8_t* data = codec_ctx_->extradata + 4;
        for (int i = 0; i < codec_ctx_->extradata_size - 4; i++)
        {
            if (0 == data[i] && 0 == data[i + 1] && 0 == data[i + 2] && 1 == data[i + 3])
            {
                pps = &data[i + 4];
                break;
            }
        }
        sps_len = int(pps - sps) - 4;       // 4�� 00 00 00 01ռ�õ��ֽ�
        pps_len = codec_ctx_->extradata_size - 4 * 2 - sps_len;
        sps_.append(sps, pps + sps_len);
        pps_.append(pps, pps + pps_len);
    }

    /* Init Frame */
    // avpicture_get_size() �� avpicture_fill() ��FFmpeg6.1���Ѳ�����.
    // ȡ����֮����av_image_get_buffer_size() �� av_image_fill_arrays()
    frame_ = av_frame_alloc();
    int pictureSize = av_image_get_buffer_size(codec_ctx_->pix_fmt, codec_ctx_->width, codec_ctx_->height, 1);
    picBuf_ = (uint8_t*)av_malloc(pictureSize);
    int ret = av_image_fill_arrays(frame_->data, frame_->linesize, picBuf_, codec_ctx_->pix_fmt,
        codec_ctx_->width, codec_ctx_->height, 1);
    frame_->width = codec_ctx_->width;
    frame_->height = codec_ctx_->height;
    frame_->format = codec_ctx_->pix_fmt;

    /* Init packet */
    av_new_packet(&pkt_, pictureSize);
    data_size_ = codec_ctx_->width * codec_ctx_->height;

    return 0;
}

/* Encode */
// avcodec_encode_video2() ������, �ָ�Ϊ avcodec_send_frame()/avcodec_receive_packet()

int H264Encoder::Encode(uint8_t* data, int in_samples, uint8_t* out, int& out_size) {
    // 1) ��� frame_ ������ָ��
    frame_->data[0] = data;                         // Y
    frame_->data[1] = data + data_size_;            // U
    frame_->data[2] = data + data_size_ * 5 / 4;    // V

    // 2) ���� PTS���ɸ���ʵ�����������
    frame_->pts = (count++) * (codec_ctx_->time_base.den) / ((codec_ctx_->time_base.num) * 25);

    // 3) ���������
    int ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        // LogError("avcodec_send_frame() failed: %d", ret);
        std::cout << "avcodec_send_frame() failed" << std::endl;
        return -1;
    }

    // 4) �ӱ�������ȡ������
    //    ����ÿ�ε��ö������һ���µ� AVPacket
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        // LogError("av_packet_alloc() failed");
        std::cout << "av_packet_alloc() failed" << std::endl;
        return -1;
    }

    ret = avcodec_receive_packet(codec_ctx_, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // ���޿�ȡ�������
        av_packet_free(&pkt);
        return -1;
    }
    else if (ret < 0) {
        // LogError("avcodec_receive_packet() error: %d", ret);
        std::cout << "avcodec_receive_packet() failed" << std::endl;
        av_packet_free(&pkt);
        return -1;
    }

    // 5) ������Ҫ���������� 00 00 00 01 (startcode) ��
    //    ���ﱣ��ԭ�߼���unconditionally ����ǰ 4 �ֽ�
    if (pkt->size > 4) {
        memcpy(out, pkt->data + 4, pkt->size - 4);
        out_size = pkt->size - 4;
    }
    else {
        out_size = 0;
    }

    // 6) �ͷ� packet����������д��������ͷţ�
    av_packet_free(&pkt);

    // ����
    return 0;
}

int H264Encoder::Encode(AVFrame* frame, uint8_t* out, int& out_size)
{
    return 0;
}

AVPacket* H264Encoder::Encode(uint8_t* yuv, uint64_t pts, const int flush)
{
    // 1) ��� frame_
    frame_->data[0] = yuv;
    frame_->data[1] = yuv + data_size_;
    frame_->data[2] = yuv + data_size_ * 5 / 4;

    // 2) PTS ����
    if (pts != 0)
        frame_->pts = pts;
    else
        frame_->pts = pts_++;

    // 3) ���������
    int ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        // LogError("avcodec_send_frame() failed: %d", ret);
        std::cout << "avcodec_send_frame() failed" << std::endl;
        return nullptr;
    }

    // 4) ���䲢���������
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        // LogError("av_packet_alloc() failed");
        std::cout << "av_packet_alloc() failed" << std::endl;
        return nullptr;
    }

    ret = avcodec_receive_packet(codec_ctx_, packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_packet_free(&packet);
        return nullptr;
    }
    else if (ret < 0) {
        // LogError("avcodec_receive_packet() failed: %d", ret);
        std::cout << "avcodec_receive_packet() failed" << std::endl;
        av_packet_free(&packet);
        return nullptr;
    }

    // 5) ֱ�ӷ��� AVPacket* ���ϲ�ʹ��
    return packet;
}





