#pragma once

#include "mediabase.h"
#include "AACEncoder.h"
#include "H264Encoder.h"
#include "RTMPPusher.h"
#include "Capture/AudioCapture.h"
#include "Capture/VideoCapture.h"
#include "Resampler/AudioResampler.h"
#include "UI/VideoOutSDL.h"

class PushWork
{
public:
	PushWork();
	~PushWork();

	RET_CODE Init(const Properties& properties);
	void DeInit();

	void PcmCallback(uint8_t* pcm, int32_t size, unsigned int timestamp);
	void YuvCallback(uint8_t* yuv, int32_t size, unsigned int timestamp);

	void Loop();

private:
	/* 音频test模式 */
	int audio_test_ = 0;
	std::string pcmInName_;

	/* 麦克风采用属性 */
	int micSampleRate_ = 48000;
	int micSampleFmt_ = AV_SAMPLE_FMT_S16;
	int micChannels_ = 2;

	/* 音频编码参数 */
	int audio_sampleRate_ = AV_SAMPLE_FMT_S16;
	int audio_bitrate_ = 128 * 1024;
	int audio_channels_ = 2;
	int audio_sample_fmt_; // 由具体的编码器决定
	int audio_ch_layout_; // 由 audio_channels_ 决定

	/* 视频test模式 */
	int video_test_ = 0;
	std::string yuvInName_;

	/* 桌面录制 */
	int desktop_x_ = 0;
	int desktop_y_ = 0;
	int desktop_width_;
	int desktop_height_;
	int desktop_fps_;
	int desktop_fmt_ = AV_PIX_FMT_YUV420P;

	/* 视频编码属性 */
	int video_width_ = 1920;
	int video_height_ = 1080;
	int video_fps_;
	int video_gop_;
	int video_bitrate_;
	int video_b_frames_;

	/* rtmp推流 */
	std::string rtmp_url_;
	int rtmp_debug_;
	RTMPPusher* pusher_;
	bool need_send_audio_sepc_config = true;
	bool need_send_video_config = true;

	// 显示推流画面时使用
	VideoOutSDL* video_out_sdl_ = nullptr;

	/* Audio */
	AudioCapture* audio_capture_ = nullptr;
	AudioResampler* audio_resampler_ = nullptr;
	AACEncoder* audio_encoder_ = nullptr;
	uint8_t *aac_buf_ = nullptr;
	const int AAC_BUF_MAX_LENGTH = 8291 + 64; // 最大为13bit长度(8191), +64 只是防止字节对齐

	/* Video */
	VideoCapture* video_capture_ = nullptr;
	H264Encoder* video_encoder_ = nullptr;
	uint8_t* video_nalu_buf_ = nullptr;
	int video_nalu_size_ = 0;
	const int VIDEO_NALU_BUF_MAX_SIZE = 1024 * 1024;

	/* Dump H264 File */
	FILE* h264_fp_ = nullptr;
	FILE* aac_fp_ = nullptr;
	FILE* pcm_flt_fp_ = nullptr;
	FILE* pcm_s16le_fp_ = nullptr;
};