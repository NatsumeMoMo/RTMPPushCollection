#include "PushWork.h"

#include "AVTimeBase.h"
#include "dlog.h"

#include <iostream>

using namespace std::placeholders;

PushWork::PushWork() {

}

PushWork::~PushWork() {
    DeInit();
}

RET_CODE PushWork::Init(const Properties& properties)
{
    audio_test_ = properties.GetProperty("audio_test", 0);
    pcmInName_ = properties.GetProperty("input_pcm_name", "48000_2_s16le.pcm");

    micSampleRate_ = properties.GetProperty("mic_sample_rate", 48000);
    micSampleFmt_ = properties.GetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    micChannels_ = properties.GetProperty("mic_channels", 2);

    audio_sampleRate_ = properties.GetProperty("audio_sample_rate", 48000);
    audio_bitrate_ = properties.GetProperty("audio_bitrate", 128 * 1024);
    audio_channels_ = properties.GetProperty("audio_channels", micChannels_);
    audio_ch_layout_ = av_get_default_channel_layout(audio_channels_);

    video_test_ = properties.GetProperty("video_test", 0);
    yuvInName_ = properties.GetProperty("input_yuv_name", "720x480_25fps_420p.yuv");

    desktop_x_ = properties.GetProperty("desktop_x", 0);
    desktop_y_ = properties.GetProperty("desktop_y", 0);
    desktop_width_ = properties.GetProperty("desktop_width", 1920);
    desktop_height_ = properties.GetProperty("desktop_height", 1080);
    desktop_fmt_ = properties.GetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
    desktop_fps_ = properties.GetProperty("desktop_fps", 25);

    video_width_ = properties.GetProperty("video_width", desktop_width_);
    video_height_ = properties.GetProperty("video_height", desktop_height_);
    video_fps_ = properties.GetProperty("video_fps", desktop_fps_);
    video_gop_ = properties.GetProperty("video_gop", video_fps_);
    video_bitrate_ = properties.GetProperty("video_bitrate", 1024 * 1024);
    video_b_frames_ = properties.GetProperty("video_b_frames", 0);

    rtmp_url_ = properties.GetProperty("rtmp_url", "");
    if(rtmp_url_ == "")
    {
        return RET_FAIL;
    }
    rtmp_debug_ = properties.GetProperty("rtmp_debug", 0);

    // SDL
    if(1 == rtmp_debug_)
    {
        video_out_sdl_ = new VideoOutSDL();
        if(!video_out_sdl_)
        {
            return RET_FAIL;
        }
        Properties yuv_properties;
        yuv_properties.SetProperty("video_width", desktop_width_);
        yuv_properties.SetProperty("video_height", desktop_height_);
        yuv_properties.SetProperty("win_x", 200);
        yuv_properties.SetProperty("win_title", "Push Video Display");
        if(video_out_sdl_->Init(yuv_properties) != RET_OK)
        {
            LogError("video_out_sdl Init failed");
            return RET_FAIL;
        }
    }

    /* RTMPPusher Init */
    pusher_ = new RTMPPusher();
    if(!pusher_)
    {
        LogError("new RTMPPusher() failed");
        return RET_FAIL;
    }

    if(!pusher_->GetRTMPBase()->Connect(rtmp_url_)) {
        LogError("rtmp_pusher connect() failed");
        return RET_FAIL;
    }

    // 初始化Publish time
    AVPublishTime::GetInstance()->Reset();

    /* AAC Encoder Init */
    audio_encoder_ = new AACEncoder();
    if(!audio_encoder_) {
        LogError("new AACEncoder() failed");
        return RET_FAIL;
    }
    Properties aud_codec_properties;
    aud_codec_properties.SetProperty("sample_rate", audio_sampleRate_);
    aud_codec_properties.SetProperty("channels", audio_channels_);
    aud_codec_properties.SetProperty("bitrate", audio_bitrate_);
    if(audio_encoder_->Init(aud_codec_properties) != RET_OK) {
        LogError("audio_encoder_ Init failed");
        return RET_FAIL;
    }
    aac_buf_ = new uint8_t[AAC_BUF_MAX_LENGTH];
    fopen_s(&aac_fp_, "push_dump.aac", "wb");
    if(!aac_fp_) {
        LogError("push_dump.aac open failed");
        return RET_FAIL;
    }

    /* Audio Resampler Init */
    audio_resampler_ = new AudioResampler();
    AudioResampleParams aud_params;
    aud_params.logtag = "[audio-resample]";
    aud_params.src_sample_fmt = (AVSampleFormat)micSampleFmt_;
    aud_params.dst_sample_fmt = (AVSampleFormat)audio_encoder_->getSampleFormat();
    aud_params.src_sample_rate = micSampleRate_;
    aud_params.dst_sample_rate = audio_encoder_->getSampleRate();
    aud_params.src_channel_layout = av_get_default_channel_layout(micChannels_);
    aud_params.dst_channel_layout = audio_encoder_->getChannelLayout();
    aud_params.logtag = "audio-resample-encode";
    audio_resampler_->InitResampler(aud_params);

    /* H264 Enocder Init */
    video_encoder_ = new H264Encoder();
    Properties  vid_codec_properties;
    vid_codec_properties.SetProperty("width", video_width_);
    vid_codec_properties.SetProperty("height", video_height_);
    vid_codec_properties.SetProperty("fps", video_fps_);
    vid_codec_properties.SetProperty("b_frames", video_b_frames_);
    vid_codec_properties.SetProperty("bitrate", video_bitrate_);
    vid_codec_properties.SetProperty("gop", video_gop_);
    if(video_encoder_->Init(vid_codec_properties) != RET_OK)
    {
        LogError("H264Encoder Init failed");
        return RET_FAIL;
    }
    fopen_s(&h264_fp_, "push_dump.h264", "wb");
    if(!h264_fp_) {
        LogError("push_dump.h264 open failed");
        return RET_FAIL;
    }


    uint8_t spsData[1024] = { 0 };
    uint8_t ppsData[1024] = { 0 };
    int spsLen = video_encoder_->getSPSSize();
    int ppsLen = video_encoder_->getPPSSize();
    if(spsLen > 0)
        memcpy(spsData, video_encoder_->getSPSData(), spsLen);
    if(ppsLen > 0)
        memcpy(ppsData, video_encoder_->getPPSData(), ppsLen);

    RTMPMetadata* metadata = new RTMPMetadata();
    memset(metadata, 0, sizeof(RTMPMetadata));
    /* 视频参数 */
    metadata->bHasVideo = true;
    metadata->nSpsLen = spsLen;
    memcpy(metadata->Sps, spsData, spsLen);
    metadata->nPpsLen = ppsLen;
    memcpy(metadata->Pps, ppsData, ppsLen);
    metadata->nWidth = video_encoder_->getWidth();
    metadata->nHeight = video_encoder_->getHeight();
    metadata->nFrameRate = 25;
    metadata->nVideoDataRate = 800 * 1024;

    /* 设置音频参数 */
    metadata->bHasAudio = true;
    metadata->nAudioSampleRate = audio_encoder_->getSampleRate();
    metadata->nAudioSampleSize = 16;
    metadata->nAudioChannels = audio_encoder_->getChannels();
    metadata->nAudioSpecCfgLen = 2;
    // metadata->AudioSpecCfg[0] = ((audio_encoder_->getCodecCtx()->profile & 0x07) << 3) |
    //     ((AACEncoder::GetSampleRateIndex(audio_encoder_->getSampleRate()) & 0x0F) >> 1 );
    // metadata->AudioSpecCfg[1] = ((AACEncoder::GetSampleRateIndex(audio_encoder_->getSampleRate()) & 0x01) << 7) |
    //     ((audio_encoder_->getCodecCtx()->channels & 0x0F) << 3);

    metadata->AudioSpecCfg[0] = 0x11;  // 第 2 字节为 profile(2bit) + freqIdx(4bit) + 第1bit通道
    metadata->AudioSpecCfg[1] = 0x90;  // 剩余bit + frameLen一部分，这里只保留通道信息相关bit

    pusher_->SendMetaData(metadata);

    /* 设置音频PTS间隔 */
    double audio_frame_duration = 1000.0 / audio_encoder_->getSampleRate() * audio_encoder_->getFrameSampleSize();
    LogInfo("audio frame duration: %f ms", audio_frame_duration);
    AVPublishTime::GetInstance()->SetAudioFrameDuration(audio_frame_duration);
    AVPublishTime::GetInstance()->SetAudioPtsStrategy(AVPublishTime::PTS_RECTIFY); // 帧间隔校正

    /* 设置AudioCapture */
    audio_capture_ = new AudioCapture();
    Properties audio_properties;
    audio_properties.SetProperty("audio_test", 1);
    audio_properties.SetProperty("input_pcm_name", pcmInName_);
    if(audio_capture_->Init(audio_properties) != RET_OK) {
        LogError("audio_capture Init failed");
        return RET_FAIL;
    }
    audio_capture_->setCallback(std::bind(&PushWork::PcmCallback, this, _1, _2, _3));
    if(audio_capture_->Start() != RET_OK) {
        LogError("audio_capture Start failed");
        return RET_FAIL;
    }

    /* 设置视频PTS间隔 */
    double video_frame_duration = 1000.0 / video_encoder_->getFrameRate();
    LogInfo("video frame duration: %f ms", video_frame_duration);
    AVPublishTime::GetInstance()->SetVideoPtsStrategy(AVPublishTime::PTS_RECTIFY); // 帧间隔校正

    /* 设置VideoCapture */
    video_capture_ = new VideoCapture();
    Properties video_properties;
    video_properties.SetProperty("video_test", 1);
    video_properties.SetProperty("input_yuv_name", yuvInName_);
    video_properties.SetProperty("width", desktop_width_);
    video_properties.SetProperty("height", desktop_height_);
    if(video_capture_->Init(video_properties) != RET_OK) {
        LogError("video_capture Init failed");
        return RET_FAIL;
    }
    video_nalu_buf_ = new uint8_t[VIDEO_NALU_BUF_MAX_SIZE];
    video_capture_->setCallback(std::bind(&PushWork::YuvCallback, this, _1, _2, _3));
    if(video_capture_->Start() != RET_OK) {
        LogError("video_capture Start failed");
        return RET_FAIL;
    }

    return RET_OK;
}

void PushWork::DeInit()
{
    if(audio_capture_) {
        delete audio_capture_;
        audio_capture_ = nullptr;
    }

    if(video_capture_) {
        delete video_capture_;
        video_capture_ = nullptr;
    }

    if(video_out_sdl_) {
        delete video_out_sdl_;
        video_out_sdl_ = nullptr;
    }

    if(video_encoder_) {
        delete video_encoder_;
        video_encoder_ = nullptr;
    }

    if(audio_encoder_) {
        delete audio_encoder_;
        audio_encoder_ = nullptr;
    }

    if(audio_resampler_) {
        delete audio_resampler_;
        audio_resampler_ = nullptr;
    }

    if(pusher_) {
        delete pusher_;
        pusher_ = nullptr;
    }

    if(h264_fp_) {
        delete h264_fp_;
        h264_fp_ = nullptr;
    }

    if(aac_fp_) {
        delete aac_fp_;
        aac_fp_ = nullptr;
    }

    if(pcm_flt_fp_) {
        delete pcm_flt_fp_;
        pcm_flt_fp_ = nullptr;
    }

    if(pcm_s16le_fp_) {
        delete pcm_s16le_fp_;
        pcm_s16le_fp_ = nullptr;
    }
}

void PushWork::PcmCallback(uint8_t *pcm, int32_t size, unsigned int timestamp)
{
    // TODO: 重采样

    AVPacket* pkt = audio_encoder_->Encode(pcm, size);
    if(!pkt) {
        // LogError("audio_encoder_->Encode failed");
        return;
    }
    AudioRawMsg aMsg(pkt->data, pkt->size);
    aMsg.nTimestamp = timestamp;
    pusher_->EnqueueAudio(aMsg);
    av_packet_free(&pkt);
}

void PushWork::YuvCallback(uint8_t *yuv, int32_t size, unsigned int timestamp)
{
    if(video_out_sdl_)
        video_out_sdl_->Cache(yuv, size);

    /* 对YUV数据进行编码, 然后将其投递到队列中 */
    AVPacket* avpkt = video_encoder_->Encode(yuv, 0);
    if(!avpkt) {
        LogError("video_encoder_->Encode failed");
        return;
    }
    //     拆分AVPacket->data 中的 NALU，并逐个发送
    //     简化做法：一般情况下 FFmpeg 默认会在 packet->data 里放一个或多个NALU
    //     常见情况：NALU 前面带有 00 00 00 01 startcode
    //     我们可以直接调用 rtmpSender.SendH264Packet() 发送这一整块，但需判断关键帧
    //     这里用简单方法：若 nal_unit_type == 5 => keyframe
    bool bIsKeyFrame = false;
    if(avpkt->flags & AV_PKT_FLAG_KEY)
        bIsKeyFrame = true;

    // RTMP 推流时，需要去掉多余的 00 00 00 01 start code 头等
    // FFmpeg输出的packet带 startcode，则简单跳过头
    // 具体视 encoder 里 avcodec_send_frame/ avcodec_receive_packet 的实现
    int skipBytes = 0;
    if (avpkt->size > 4 &&  avpkt->data[0] == 0x00 &&
                            avpkt->data[1] == 0x00 &&
                            avpkt->data[2] == 0x00 &&
                            avpkt->data[3] == 0x01)
    {
        skipBytes = 4; // 跳过startcode
    }

    VideoRawMsg vMsg(avpkt->data + skipBytes, avpkt->size - skipBytes);
    vMsg.bIsKeyFrame = bIsKeyFrame;
    vMsg.nTimestamp = timestamp;
    // vMsg.nTimestamp = AVPublishTime::GetInstance()->GetVideoPts();
    pusher_->EnqueueVideo(vMsg);
    av_packet_free(&avpkt);
}

void PushWork::Loop()
{
    if(video_out_sdl_)
        video_out_sdl_->Loop();
}
