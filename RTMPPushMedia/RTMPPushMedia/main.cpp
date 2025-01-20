#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "RTMPStream.h"   // ���CRTMPStream��
#include "H264Encoder.h"  
#include "AACEncoder.h"
#include "mediabase.h"    // ���Properties������
#include "codecs.h"       // ���AudioCodec��FFmpeg�������

// -------------------- �������ݽṹ�����м�ͬ����־ --------------------
struct EncodedFrame {
    bool bKeyFrame;         // ������Ƶ��Ч����Ƶ�ɺ���
    unsigned int timestamp; // ����
    std::vector<uint8_t> data;
};

// ��Ƶ���ݶ���
static std::queue<EncodedFrame> g_videoQueue;
static std::mutex g_videoMutex;
static std::condition_variable g_videoCond;
static bool g_videoDone = false;  // ��Ƶ�߳��Ƿ����

// ��Ƶ���ݶ���
static std::queue<EncodedFrame> g_audioQueue;
static std::mutex g_audioMutex;
static std::condition_variable g_audioCond;
static bool g_audioDone = false;  // ��Ƶ�߳��Ƿ����

//--------------------------------------------------------
// ��Ƶ�ɼ��̣߳���ȡ YUV -> H264 ���� -> �������
//--------------------------------------------------------
void VideoCaptureThread(H264Encoder* h264Enc,
    const std::string& yuvFilePath,
    int fps)
{
    // ��YUV�ļ�
    FILE* fp_yuv = fopen(yuvFilePath.c_str(), "rb");
    if (!fp_yuv) {
        std::cerr << "[VideoThread] Failed to open YUV file: " << yuvFilePath << std::endl;
        return;
    }
    std::cout << "[VideoThread] YUV file opened: " << yuvFilePath << std::endl;

    // ��ȡ��������Ϣ
    int width = h264Enc->getWidth();
    int height = h264Enc->getHeight();
    int frameSize = width * height * 3 / 2; // YUV420P
    uint8_t* yuvBuffer = new uint8_t[frameSize];

    unsigned int videoTimestamp = 0; // ����
    auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        // ��ȡһ֡ YUV
        size_t ret = fread(yuvBuffer, 1, frameSize, fp_yuv);
        if (ret < (size_t)frameSize) {
            std::cout << "[VideoThread] YUV file read end.\n";
            break;
        }

        // ���� => �õ� H.264 Packet
        AVPacket* packet = h264Enc->Encode(yuvBuffer, 0 /*pts�ɲ�ʹ��*/);
        if (!packet) {
            std::cerr << "[VideoThread] H264Encoder returned null packet, skip.\n";
            continue;
        }

        // �ж��Ƿ�ؼ�֡
        bool bIsKeyFrame = false;
        if (packet->flags & AV_PKT_FLAG_KEY) {
            bIsKeyFrame = true;
        }

        // ����00 00 00 01��ʼ�루���У�
        int skipBytes = 0;
        if (packet->size > 4 && packet->data[0] == 0x00 &&
            packet->data[1] == 0x00 &&
            packet->data[2] == 0x00 &&
            packet->data[3] == 0x01)
        {
            skipBytes = 4;
        }

        // ��װ��EncodedFrame
        int sendSize = packet->size - skipBytes;
        EncodedFrame frame;
        frame.bKeyFrame = bIsKeyFrame;
        frame.timestamp = videoTimestamp;
        frame.data.resize(sendSize);
        memcpy(frame.data.data(), packet->data + skipBytes, sendSize);

        // ������Ƶ����
        {
            std::unique_lock<std::mutex> lock(g_videoMutex);
            g_videoQueue.push(std::move(frame));
        }
        g_videoCond.notify_one();

        // �ͷ�packet
        av_packet_free(&packet);

        // ����fps���㱾֡����ʱ��
        unsigned int frameDurationMs = 1000 / fps;
        videoTimestamp += frameDurationMs;

        // ׼ʵʱ���ȴ�����һ֡ʱ��
        auto expected_time = start_time + std::chrono::milliseconds(videoTimestamp);
        auto now = std::chrono::steady_clock::now();
        if (expected_time > now) {
            std::this_thread::sleep_until(expected_time);
        }
    }

    delete[] yuvBuffer;
    fclose(fp_yuv);

    // �����Ƶ�ɼ�����
    {
        std::unique_lock<std::mutex> lock(g_videoMutex);
        g_videoDone = true;
    }
    g_videoCond.notify_one(); // ֪ͨ�����߳̿�����Ҫ����
    std::cout << "[VideoThread] Finished.\n";
}

//--------------------------------------------------------
// ��Ƶ�ɼ��̣߳���ȡ PCM -> AAC ���� -> �������
//--------------------------------------------------------
void AudioCaptureThread(AACEncoder* aacEnc,
    const std::string& pcmFilePath)
{
    // ��PCM�ļ�
    FILE* fp_pcm = fopen(pcmFilePath.c_str(), "rb");
    if (!fp_pcm) {
        std::cerr << "[AudioThread] Failed to open PCM file: " << pcmFilePath << std::endl;
        return;
    }
    std::cout << "[AudioThread] PCM file opened: " << pcmFilePath << std::endl;

    // �ӱ������õ�֡��С
    int frameSize = aacEnc->getCodecCtx()->frame_size; // һ��1024
    int channels = aacEnc->getCodecCtx()->channels;   // 2
    int bytesPerSample = 2; // ����16bit
    int bytesPerFrame = frameSize * channels * bytesPerSample;

    std::vector<uint8_t> pcmBuffer(bytesPerFrame);

    unsigned int audioTimestamp = 0; // ����
    auto start_time = std::chrono::steady_clock::now();
    // AACһ֡ͨ����Ӧ1024�������� => 1024 / sample_rate��
    double frameDurationMs = 1024.0 / (double)aacEnc->getSampleRate() * 1000.0;

    while (true)
    {
        // ��ȡһ֡PCM
        size_t ret = fread(pcmBuffer.data(), 1, bytesPerFrame, fp_pcm);
        if (ret < (size_t)bytesPerFrame) {
            // �ļ�����
            std::cout << "[AudioThread] PCM file read end.\n";
            break;
        }

        // ���� => �õ�AAC������
        AVPacket* packet = aacEnc->Encode(pcmBuffer.data(), bytesPerFrame);
        if (!packet) {
            std::cerr << "[AudioThread] AACEncoder returned null packet, skip.\n";
            continue;
        }

        // ��װ��EncodedFrame
        EncodedFrame frame;
        frame.bKeyFrame = false; // ��Ƶ�޹ؼ�֡����
        frame.timestamp = audioTimestamp;
        frame.data.resize(packet->size);
        memcpy(frame.data.data(), packet->data, packet->size);

        // ������Ƶ����
        {
            std::unique_lock<std::mutex> lock(g_audioMutex);
            g_audioQueue.push(std::move(frame));
        }
        g_audioCond.notify_one();

        // �ͷ�packet
        av_packet_free(&packet);

        // �ۼ�ʱ���
        audioTimestamp += (unsigned int)(frameDurationMs + 0.5);

        // ׼ʵʱ���ͣ��ȴ���һ֡
        auto expected_time = start_time + std::chrono::milliseconds(audioTimestamp);
        auto now = std::chrono::steady_clock::now();
        if (expected_time > now) {
            std::this_thread::sleep_until(expected_time);
        }
    }

    fclose(fp_pcm);

    // �����Ƶ�ɼ�����
    {
        std::unique_lock<std::mutex> lock(g_audioMutex);
        g_audioDone = true;
    }
    g_audioCond.notify_one();
    std::cout << "[AudioThread] Finished.\n";
}

//--------------------------------------------------------
// �����̣߳��Ӷ�����ȡ���� -> RTMP ����
//--------------------------------------------------------
void PushThreadFunc(CRTMPStream* rtmpSender)
{
    std::cout << "[PushThread] Start.\n";

    while (true)
    {
        bool allDone = false;
        {
            // �ж��Ƿ�����Ƶ������Ҷ���Ϊ��
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

        // 1. ȡһ֡��Ƶ�����У�
        {
            std::unique_lock<std::mutex> lk(g_videoMutex);
            if (!g_videoQueue.empty())
            {
                EncodedFrame frame = std::move(g_videoQueue.front());
                g_videoQueue.pop();
                lk.unlock();

                // ����H264
                rtmpSender->SendH264Packet(frame.data.data(),
                    frame.data.size(),
                    frame.bKeyFrame,
                    frame.timestamp);
            }
            else {
                // û����Ƶ֡��ȡ���Ȳ�����
            }
        }

        // 2. ȡһ֡��Ƶ�����У�
        {
            std::unique_lock<std::mutex> lk(g_audioMutex);
            if (!g_audioQueue.empty())
            {
                EncodedFrame frame = std::move(g_audioQueue.front());
                g_audioQueue.pop();
                lk.unlock();

                // ����AAC
                rtmpSender->SendAACPacket(frame.data.data(),
                    frame.data.size(),
                    frame.timestamp);
            }
            else {
                // û����Ƶ֡��ȡ
            }
        }

        // �����߳���һ��С��sleep�����ת 
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "[PushThread] All done, exit.\n";
}

//--------------------------------------------------------
// ���������ֱ����� (1) ��Ƶ�ɼ��߳� (2) ��Ƶ�ɼ��߳� (3) �����߳�
//--------------------------------------------------------
int main()
{
    // RTMP ��������ַ�������ļ�
    std::string rtmp_url = "rtmp://localhost/live/livestream";
    std::string yuv_file = "720x480_25fps_420p.yuv";
    std::string pcm_file = "48000_2_s16le.pcm";

    // 1) ����������RTMP
    CRTMPStream rtmpSender;
    if (!rtmpSender.Connect(rtmp_url.c_str()))
    {
        std::cerr << "Failed to connect to RTMP Server: " << rtmp_url << std::endl;
        return -1;
    }
    std::cout << "[Main] RTMP connected: " << rtmp_url << std::endl;

    // 2) ��ʼ����Ƶ������ (H264Encoder)
    {
        // ʾ������720, ��480, fps=25, bitrate=800kbps, gop=25
    }
    Properties propsVideo;
    propsVideo.SetProperty("width", 720);
    propsVideo.SetProperty("height", 480);
    propsVideo.SetProperty("fps", 25);
    propsVideo.SetProperty("bitrate", 800 * 1024);
    propsVideo.SetProperty("b_frames", 0); // ��ҪB֡
    propsVideo.SetProperty("gop", 25);     // GOP��С

    H264Encoder h264Enc;
    if (h264Enc.Init(propsVideo) < 0) {
        std::cerr << "[Main] H264Encoder init failed.\n";
        return -1;
    }
    std::cout << "[Main] H264Encoder init OK.\n";

    // ��ȡSPS/PPS
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

    // 3) ��ʼ����Ƶ������ (AACEncoder)
    {
        // ʾ����������48000, ˫����, 128kbps
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

    // 4) ��������ƵMetaData (SPS/PPS + AudioSpecCfg)
    RTMPMetadata metaData;
    memset(&metaData, 0, sizeof(RTMPMetadata));

    // ��Ƶ����
    metaData.bHasVideo = true;
    metaData.nWidth = h264Enc.getWidth();
    metaData.nHeight = h264Enc.getHeight();
    metaData.nFrameRate = 25;
    metaData.nVideoDataRate = 800 * 1024;
    metaData.nSpsLen = spsLen;
    memcpy(metaData.Sps, spsData, spsLen);
    metaData.nPpsLen = ppsLen;
    memcpy(metaData.Pps, ppsData, ppsLen);

    // ��Ƶ����
    metaData.bHasAudio = true;
    metaData.nAudioSampleRate = aacEnc.getSampleRate(); // 48000
    metaData.nAudioSampleSize = 16; // 16bit
    metaData.nAudioChannels = aacEnc.getChannels(); // 2

    // ƴװ AudioSpecCfg (2�ֽ�)
    // ������ʾд�����ADTSͷ��ȡǰ2�ֽ�
    uint8_t adtsHeader[7] = { 0 };
    aacEnc.getAdtsHeader(adtsHeader, 0);
    // ʾ��: profile=2, freqIdx=3(48kHz), channelCfg=2 => 0x12, 0x10(��0x90)��
    metaData.AudioSpecCfg[0] = 0x11; // ����ʵ������޸�
    metaData.AudioSpecCfg[1] = 0x90;
    metaData.nAudioSpecCfgLen = 2;

    // ����MetaData
    rtmpSender.SendMetadata(&metaData);
    std::cout << "[Main] Sent RTMP Metadata.\n";

    // 5) �����ɼ��߳�(��Ƶ����Ƶ) + �����߳�
    std::thread videoThread(VideoCaptureThread, &h264Enc, yuv_file, 25);
    std::thread audioThread(AudioCaptureThread, &aacEnc, pcm_file);
    std::thread pushThread(PushThreadFunc, &rtmpSender);

    // 6) �ȴ������߳̽���
    videoThread.join();
    audioThread.join();
    pushThread.join();

    // 7) �ر�RTMP
    rtmpSender.Close();
    std::cout << "[Main] RTMP push finished. Exit.\n";
    return 0;
}
