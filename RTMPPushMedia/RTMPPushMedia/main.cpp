#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "RTMPStream.h"   // 你的CRTMPStream类
#include "H264Encoder.h"  
#include "AACEncoder.h"
#include "mediabase.h"    // 你的Properties等声明
#include "codecs.h"       // 你的AudioCodec等FFmpeg相关声明

// -------------------- 共享数据结构、队列及同步标志 --------------------
struct EncodedFrame {
    bool bKeyFrame;         // 对于视频有效，音频可忽略
    unsigned int timestamp; // 毫秒
    std::vector<uint8_t> data;
};

// 视频数据队列
static std::queue<EncodedFrame> g_videoQueue;
static std::mutex g_videoMutex;
static std::condition_variable g_videoCond;
static bool g_videoDone = false;  // 视频线程是否结束

// 音频数据队列
static std::queue<EncodedFrame> g_audioQueue;
static std::mutex g_audioMutex;
static std::condition_variable g_audioCond;
static bool g_audioDone = false;  // 音频线程是否结束

//--------------------------------------------------------
// 视频采集线程：读取 YUV -> H264 编码 -> 存入队列
//--------------------------------------------------------
void VideoCaptureThread(H264Encoder* h264Enc,
    const std::string& yuvFilePath,
    int fps)
{
    // 打开YUV文件
    FILE* fp_yuv = fopen(yuvFilePath.c_str(), "rb");
    if (!fp_yuv) {
        std::cerr << "[VideoThread] Failed to open YUV file: " << yuvFilePath << std::endl;
        return;
    }
    std::cout << "[VideoThread] YUV file opened: " << yuvFilePath << std::endl;

    // 获取编码器信息
    int width = h264Enc->getWidth();
    int height = h264Enc->getHeight();
    int frameSize = width * height * 3 / 2; // YUV420P
    uint8_t* yuvBuffer = new uint8_t[frameSize];

    unsigned int videoTimestamp = 0; // 毫秒
    auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        // 读取一帧 YUV
        size_t ret = fread(yuvBuffer, 1, frameSize, fp_yuv);
        if (ret < (size_t)frameSize) {
            std::cout << "[VideoThread] YUV file read end.\n";
            break;
        }

        // 编码 => 得到 H.264 Packet
        AVPacket* packet = h264Enc->Encode(yuvBuffer, 0 /*pts可不使用*/);
        if (!packet) {
            std::cerr << "[VideoThread] H264Encoder returned null packet, skip.\n";
            continue;
        }

        // 判断是否关键帧
        bool bIsKeyFrame = false;
        if (packet->flags & AV_PKT_FLAG_KEY) {
            bIsKeyFrame = true;
        }

        // 跳过00 00 00 01起始码（若有）
        int skipBytes = 0;
        if (packet->size > 4 && packet->data[0] == 0x00 &&
            packet->data[1] == 0x00 &&
            packet->data[2] == 0x00 &&
            packet->data[3] == 0x01)
        {
            skipBytes = 4;
        }

        // 组装到EncodedFrame
        int sendSize = packet->size - skipBytes;
        EncodedFrame frame;
        frame.bKeyFrame = bIsKeyFrame;
        frame.timestamp = videoTimestamp;
        frame.data.resize(sendSize);
        memcpy(frame.data.data(), packet->data + skipBytes, sendSize);

        // 推入视频队列
        {
            std::unique_lock<std::mutex> lock(g_videoMutex);
            g_videoQueue.push(std::move(frame));
        }
        g_videoCond.notify_one();

        // 释放packet
        av_packet_free(&packet);

        // 根据fps估算本帧持续时间
        unsigned int frameDurationMs = 1000 / fps;
        videoTimestamp += frameDurationMs;

        // 准实时：等待到下一帧时间
        auto expected_time = start_time + std::chrono::milliseconds(videoTimestamp);
        auto now = std::chrono::steady_clock::now();
        if (expected_time > now) {
            std::this_thread::sleep_until(expected_time);
        }
    }

    delete[] yuvBuffer;
    fclose(fp_yuv);

    // 标记视频采集结束
    {
        std::unique_lock<std::mutex> lock(g_videoMutex);
        g_videoDone = true;
    }
    g_videoCond.notify_one(); // 通知推流线程可能需要结束
    std::cout << "[VideoThread] Finished.\n";
}

//--------------------------------------------------------
// 音频采集线程：读取 PCM -> AAC 编码 -> 存入队列
//--------------------------------------------------------
void AudioCaptureThread(AACEncoder* aacEnc,
    const std::string& pcmFilePath)
{
    // 打开PCM文件
    FILE* fp_pcm = fopen(pcmFilePath.c_str(), "rb");
    if (!fp_pcm) {
        std::cerr << "[AudioThread] Failed to open PCM file: " << pcmFilePath << std::endl;
        return;
    }
    std::cout << "[AudioThread] PCM file opened: " << pcmFilePath << std::endl;

    // 从编码器拿到帧大小
    int frameSize = aacEnc->getCodecCtx()->frame_size; // 一般1024
    int channels = aacEnc->getCodecCtx()->channels;   // 2
    int bytesPerSample = 2; // 假设16bit
    int bytesPerFrame = frameSize * channels * bytesPerSample;

    std::vector<uint8_t> pcmBuffer(bytesPerFrame);

    unsigned int audioTimestamp = 0; // 毫秒
    auto start_time = std::chrono::steady_clock::now();
    // AAC一帧通常对应1024个采样点 => 1024 / sample_rate秒
    double frameDurationMs = 1024.0 / (double)aacEnc->getSampleRate() * 1000.0;

    while (true)
    {
        // 读取一帧PCM
        size_t ret = fread(pcmBuffer.data(), 1, bytesPerFrame, fp_pcm);
        if (ret < (size_t)bytesPerFrame) {
            // 文件读完
            std::cout << "[AudioThread] PCM file read end.\n";
            break;
        }

        // 编码 => 得到AAC裸数据
        AVPacket* packet = aacEnc->Encode(pcmBuffer.data(), bytesPerFrame);
        if (!packet) {
            std::cerr << "[AudioThread] AACEncoder returned null packet, skip.\n";
            continue;
        }

        // 组装到EncodedFrame
        EncodedFrame frame;
        frame.bKeyFrame = false; // 音频无关键帧概念
        frame.timestamp = audioTimestamp;
        frame.data.resize(packet->size);
        memcpy(frame.data.data(), packet->data, packet->size);

        // 推入音频队列
        {
            std::unique_lock<std::mutex> lock(g_audioMutex);
            g_audioQueue.push(std::move(frame));
        }
        g_audioCond.notify_one();

        // 释放packet
        av_packet_free(&packet);

        // 累加时间戳
        audioTimestamp += (unsigned int)(frameDurationMs + 0.5);

        // 准实时发送：等待下一帧
        auto expected_time = start_time + std::chrono::milliseconds(audioTimestamp);
        auto now = std::chrono::steady_clock::now();
        if (expected_time > now) {
            std::this_thread::sleep_until(expected_time);
        }
    }

    fclose(fp_pcm);

    // 标记音频采集结束
    {
        std::unique_lock<std::mutex> lock(g_audioMutex);
        g_audioDone = true;
    }
    g_audioCond.notify_one();
    std::cout << "[AudioThread] Finished.\n";
}

//--------------------------------------------------------
// 推流线程：从队列里取数据 -> RTMP 推送
//--------------------------------------------------------
void PushThreadFunc(CRTMPStream* rtmpSender)
{
    std::cout << "[PushThread] Start.\n";

    while (true)
    {
        bool allDone = false;
        {
            // 判断是否音视频都完成且队列为空
            std::unique_lock<std::mutex> lockV(g_videoMutex);
            std::unique_lock<std::mutex> lockA(g_audioMutex);

            if (g_videoDone && g_audioDone &&
                g_videoQueue.empty() && g_audioQueue.empty())
            {
                allDone = true;
            }
        }
        if (allDone) {
            break;
        }

        // 1. 取一帧视频（如有）
        {
            std::unique_lock<std::mutex> lk(g_videoMutex);
            if (!g_videoQueue.empty())
            {
                EncodedFrame frame = std::move(g_videoQueue.front());
                g_videoQueue.pop();
                lk.unlock();

                // 推送H264
                rtmpSender->SendH264Packet(frame.data.data(),
                    frame.data.size(),
                    frame.bKeyFrame,
                    frame.timestamp);
            }
            else {
                // 没有视频帧可取，先不阻塞
            }
        }

        // 2. 取一帧音频（如有）
        {
            std::unique_lock<std::mutex> lk(g_audioMutex);
            if (!g_audioQueue.empty())
            {
                EncodedFrame frame = std::move(g_audioQueue.front());
                g_audioQueue.pop();
                lk.unlock();

                // 推送AAC
                rtmpSender->SendAACPacket(frame.data.data(),
                    frame.data.size(),
                    frame.timestamp);
            }
            else {
                // 没有音频帧可取
            }
        }

        // 推流线程做一个小的sleep避免空转 
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "[PushThread] All done, exit.\n";
}

//--------------------------------------------------------
// 主函数：分别启动 (1) 视频采集线程 (2) 音频采集线程 (3) 推流线程
//--------------------------------------------------------
int main()
{
    // RTMP 服务器地址、输入文件
    std::string rtmp_url = "rtmp://localhost/live/livestream";
    std::string yuv_file = "720x480_25fps_420p.yuv";
    std::string pcm_file = "48000_2_s16le.pcm";

    // 1) 创建并连接RTMP
    CRTMPStream rtmpSender;
    if (!rtmpSender.Connect(rtmp_url.c_str()))
    {
        std::cerr << "Failed to connect to RTMP Server: " << rtmp_url << std::endl;
        return -1;
    }
    std::cout << "[Main] RTMP connected: " << rtmp_url << std::endl;

    // 2) 初始化视频编码器 (H264Encoder)
    {
        // 示例：宽720, 高480, fps=25, bitrate=800kbps, gop=25
    }
    Properties propsVideo;
    propsVideo.SetProperty("width", 720);
    propsVideo.SetProperty("height", 480);
    propsVideo.SetProperty("fps", 25);
    propsVideo.SetProperty("bitrate", 800 * 1024);
    propsVideo.SetProperty("b_frames", 0); // 不要B帧
    propsVideo.SetProperty("gop", 25);     // GOP大小

    H264Encoder h264Enc;
    if (h264Enc.Init(propsVideo) < 0) {
        std::cerr << "[Main] H264Encoder init failed.\n";
        return -1;
    }
    std::cout << "[Main] H264Encoder init OK.\n";

    // 获取SPS/PPS
    int spsLen = h264Enc.getSPSSize();
    int ppsLen = h264Enc.getPPSSize();
    uint8_t spsData[1024] = { 0 };
    uint8_t ppsData[1024] = { 0 };
    if (spsLen > 0) {
        memcpy(spsData, h264Enc.getSPSData(), spsLen);
    }
    if (ppsLen > 0) {
        memcpy(ppsData, h264Enc.getPPSData(), ppsLen);
    }

    // 3) 初始化音频编码器 (AACEncoder)
    {
        // 示例：采样率48000, 双声道, 128kbps
    }
    Properties propsAudio;
    propsAudio.SetProperty("sample_rate", 48000);
    propsAudio.SetProperty("channels", 2);
    propsAudio.SetProperty("bitrate", 128 * 1024);

    AACEncoder aacEnc;
    if (aacEnc.Init(propsAudio) != RET_OK) {
        std::cerr << "[Main] AACEncoder init failed.\n";
        return -1;
    }
    std::cout << "[Main] AACEncoder init OK.\n";

    // 4) 发送音视频MetaData (SPS/PPS + AudioSpecCfg)
    RTMPMetadata metaData;
    memset(&metaData, 0, sizeof(RTMPMetadata));

    // 视频部分
    metaData.bHasVideo = true;
    metaData.nWidth = h264Enc.getWidth();
    metaData.nHeight = h264Enc.getHeight();
    metaData.nFrameRate = 25;
    metaData.nVideoDataRate = 800 * 1024;
    metaData.nSpsLen = spsLen;
    memcpy(metaData.Sps, spsData, spsLen);
    metaData.nPpsLen = ppsLen;
    memcpy(metaData.Pps, ppsData, ppsLen);

    // 音频部分
    metaData.bHasAudio = true;
    metaData.nAudioSampleRate = aacEnc.getSampleRate(); // 48000
    metaData.nAudioSampleSize = 16; // 16bit
    metaData.nAudioChannels = aacEnc.getChannels(); // 2

    // 拼装 AudioSpecCfg (2字节)
    // 这里演示写死或从ADTS头中取前2字节
    uint8_t adtsHeader[7] = { 0 };
    aacEnc.getAdtsHeader(adtsHeader, 0);
    // 示例: profile=2, freqIdx=3(48kHz), channelCfg=2 => 0x12, 0x10(或0x90)等
    metaData.AudioSpecCfg[0] = 0x11; // 可视实际情况修改
    metaData.AudioSpecCfg[1] = 0x90;
    metaData.nAudioSpecCfgLen = 2;

    // 发送MetaData
    rtmpSender.SendMetadata(&metaData);
    std::cout << "[Main] Sent RTMP Metadata.\n";

    // 5) 启动采集线程(视频、音频) + 推流线程
    std::thread videoThread(VideoCaptureThread, &h264Enc, yuv_file, 25);
    std::thread audioThread(AudioCaptureThread, &aacEnc, pcm_file);
    std::thread pushThread(PushThreadFunc, &rtmpSender);

    // 6) 等待所有线程结束
    videoThread.join();
    audioThread.join();
    pushThread.join();

    // 7) 关闭RTMP
    rtmpSender.Close();
    std::cout << "[Main] RTMP push finished. Exit.\n";
    return 0;
}
