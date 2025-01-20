#include "H264Encoder.h"
#include "dlog.h"

#pragma warning(disable:4576) // for (AVRational){ 1, fps_ };

H264Encoder::H264Encoder() {

}

H264Encoder::~H264Encoder() {
    if(codec_ctx_)
        avcodec_close(codec_ctx_);
    if(frame_)
        av_free(frame_);
    if(picBuf_)
        av_free(picBuf_);
    av_packet_unref(&pkt_);
}

int H264Encoder::Init(const Properties &properties) {
    width_ = properties.GetProperty("width", 0);
    if(width_ == 0 || width_ % 2 != 0) {
        LogError("width : %d", width_);
        return RET_ERR_NOT_SUPPORT;
    }

    height_ = properties.GetProperty("height", 0);
    if(height_ == 0 || height_ % 2 != 0) {
        LogError("height : %d", height_);
        return RET_ERR_NOT_SUPPORT;
    }

    fps_ = properties.GetProperty("fps", 25);
    bFrames_ = properties.GetProperty("b_frames", 0);
    bitrate_ = properties.GetProperty("bitrate", 500 * 1024);
    gop_ = properties.GetProperty("gop", fps_);

    codec_ = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_H264);
    if(codec_ == nullptr) {
        LogError("avcodec_find_encoder failed");
    }

    count = 0;
    framecnt_ = 0;
    codec_ctx_ = avcodec_alloc_context3(codec_);

    /* 最大和最小量化系数, 取值范围为 0~51 */
    codec_ctx_->qmin = 10;
    codec_ctx_->qmax = 31;

    codec_ctx_->width = width_;
    codec_ctx_->height = height_;

    codec_ctx_->bit_rate = bitrate_;

    /* 每一组GOP中插入一个I帧 */
    codec_ctx_->gop_size = gop_;

    /* 帧率的基本单位，time_base.num为时间线分子，time_base.den为时间线分母，帧率=分子/分母 */
    codec_ctx_->time_base = (AVRational){ 1, fps_ };

    codec_ctx_->framerate.num = fps_;
    codec_ctx_->framerate.den = 1;

    /* 图像色彩空间的格式, 采用什么样的色彩空间来表明一个像素点 */
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    /* 编码器编码的数据类型 */
    codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;

    /* Optional Param */
    // 两个非B帧之间允许出现的B帧数, 设置为0表示不使用B帧, 没有编码延时. B帧越多, 压缩率越高
    codec_ctx_->max_b_frames = bFrames_;
    if(codec_ctx_->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param_, "preset", "ultrafast", 0);
        av_dict_set(&param_, "tune", "zerolatency", 0);
    }
    if(codec_ctx_->codec_id == AV_CODEC_ID_H265) {
        av_dict_set(&param_, "preset", "ultrafast", 0);
        av_dict_set(&param_, "tune", "zerolatency", 0);
    }
    codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // extradata拷贝 sps pps

    /* 初始化音视频编码器的AVCodecContext */
    if(avcodec_open2(codec_ctx_, codec_, &param_) < 0) {
        LogError("avcodec_open2 failed");
    }

    /* 读取 SPS PPS 信息 */
    if(codec_ctx_->extradata)
    {
        LogWarn("extradata size : %d", codec_ctx_->extradata_size);
        // 第一个为SPS 7
        // 第二个为PPS 8

        uint8_t* sps = codec_ctx_->extradata + 4; // 直接跳过StartCode到数据
        int sps_len = 0;
        uint8_t* pps = nullptr;
        int pps_len = 0;
        uint8_t* data = codec_ctx_->extradata + 4;
        for(int i = 0; i < codec_ctx_->extradata_size - 4; i++)
        {
            if(0 == data[i] && 0 == data[i + 1] && 0 == data[i + 2] && 1 == data[i + 3])
            {
                pps = &data[i + 4];
                break;
            }
        }
        sps_len = int(pps - sps) - 4;       // 4是 00 00 00 01占用的字节
        pps_len = codec_ctx_->extradata_size - 4 * 2 - sps_len;
        sps_.append(sps, pps + sps_len);
        pps_.append(pps, pps + pps_len);
    }

    /* Init Frame */
    // avpicture_get_size() 和 avpicture_fill() 在FFmpeg6.1中已不存在.
    // 取而代之的是av_image_get_buffer_size() 和 av_image_fill_arrays()
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
// avcodec_encode_video2() 已弃用, 现改为 avcodec_send_frame()/avcodec_receive_packet()

int H264Encoder::Encode(uint8_t *data, int in_samples, uint8_t *out, int &out_size) {
    // 1) 填充 frame_ 的数据指针
    frame_->data[0] = data;                         // Y
    frame_->data[1] = data + data_size_;            // U
    frame_->data[2] = data + data_size_ * 5 / 4;    // V

    // 2) 设置 PTS（可根据实际需求调整）
    frame_->pts = (count++) * (codec_ctx_->time_base.den) / ((codec_ctx_->time_base.num) * 25);

    // 3) 送入编码器
    int ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        LogError("avcodec_send_frame() failed: %d", ret);
        return -1;
    }

    // 4) 从编码器读取编码结果
    //    这里每次调用都需分配一个新的 AVPacket
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        LogError("av_packet_alloc() failed");
        return -1;
    }

    ret = avcodec_receive_packet(codec_ctx_, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // 暂无可取的输出包
        av_packet_free(&pkt);
        return -1;
    } else if (ret < 0) {
        LogError("avcodec_receive_packet() error: %d", ret);
        av_packet_free(&pkt);
        return -1;
    }

    // 5) 如有需要，可以跳过 00 00 00 01 (startcode) 等
    //    这里保持原逻辑：unconditionally 跳过前 4 字节
    if (pkt->size > 4) {
        memcpy(out, pkt->data + 4, pkt->size - 4);
        out_size = pkt->size - 4;
    } else {
        out_size = 0;
    }

    // 6) 释放 packet（或后续自行处理完再释放）
    av_packet_free(&pkt);

    // 计数
    return 0;
}

int H264Encoder::Encode(AVFrame *frame, uint8_t *out, int &out_size)
{
    return 0;
}

AVPacket * H264Encoder::Encode(uint8_t *yuv, uint64_t pts, const int flush)
{
    // 1) 填充 frame_
    frame_->data[0] = yuv;
    frame_->data[1] = yuv + data_size_;
    frame_->data[2] = yuv + data_size_ * 5 / 4;

    // 2) PTS 处理
    if (pts != 0)
        frame_->pts = pts;
    else
        frame_->pts = pts_++;

    // 3) 送入编码器
    int ret = avcodec_send_frame(codec_ctx_, frame_);
    if (ret < 0) {
        LogError("avcodec_send_frame() failed: %d", ret);
        return nullptr;
    }

    // 4) 分配并接收输出包
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LogError("av_packet_alloc() failed");
        return nullptr;
    }

    ret = avcodec_receive_packet(codec_ctx_, packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_packet_free(&packet);
        return nullptr;
    } else if (ret < 0) {
        LogError("avcodec_receive_packet() failed: %d", ret);
        av_packet_free(&packet);
        return nullptr;
    }

    // 5) 直接返回 AVPacket* 供上层使用
    return packet;
}





