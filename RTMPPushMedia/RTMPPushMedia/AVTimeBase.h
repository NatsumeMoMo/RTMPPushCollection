//
// Created by 11197 on 25-1-2.
//

#ifndef AVTIMEBASE_H
#define AVTIMEBASE_H
#include <cstdint>
#include <ctime>
#include "TimeUtil.h"

#ifdef WIN32
#include <winsock2.h>
#include <time.h>
#else
#include <sys/time.h>
#endif


class AVPublishTime
{
public:
    typedef enum PTS_STRATEGY {
        PTS_RECTIFY = 0,    // ȱʡ����, pts��ʱ������������֡���
        PTS_REAL_TIME       // ʵʱPTS
    }PTS_STRATEGY;
public:
    static AVPublishTime* GetInstance()
    {
        if (instance == nullptr)
            instance = new AVPublishTime();
        return instance;
    }
    AVPublishTime()
    {
        start_time_ = GetCurrentTimeMsec();
    }

    void Reset()
    {
        start_time_ = GetCurrentTimeMsec();
    }

    void SetAudioFrameDuration(const double frame_duration)
    {
        audio_frame_duration_ = frame_duration;
        audio_frame_threshold = (uint32_t)(frame_duration / 2);
    }

    void SetVideoFrameDuration(const double frame_duration)
    {
        video_frame_duration_ = frame_duration;
        video_frame_threshold_ = (uint32_t)(frame_duration / 2);
    }

    uint32_t GetAudioPts()
    {
        int64_t pts = GetCurrentTimeMsec() - start_time_;
        if (PTS_RECTIFY == audio_pts_strategy_) {
            uint32_t diff = (uint32_t)abs(pts - (long long)(audio_pre_pts_ + audio_frame_duration_));
            if (diff < audio_frame_threshold) {
                // �������ֵ��Χ��, ����֡���
                audio_pre_pts_ += audio_frame_duration_; // ֡����ۼ�, ������
                // LogDebug("get_audio_pts1:%u RECTIFY:%0.0lf", diff, audio_pre_pts_);
                return (uint32_t)(((int64_t)audio_pre_pts_) % 0xffffffff);
            }
            audio_pre_pts_ = (double)pts; // ������֡, ���µ���PTS
            // LogDebug("get_audio_pts2:%u, RECTIFY:%0.0lf", diff, audio_pre_pts_);
            return (uint32_t)(pts % 0xffffffff);
        }
        else {
            audio_pre_pts_ = (double)pts; // ������֡, ���µ���PTS
            // LogDebug("get_audio_pts REAL_TIME:%0.0lf", audio_pre_pts_);
            return (uint32_t)(pts % 0xffffffff);
        }
    }

    uint32_t GetVideoPts()
    {
        int64_t pts = GetCurrentTimeMsec() - start_time_;
        if (PTS_RECTIFY == video_pts_strategy_) {
            uint32_t diff = (uint32_t)abs(pts - (long long)(video_pre_pts_ + video_frame_duration_));
            if (diff < video_frame_threshold_) {
                // �������ֵ��Χ��, ����֡���
                video_pre_pts_ += video_frame_duration_;
                // LogDebug("get_video_pts1:%u RECTIFY:%0.0lf", diff, video_pre_pts_);
                return (uint32_t)(((int64_t)video_pre_pts_) % 0xffffffff);
            }
            video_pre_pts_ = (double)pts; // ������֡, ���µ���PTS
            // LogDebug("get_video_pts2:%u RECTIFY:%0.0lf", diff, video_pre_pts_);
            return (uint32_t)(pts % 0xffffffff);
        }
        else {
            video_pre_pts_ = (double)pts; // ������֡, ���µ���PTS
            // LogDebug("get_video_pts REAL_TIME:%0.0lf", video_pre_pts_);
            return (uint32_t)(pts % 0xffffffff);
        }
    }

    void SetAudioPtsStrategy(const PTS_STRATEGY strategy) {
        audio_pts_strategy_ = strategy;
    }

    void SetVideoPtsStrategy(const PTS_STRATEGY strategy) {
        video_pts_strategy_ = strategy;
    }

    uint32_t getCurrentTime() // GetCurrentTime �� WinBase.h ��GetTickCount()�ĺ�
    {
        int64_t t = GetCurrentTimeMsec() - start_time_;
        return (uint32_t)(t % 0xffffffff);
    }

    // �����ؼ����ʱ���
    inline const char* getKeyTimeTag() { return "keytime"; }
    // rtmpλ�ùؼ���
    inline const char* getRtmpTag() { return "keytime:rtmp_publish"; }

    // ����metadata
    inline const char* getMetadataTag() { return "keytime:metadata"; }
    // aac sequence header
    inline const char* getAacHeaderTag() { return "keytime:aacheader"; }
    // aac raw data
    inline const char* getAacDataTag() { return "keytime:aacdata"; }
    // avc sequence header
    inline const char* getAvcHeaderTag() { return "keytime:avcheader"; }

    // ��һ��i֡
    inline const char* getAvcIFrameTag() { return "keytime:avciframe"; }
    // ��һ����i֡
    inline const char* getAvcFrameTag() { return "keytime:avcframe"; }
    // ����Ƶ����
    inline const char* getAcodecTag() { return "keytime:acodec"; }
    inline const char* getVcodecTag() { return "keytime:vcodec"; }
    // ����Ƶ����
    inline const char* getAInTag() { return "keytime:ain"; }
    inline const char* getVInTag() { return "keytime:vint"; }

private:
    int64_t GetCurrentTimeMsec() {
#ifdef WIN32
        timeval tv;
        time_t clock;
        tm tm;
        SYSTEMTIME wtm;
        GetLocalTime(&wtm);
        tm.tm_year = wtm.wYear - 1900;
        tm.tm_mon = wtm.wMonth - 1;
        tm.tm_mday = wtm.wDay;
        tm.tm_hour = wtm.wHour;
        tm.tm_min = wtm.wMinute;
        tm.tm_sec = wtm.wSecond;
        tm.tm_isdst = -1;
        clock = mktime(&tm);
        tv.tv_sec = clock;
        tv.tv_usec = wtm.wMilliseconds * 1000;
        return ((unsigned long long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000);
#else
        timeval tv;
        gettimeofday(&tv, NULL);
        return ((unsigned long long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000);
#endif
    }

private:
    int64_t start_time_;
    PTS_STRATEGY audio_pts_strategy_ = PTS_RECTIFY;
    double audio_frame_duration_ = 21.3333; // Ĭ�ϰ�AAC 1024��������, 48kHZ����
    uint32_t audio_frame_threshold = (uint32_t)(audio_frame_duration_ / 2);
    double audio_pre_pts_ = 0;

    PTS_STRATEGY video_pts_strategy_ = PTS_RECTIFY;
    double video_frame_duration_ = 40; // Ĭ�ϰ�25֡����
    uint32_t video_frame_threshold_ = (uint32_t)(video_frame_duration_ / 2);
    double video_pre_pts_ = 0;

    static AVPublishTime* instance;
};


/* ����Debug RTMP�����Ĺؼ�ʱ��� */
class AVPlayTime
{
public:
    static AVPlayTime* GetInstance() {
        if (instance == nullptr) {
            instance = new AVPlayTime();
        }
        return instance;
    }

    AVPlayTime() {
        start_time_ = getCurrentTimeMsec();
    }

    void Reset() {
        start_time_ = getCurrentTimeMsec();
    }

    // �����ؼ����ʱ���
    inline const char* getKeyTimeTag() { return "keytime"; }

    // RTMPλ�ùؼ���
    inline const char* getRtmpTag() { return "keytime:rtmp_pull"; }

    // ��ȡ��MetaData
    inline const char* getMetadataTag() { return "metadata"; }

    // AAC Sequence header
    inline const char* getAACHeaderTag() { return "aacheader"; }

    // AAC RAW Data
    inline const char* getAACDataTag() { return "aacdata"; }

    // AVC Sequnence header
    inline const char* getAVCHeaderTag() { return "avcheader"; }

    // ��һ��I֡
    inline const char* getAVCIFrameTag() { return "avciframe"; }

    // ��һ����I֡
    inline const char* getAVCFrameTag() { return "avcframe"; }

    // ����Ƶ����
    inline const char* getAcodecTag() { return "keytime:acodec"; }
    inline const char* getVcodecTag() { return "keytime:vcodec"; }

    // ����Ƶ���
    inline const char* getAoutTag() { return "keytime:aout"; }
    inline const char* getVoutTag() { return "keytime:vout"; }

    // ���غ���
    uint32_t getCurrentTime() {
        int64_t t = getCurrentTimeMsec() - start_time_;
        return (uint32_t)(t % 0xffffffff);
    }

private:
    int64_t getCurrentTimeMsec() {
#ifdef WIN32
        timeval tv;
        time_t clock;
        tm tm;
        SYSTEMTIME wtm;
        GetLocalTime(&wtm);
        tm.tm_year = wtm.wYear - 1900;
        tm.tm_mon = wtm.wMonth - 1;
        tm.tm_mday = wtm.wDay;
        tm.tm_hour = wtm.wHour;
        tm.tm_min = wtm.wMinute;
        tm.tm_sec = wtm.wSecond;
        tm.tm_isdst = -1;
        clock = mktime(&tm);
        tv.tv_sec = clock;
        tv.tv_usec = wtm.wMilliseconds * 1000;
        return ((unsigned long long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000);
#else
        timeval tv;
        gettimeofday(&tv, NULL);
        return return ((unsigned long long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000);
#endif
    }

private:
    int64_t start_time_;
    static AVPlayTime* instance;
};

#endif //AVTIMEBASE_H
