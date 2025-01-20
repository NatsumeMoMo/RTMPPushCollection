#pragma once

#include <iostream>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#pragma warning(disable:4996)

enum RET_CODE
{
    RET_ERR_UNKNOWN = -2,                   // 未知错误
    RET_FAIL = -1,							// 失败
    RET_OK = 0,							// 正常
    RET_ERR_OPEN_FILE,						// 打开文件失败
    RET_ERR_NOT_SUPPORT,					// 不支持
    RET_ERR_OUTOFMEMORY,					// 没有内存
    RET_ERR_STACKOVERFLOW,					// 溢出
    RET_ERR_NULLREFERENCE,					// 空参考
    RET_ERR_ARGUMENTOUTOFRANGE,				//
    RET_ERR_PARAMISMATCH,					//
    RET_ERR_MISMATCH_CODE,                  // 没有匹配的编解码器
    RET_ERR_EAGAIN,
    RET_ERR_EOF
};

class MsgBaseObj
{
public:
    MsgBaseObj() {}
    virtual ~MsgBaseObj() {}
};

typedef struct LooperMessage {
    int what;
    MsgBaseObj* obj;
    bool quit;
} LooperMessage;

enum RTMP_BODY_TYPE
{
    RTMP_BODY_METADATA, // metadata
    RTMP_BODY_AUD_RAW, // raw data
    RTMP_BODY_AUD_SPEC, // AudioSpecificConfig
    RTMP_BODY_VID_RAW, // raw data
    RTMP_BODY_VID_CONFIG // H264 Configuration
};

class FLVMetadataMsg : public MsgBaseObj
{
public:
    FLVMetadataMsg() {}
    virtual ~FLVMetadataMsg() {}

    bool has_audio = false;
    bool has_video = false;
    int audiocodeid = -1;
    int audiodatarate = 0;
    int audiodelay = 0;
    int audiosamplerate = 0;
    int audiosamplesize = 0;
    int channels = 0;

    bool canSeekToEnd = 0;

    std::string creationdate;
    int duration = 0;
    int64_t filesize = 0;
    double framerate = 0;
    int height = 0;
    bool stereo = true;

    int videocodecid = -1;
    int64_t videodatarate = 0;
    int width = 0;
    int64_t pts = 0;
};

class AudioSpecMsg :public MsgBaseObj {
public:
    AudioSpecMsg(uint8_t profile, uint8_t channel_num, uint32_t samplerate) {
        profile_ = profile;
        channels_ = channel_num;
        sample_rate_ = samplerate;
    }

    virtual ~AudioSpecMsg() {}

    uint8_t profile_ = 2;
    uint8_t channels_ = 2;
    uint32_t sample_rate_ = 48000;
    int64_t pts_;
};

class AudioRawMsg : public MsgBaseObj {
public:
    AudioRawMsg(int size, int with_adts = 0)
    {
        this->size = size;
        type = 0;
        with_adts_ = with_adts;
        data = (unsigned char*)malloc(size * sizeof(char));
    }

    AudioRawMsg(const unsigned char* buf, int size, int with_adts = 0) {
        this->size = size;
        type = buf[4] & 0x1f;
        with_adts_ = with_adts;
        data = (unsigned char*)malloc(size * sizeof(char));
        memcpy(data, buf, size);
    }

    virtual ~AudioRawMsg() {
        if (data) free(data);
    }

    int type;
    int size;
    int with_adts_ = 0;
    unsigned char* data = nullptr;
    uint32_t pts;
};

class NaluStruct : public MsgBaseObj
{
public:
    NaluStruct(int size);
    NaluStruct(const unsigned char* data, int size);
    virtual ~NaluStruct();
    int type;
    int size;
    unsigned char* data = nullptr;
    uint32_t pts;
};

class VideoSequenceHeaderMsg : public MsgBaseObj
{
public:
    VideoSequenceHeaderMsg(uint8_t* sps, int sps_size, uint8_t* pps, int pps_size)
    {
        sps_ = (uint8_t*)malloc(sps_size * sizeof(uint8_t));
        pps_ = (uint8_t*)malloc(pps_size * sizeof(uint8_t));
        if (!sps || !pps)
        {
            // LogError("VideoSequenceHeaderMsg malloc failed");
            std::cerr << "VideoSequenceHeaderMsg malloc failed" << std::endl;
            return;
        }

        sps_size_ = sps_size;
        memcpy(sps_, sps, sps_size);
        pps_size_ = pps_size;
        memcpy(pps_, pps, pps_size);
    }
    virtual ~VideoSequenceHeaderMsg()
    {
        if (sps_)
            free(sps_);
        if (pps_)
            free(pps_);
    }

public:
    uint8_t* sps_;
    int sps_size_;
    uint8_t* pps_;
    int pps_size_;

    unsigned int nWidth;
    unsigned int nHeight;
    unsigned int nFrameRate; // fps
    unsigned int nVideoDataRate; // bps
    int64_t pts_ = 0;
};

static inline int strcasecmp(const char* s1, const char* s2)
{
    //   while  (toupper((unsigned char)*s1) == toupper((unsigned char)*s2++))
    //       if (*s1++ == '\0') return 0;
    //   return(toupper((unsigned char)*s1) - toupper((unsigned char)*--s2));
    while ((unsigned char)*s1 == (unsigned char)*s2++)
        if (*s1++ == '\0') return 0;
    return((unsigned char)*s1 - (unsigned char)*--s2);
}

class Properties : public std::map<std::string, std::string>
{
public:
    bool HasProperty(const std::string& key) const
    {
        return find(key) != end();
    }

    void SetProperty(const char* key, int intval)
    {
        SetProperty(std::string(key), std::to_string(intval));
    }

    void SetProperty(const char* key, uint32_t val)
    {
        SetProperty(std::string(key), std::to_string(val));
    }

    void SetProperty(const char* key, uint64_t val)
    {
        SetProperty(std::string(key), std::to_string(val));
    }

    void SetProperty(const char* key, const char* val)
    {
        SetProperty(std::string(key), std::string(val));
    }

    void SetProperty(const std::string& key, const std::string& val)
    {
        insert(std::pair<std::string, std::string>(key, val));
    }

    void GetChildren(const std::string& path, Properties& children) const
    {
        //Create sarch string
        std::string parent(path);
        //Add the final .
        parent += ".";
        //For each property
        for (const_iterator it = begin(); it != end(); ++it)
        {
            const std::string& key = it->first;
            //Check if it is from parent
            if (key.compare(0, parent.length(), parent) == 0)
                //INsert it
                children.SetProperty(key.substr(parent.length(), key.length() - parent.length()), it->second);
        }
    }

    void GetChildren(const char* path, Properties& children) const
    {
        GetChildren(std::string(path), children);
    }

    Properties GetChildren(const std::string& path) const
    {
        Properties properties;
        //Get them
        GetChildren(path, properties);
        //Return
        return properties;
    }

    Properties GetChildren(const char* path) const
    {
        Properties properties;
        //Get them
        GetChildren(path, properties);
        //Return
        return properties;
    }

    void GetChildrenArray(const char* path, std::vector<Properties>& array) const
    {
        //Create sarch string
        std::string parent(path);
        //Add the final .
        parent += ".";

        //Get array length
        int length = GetProperty(parent + "length", 0);

        //For each element
        for (int i = 0; i < length; ++i)
        {
            char index[64];
            //Print string
            snprintf(index, sizeof(index), "%d", i);
            //And get children
            array.push_back(GetChildren(parent + index));
        }
    }

    const char* GetProperty(const char* key) const // xxxx
    {
        return GetProperty(key, "");
    }

    std::string GetProperty(const char* key, const std::string defaultValue) const // xxx
    {
        //Find item
        const_iterator it = find(std::string(key));
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second;
    }

    std::string GetProperty(const std::string& key, const std::string defaultValue) const //xxx
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second;
    }

    const char* GetProperty(const char* key, const char* defaultValue) const //xxx
    {
        //Find item
        const_iterator it = find(std::string(key));
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second.c_str();
    }

    const char* GetProperty(const std::string& key, char* defaultValue) const //xxx
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second.c_str();
    }

    int GetProperty(const char* key, int defaultValue) const // xxx
    {
        return GetProperty(std::string(key), defaultValue);
    }

    int GetProperty(const std::string& key, int defaultValue) const // xxx
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return atoi(it->second.c_str());
    }

    uint64_t GetProperty(const char* key, uint64_t defaultValue) const // xxx
    {
        return GetProperty(std::string(key), defaultValue);
    }

    uint64_t GetProperty(const std::string& key, uint64_t defaultValue) const // xxx
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return atoll(it->second.c_str());
    }

    bool GetProperty(const char* key, bool defaultValue) const // xxx
    {
        return GetProperty(std::string(key), defaultValue);
    }

    bool GetProperty(const std::string& key, bool defaultValue) const // xxx
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Get value
        char* val = (char*)it->second.c_str();
        //Check it
        if (strcasecmp(val, (char*)"yes") == 0)
            return true;
        else if (strcasecmp(val, (char*)"true") == 0)
            return true;
        //Return value
        return (atoi(val));
    }
};
