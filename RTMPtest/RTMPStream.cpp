/********************************************************************
filename:   RTMPStream.cpp
created:    2013-04-3
author:     firehood
purpose:    发送H264视频到RTMP Server，使用libRtmp库
*********************************************************************/
#include "RTMPStream.h"
#include "SpsDecode.h"


#include "H264Encoder.h"
#include "mediabase.h"
#include <thread>

#include "AVTimeBase.h"


#ifdef WIN32
#include <windows.h>
#endif


#ifdef WIN32
#pragma comment(lib,"WS2_32.lib")
#pragma comment(lib,"winmm.lib")
#endif

enum
{
	FLV_CODECID_H264 = 7,
	FLV_CODECID_AAC = 10,
};

int InitSockets()
{
#ifdef WIN32
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);
	return (WSAStartup(version, &wsaData) == 0);
#else
	return TRUE;
#endif
}

inline void CleanupSockets()
{
#ifdef WIN32
	WSACleanup();
#endif
}

char * put_byte( char *output, uint8_t nVal )
{
	output[0] = nVal;
	return output+1;
}
char * put_be16(char *output, uint16_t nVal )
{
	output[1] = nVal & 0xff;
	output[0] = nVal >> 8;
	return output+2;
}
char * put_be24(char *output,uint32_t nVal )
{
	output[2] = nVal & 0xff;
	output[1] = nVal >> 8;
	output[0] = nVal >> 16;
	return output+3;
}
char * put_be32(char *output, uint32_t nVal )
{
	output[3] = nVal & 0xff;
	output[2] = nVal >> 8;
	output[1] = nVal >> 16;
	output[0] = nVal >> 24;
	return output+4;
}
char *  put_be64( char *output, uint64_t nVal )
{
	output=put_be32( output, nVal >> 32 );
	output=put_be32( output, nVal );
	return output;
}
char * put_amf_string( char *c, const char *str )
{
	uint16_t len = strlen( str );
	c=put_be16( c, len );
	memcpy(c,str,len);
	return c+len;
}
char * put_amf_double( char *c, double d )
{
	*c++ = AMF_NUMBER;  /* type: Number */
	{
		unsigned char *ci, *co;
		ci = (unsigned char *)&d;
		co = (unsigned char *)c;
		co[0] = ci[7];
		co[1] = ci[6];
		co[2] = ci[5];
		co[3] = ci[4];
		co[4] = ci[3];
		co[5] = ci[2];
		co[6] = ci[1];
		co[7] = ci[0];
	}
	return c+8;
}

CRTMPStream::CRTMPStream(void):
m_pRtmp(NULL),
m_nFileBufSize(0),
m_nCurPos(0)
{
	m_pFileBuf = new unsigned char[FILEBUFSIZE];
	memset(m_pFileBuf,0,FILEBUFSIZE);
	InitSockets();
	m_pRtmp = RTMP_Alloc();
	RTMP_Init(m_pRtmp);
}

CRTMPStream::~CRTMPStream(void)
{
	Close();
	WSACleanup();
	delete[] m_pFileBuf;
}

bool CRTMPStream::Connect(const char* url)
{
	if(RTMP_SetupURL(m_pRtmp, (char*)url)<0)
	{
		return FALSE;
	}
	RTMP_EnableWrite(m_pRtmp);
	if(RTMP_Connect(m_pRtmp, NULL)<0)
	{
		return FALSE;
	}
	if(RTMP_ConnectStream(m_pRtmp,0)<0)
	{
		return FALSE;
	}
	return TRUE;
}

void CRTMPStream::Close()
{
	if(m_pRtmp)
	{
		RTMP_Close(m_pRtmp);
		RTMP_Free(m_pRtmp);
		m_pRtmp = NULL;
	}
}

int CRTMPStream::SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp)
{
	if(m_pRtmp == NULL)
	{
		return FALSE;
	}

	RTMPPacket packet;
	RTMPPacket_Reset(&packet);
	RTMPPacket_Alloc(&packet,size);

	packet.m_packetType = nPacketType;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nTimeStamp = nTimestamp;
	packet.m_nInfoField2 = m_pRtmp->m_stream_id;
	packet.m_nBodySize = size;

	packet.m_hasAbsTimestamp = 0;
	memcpy(packet.m_body,data,size);

	int nRet = RTMP_SendPacket(m_pRtmp,&packet,0);

	RTMPPacket_Free(&packet);

	return nRet;
}


bool CRTMPStream::SendMetadata(LPRTMPMetadata lpMetaData)
{
    if(lpMetaData == NULL)
    {
        return false;
    }
	char body[1024] = { 0 };
    char * p = (char *)body;
	p = put_byte(p, AMF_STRING);
	p = put_amf_string(p, "@setDataFrame");

	p = put_byte(p, AMF_STRING);
	p = put_amf_string(p, "onMetaData");

	p = put_byte(p, AMF_OBJECT);
	p = put_amf_string(p, "copyright");
	p = put_byte(p, AMF_STRING);
	p = put_amf_string(p, "firehood");


    // Video MetaData
	p = put_amf_string(p, "width");
	p = put_amf_double(p, lpMetaData->nWidth);

	p = put_amf_string(p, "height");
	p = put_amf_double(p, lpMetaData->nHeight);

	p = put_amf_string(p, "framerate");
	p = put_amf_double(p, lpMetaData->nFrameRate);

	p = put_amf_string(p, "videodatarate");
	p = put_amf_double(p, lpMetaData->nVideoDataRate);

	p = put_amf_string(p, "videocodecid");
	p = put_amf_double(p, FLV_CODECID_H264);

    // Audio MetaData
    if(lpMetaData->bHasAudio)
    {
        p = put_amf_string(p, "hasAudio");
        p = put_byte(p, AMF_BOOLEAN);
        *p++ = lpMetaData->bHasAudio ? 0x01 : 0x00;

        p = put_amf_string(p, "audiosamplerate");
        p = put_amf_double(p, lpMetaData->nAudioSampleRate);

        p = put_amf_string(p, "audiosamplesize");
        p = put_amf_double(p, lpMetaData->nAudioSampleSize);

        p = put_amf_string(p, "audiochannels");
        p = put_amf_double(p, lpMetaData->nAudioChannels);

        p = put_amf_string(p, "audiocodecid");
        p = put_amf_double(p, FLV_CODECID_AAC);
    }

    p = put_amf_string(p, "" );
    p = put_byte( p, AMF_OBJECT_END  );

    // 发送MetaData包
    SendPacket(RTMP_PACKET_TYPE_INFO, (unsigned char*)body, p - body, 0);

    // 发送视频MetaData的AVCDecoderConfigurationRecord
    if(lpMetaData->bHasVideo)
    {
    	int i = 0;
    	body[i++] = 0x17;                   // 1:keyframe  7: AVC
    	body[i++] = 0x00;                   // AVC Sequence header

    	body[i++] = 0x00;
    	body[i++] = 0x00;
    	body[i++] = 0x00;                   // fill in 0

    	/* AVCDecoderConfigurationVersion */
    	body[i++] = 0x01;                   // ConfigurationVersion
    	body[i++] = lpMetaData->Sps[1];       // AVCProfileIndication
    	body[i++] = lpMetaData->Sps[2];       // Profile compatibility
    	body[i++] = lpMetaData->Sps[3];       // AVCLevel Indication
    	body[i++] = 0xff;                   // LengthSizeMinusOne

    	/* SPS nums */
    	body[i++] = 0xE1;                   // &0x1f

    	/* SPS data length */
    	body[i++] = lpMetaData->nSpsLen >> 8;
    	body[i++] = lpMetaData->nSpsLen & 0xFF;

    	/* SPS data */
    	memcpy(&body[i], lpMetaData->Sps, lpMetaData->nSpsLen);
    	i = i + lpMetaData->nSpsLen;

    	/* PPS nums */
    	body[i++] = 0x01;                   // &0x1f
    	body[i++] = lpMetaData->nPpsLen >> 8;
    	body[i++] = lpMetaData->nPpsLen & 0xFF;

    	/* PPS data */
    	memcpy(&body[i], lpMetaData->Pps, lpMetaData->nPpsLen);
    	i = i + lpMetaData->nPpsLen;
    	SendPacket(RTMP_PACKET_TYPE_VIDEO, (unsigned char*)body, i, 0);
    }

    // 发送音频MetaData的AudioSpecificConfig
    if(lpMetaData->bHasAudio)
    {
        int i = 0;
        char audioBody[2] = {0};

        audioBody[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo)
        audioBody[i++] = 0x00; // AACPacketType: 0 (sequence header)

        // AudioSpecificConfig
        // 通常为2字节，可以从AACEncoder或音频源获取
        char aacSeqConfig[2];
        memcpy(aacSeqConfig, lpMetaData->AudioSpecCfg, lpMetaData->nAudioSpecCfgLen);
        // 构建完整的音频包
        char fullAudioBody[4];
        fullAudioBody[0] = 0xAF; // 配置
        fullAudioBody[1] = 0x00; // 序列头
        fullAudioBody[2] = aacSeqConfig[0];
        fullAudioBody[3] = aacSeqConfig[1];

        // 发送音频MetaData
        SendPacket(RTMP_PACKET_TYPE_AUDIO, (unsigned char*)fullAudioBody, 4, 0);
    }

    return true;
}


int get_freq_index(int sample_rate)
{
	switch (sample_rate)
	{
		case 96000: return 0;
		case 88200: return 1;
		case 64000: return 2;
		case 48000: return 3;
		case 44100: return 4;
		case 32000: return 5;
		case 24000: return 6;
		case 22050: return 7;
		case 16000: return 8;
		case 12000: return 9;
		case 11025: return 10;
		case 8000:  return 11;
		case 7350:  return 12;
		default:
			std::cerr << "Errpr SampleRate: " << sample_rate << "Hz" << std::endl;
		return 4; // 默认44100Hz
	}
}


bool CRTMPStream::SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp)
{
	if(data == NULL && size<11)
	{
		return false;
	}

	unsigned char *body = new unsigned char[size+9];

	int i = 0;
	if(bIsKeyFrame)
	{
		body[i++] = 0x17;// 1:Iframe  7:AVC
	}
	else
	{
		body[i++] = 0x27;// 2:Pframe  7:AVC
	}
	body[i++] = 0x01;// AVC NALU
	body[i++] = 0x00;
	body[i++] = 0x00;
	body[i++] = 0x00;

	// NALU size
	body[i++] = size>>24;
	body[i++] = size>>16;
	body[i++] = size>>8;
	body[i++] = size&0xff;;

	// NALU data
	memcpy(&body[i],data,size);

	bool bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO,body,i+size,nTimeStamp);

	delete[] body;

	return bRet;
}

bool CRTMPStream::SendH264File(const char *pFileName)
{
	if(pFileName == NULL)
	{
		return FALSE;
	}
	FILE *fp = fopen(pFileName, "rb");
	if(!fp)
	{
		printf("ERROR:open file %s failed!",pFileName);
	}
	fseek(fp, 0, SEEK_SET);
	m_nFileBufSize = fread(m_pFileBuf, sizeof(unsigned char), FILEBUFSIZE, fp);
	if(m_nFileBufSize >= FILEBUFSIZE)
	{
		printf("warning : File size is larger than BUFSIZE\n");
	}
	fclose(fp);

	RTMPMetadata metaData;
	memset(&metaData,0,sizeof(RTMPMetadata));

    NaluUnit naluUnit;
	// 读取SPS帧
    ReadOneNaluFromBuf(naluUnit);
	metaData.nSpsLen = naluUnit.size;
	memcpy(metaData.Sps,naluUnit.data,naluUnit.size);

	// 读取PPS帧
	ReadOneNaluFromBuf(naluUnit);
	metaData.nPpsLen = naluUnit.size;
	memcpy(metaData.Pps,naluUnit.data,naluUnit.size);

	// 解码SPS,获取视频图像宽、高信息
	int width = 0,height = 0;
    h264_decode_sps(metaData.Sps,metaData.nSpsLen,width,height);
	metaData.nWidth = width;
    metaData.nHeight = height;
	metaData.nFrameRate = 25;
	metaData.bHasVideo = true;

	// 发送MetaData
    SendMetadata(&metaData);

	unsigned int tick = 0;
	while(ReadOneNaluFromBuf(naluUnit))
	{
		bool bKeyframe  = (naluUnit.type == 0x05) ? TRUE : FALSE;
		// 发送H264数据帧
		SendH264Packet(naluUnit.data,naluUnit.size,bKeyframe,tick);
		msleep(40);
		tick +=40;
	}

	return TRUE;
}

bool CRTMPStream::SendYUVFile(const char *pFileName) {
	int width  = 720;
    int height = 480;

    // ==== 2. 打开YUV文件 ====
    FILE* fp_yuv = fopen(pFileName, "rb");
    if (!fp_yuv) {
        std::cerr << "Failed to open YUV file: " << pFileName << std::endl;
        return -1;
    }
    std::cout << "Opened YUV file: " << pFileName << "\n";

    // ==== 3. 初始化H264编码器 ====
    // 这里使用Properties或自己手动设置参数都可以
    Properties props;
    props.SetProperty("width",  width);
    props.SetProperty("height", height);
    props.SetProperty("fps",    25);            // 目标帧率
    props.SetProperty("bitrate", 800 * 1024);   // 800kbps 仅示例
    props.SetProperty("b_frames", 0);           // 不使用B帧，减低延迟
    props.SetProperty("gop", 25);               // GOP大小=帧率

    H264Encoder encoder;
    if (encoder.Init(props) < 0) {
        std::cerr << "H264Encoder init failed.\n";
        fclose(fp_yuv);
        return -1;
    }
    std::cout << "H264Encoder init OK.\n";

    // ==== 4. 获取SPS/PPS 信息，用来发送MetaData ====
    // 由于H264Encoder中已经在Init阶段解析了 extradata，所以可以直接getSPS/getPPS
    uint8_t spsData[1024] = {0};
    uint8_t ppsData[1024] = {0};
    int spsLen = 0;
    int ppsLen = 0;

    // 如果你的H264Encoder里提供了 getSPS/ getPPS 函数
    // 也可以用 encoder.getSPSData(), getSPSSize() 直接获取
    spsLen = encoder.getSPSSize();
    ppsLen = encoder.getPPSSize();
    if (spsLen > 0) {
        memcpy(spsData, encoder.getSPSData(), spsLen);
    }
    if (ppsLen > 0) {
        memcpy(ppsData, encoder.getPPSData(), ppsLen);
    }

    // 构建 MetaData
    RTMPMetadata metaData;
    memset(&metaData, 0, sizeof(RTMPMetadata));
    metaData.nSpsLen = spsLen;
    memcpy(metaData.Sps, spsData, spsLen);
    metaData.nPpsLen = ppsLen;
    memcpy(metaData.Pps, ppsData, ppsLen);
    metaData.nWidth     = encoder.getWidth();
    metaData.nHeight    = encoder.getHeight();
    metaData.nFrameRate = 25;  // 与编码器一致
	metaData.bHasVideo = true;
    // 如果有音频，可设置 metaData.bHasAudio = true 等，但此处仅演示视频
    // 发送MetaData(SPS/PPS)给RTMP服务器，让对端知晓解码参数
    SendMetadata(&metaData);
    std::cout << "Sent MetaData (SPS/PPS) to RTMP server.\n";

    // ==== 6. 读YUV数据，编码并推流 ====
    // YUV420P一帧大小 = width * height * 3 / 2
    const int frameSize = width * height * 3 / 2;
    uint8_t* yuvBuffer = new uint8_t[frameSize];

    int frameIndex = 0;
	auto start_time = std::chrono::steady_clock::now();
    while (true) {
        // 6.1 读取YUV一帧
        int ret = fread(yuvBuffer, 1, frameSize, fp_yuv);
        if (ret < frameSize) {
            std::cout << "End of YUV file or read error.\n";
            break;
        }

        // 6.2 使用H264Encoder编码，获取H.264数据
        //     这里演示使用 encoder.Encode(yuv, pts) -> AVPacket*
        AVPacket* packet = encoder.Encode(yuvBuffer, 0 /*pts*/);
        if (!packet) {
            // 编码器可能需要更多数据或者出现了EAGAIN
            // 也可能直接返回空，此处简单处理
            std::cerr << "Encoder returned null packet. skip.\n";
            continue;
        }

        // 6.3 拆分AVPacket->data 中的 NALU，并逐个发送
        //     简化做法：一般情况下 FFmpeg 默认会在 packet->data 里放一个或多个NALU
        //     常见情况：NALU 前面带有 00 00 00 01 startcode
        //     我们可以直接调用 rtmpSender.SendH264Packet() 发送这一整块，但需判断关键帧
        //     这里用简单方法：若 nal_unit_type == 5 => keyframe
        bool bIsKeyFrame = false;
        // 大多数时候可以通过 (packet->flags & AV_PKT_FLAG_KEY) 判断
        if (packet->flags & AV_PKT_FLAG_KEY) {
            bIsKeyFrame = true;
        }

        // RTMP 推流时，需要去掉多余的 00 00 00 01 start code 头等
        // 在H264Encoder里我们可能已经跳过4字节startcode，也可能保留
        // 这里假定 FFmpeg输出的packet带 startcode，则简单跳过头
        // 具体视 encoder 里 avcodec_send_frame/ avcodec_receive_packet 的实现
        int skipBytes = 0;
        if (packet->size > 4 && packet->data[0] == 0x00 &&
                                packet->data[1] == 0x00 &&
                                packet->data[2] == 0x00 &&
                                packet->data[3] == 0x01) {
            skipBytes = 4; // 跳过startcode
        }

        uint8_t* sendData = packet->data + skipBytes;
        int sendSize      = packet->size - skipBytes;

        // 6.4 送到 RTMP 流媒体服务器
        unsigned int timestamp = (unsigned int)(frameIndex * 40); // 假设25fps => 40ms 一帧

    	auto expected_time = start_time + std::chrono::milliseconds(static_cast<int>(frameIndex * 1000.0 / 25));
    	auto now = std::chrono::steady_clock::now();
    	if (expected_time > now) {
    		std::this_thread::sleep_until(expected_time);
    	}

        // sendData里可能包含多个NALU(含SPS/PPS/SEI/IDR等)，RTMP可以一次发送
        SendH264Packet(sendData, sendSize, bIsKeyFrame, timestamp);

        // 6.5 释放本次Packet
        av_packet_free(&packet);

        // 6.6 控制帧率(若需要做准实时推流)
        //     以25fps为例，sleep ~40ms。如果要超低延迟，可以考虑不sleep，
        //     但是网络+编码缓冲还是会有一定延迟
        // std::this_thread::sleep_for(std::chrono::milliseconds(5));

        frameIndex++;
    }

    // ==== 7. 结束推流, 释放资源 ====
    delete[] yuvBuffer;
    fclose(fp_yuv);
    Close();
    std::cout << "RTMP Stream finished.\n";

    return 0;
}

bool CRTMPStream::SendAACPacket(unsigned char *data, unsigned int size, unsigned int nTimeStamp) {
	if(data == NULL || size == 0)
	{
		return false;
	}

	unsigned char *body = new unsigned char[size + 2];
	int i = 0;

	body[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo)
	body[i++] = 0x01; // AACPacketType: 1 (raw AAC frame)

	memcpy(&body[i], data, size);

	bool bRet = SendPacket(RTMP_PACKET_TYPE_AUDIO, body, i + size, nTimeStamp);

	delete[] body;

	return bRet;
}


// 辅助函数：解析ADTS头部
bool ParseADTSHeader(const unsigned char* data, int dataLen, ADTSHeader& header)
{
	if (dataLen < 7) {
		header.valid = false;
		return false;
	}

	// 检查同步字
	if (data[0] != 0xFF || (data[1] & 0xF0) != 0xF0) {
		header.valid = false;
		return false;
	}

	// 解析ADTS头部字段
	header.profile = ((data[2] & 0xC0) >> 6) + 1; // profile: 1~4
	header.samplingFrequencyIndex = (data[2] & 0x3C) >> 2; // 采样率索引
	header.channelConfiguration = ((data[2] & 0x01) << 2) | ((data[3] & 0xC0) >> 6); // 声道数

	// 解析帧长度
	header.frameLength = ((data[3] & 0x03) << 11) |
						 (data[4] << 3) |
						 ((data[5] & 0xE0) >> 5);

	// 提取AudioSpecificConfig（前两个字节）
	// AudioSpecificConfig = profile (5 bits) + samplingFrequencyIndex (4 bits) + channelConfiguration (4 bits)
	header.AudioSpecificConfig[0] = ((header.profile & 0x07) << 3) | ((header.samplingFrequencyIndex & 0x0F) >> 1);
	header.AudioSpecificConfig[1] = ((header.samplingFrequencyIndex & 0x01) << 7) | ((header.channelConfiguration & 0x0F) << 3);

	header.valid = true;
	return true;
}

// 辅助函数：获取采样率
int GetSamplingFrequency(int samplingFrequencyIndex)
{
	static const int samplingFrequencies[] = {
		96000, // 0
		88200, // 1
		64000, // 2
		48000, // 3
		44100, // 4
		32000, // 5
		24000, // 6
		22050, // 7
		16000, // 8
		12000, // 9
		11025, // 10
		8000,  // 11
		7350,  // 12
		0,     // 13
		0,     // 14
		0      // 15
	};

	if (samplingFrequencyIndex < 0 || samplingFrequencyIndex > 12)
		return 44100; // 默认采样率

	return samplingFrequencies[samplingFrequencyIndex];
}


bool CRTMPStream::SendAACFile(const char *pFileName) {
    if(pFileName == NULL)
    {
        printf("Open file eror!\n");
        return FALSE;
    }
    FILE *fp = fopen(pFileName, "rb");
    if(!fp)
    {
        printf("Open file eror!\n", pFileName);
        return FALSE;
    }
    fseek(fp, 0, SEEK_SET);
    m_nFileBufSize = fread(m_pFileBuf, sizeof(unsigned char), FILEBUFSIZE, fp);
    if(m_nFileBufSize >= FILEBUFSIZE)
    {
    }
    fclose(fp);

    RTMPMetadata metaData;
    memset(&metaData, 0, sizeof(RTMPMetadata));

    // 解析第一个ADTS头部，获取AudioSpecificConfig
    ADTSHeader adtsHeader;
    if(!ParseADTSHeader(m_pFileBuf, m_nFileBufSize, adtsHeader))
    {
        printf("ERROR: Parse ADTS Header fail!\n");
        return FALSE;
    }

    // 设置音频元数据
    metaData.bHasAudio = true;
    metaData.nAudioSampleRate = GetSamplingFrequency(adtsHeader.samplingFrequencyIndex);
    metaData.nAudioSampleSize = 16; // 假设16位
    metaData.nAudioChannels = adtsHeader.channelConfiguration;
    metaData.AudioSpecCfg[0] = adtsHeader.AudioSpecificConfig[0];
    metaData.AudioSpecCfg[1] = adtsHeader.AudioSpecificConfig[1];
    metaData.nAudioSpecCfgLen = 2;

    // 发送音频元数据
    SendMetadata(&metaData);

    unsigned int tick = 0;
    unsigned int pos = 0;
	// 获取起始时间点
	auto start_time = std::chrono::steady_clock::now();
    while(pos + 7 <= m_nFileBufSize) // 至少需要7字节的ADTS头部
    {
        ADTSHeader currentHeader;
        if(!ParseADTSHeader(&m_pFileBuf[pos], m_nFileBufSize - pos, currentHeader))
        {
            pos += 1;
            continue;
        }

        // AAC帧数据开始的位置
        int aacFrameStart = pos + 7; // ADTS头部通常为7字节
        if(aacFrameStart + (currentHeader.frameLength - 7) > m_nFileBufSize)
        {
            break;
        }

        // 提取AAC帧数据
        unsigned char* aacData = &m_pFileBuf[aacFrameStart];
        int aacFrameSize = currentHeader.frameLength - 7;

    	auto expected_time = start_time + std::chrono::milliseconds(tick);
    	auto now = std::chrono::steady_clock::now();
    	if (expected_time > now) {
    		std::this_thread::sleep_until(expected_time);
    	}
        // 发送AAC帧
        SendAACPacket(aacData, aacFrameSize, tick);
        // printf("发送AAC帧: %d 字节, 时间戳: %u ms\n", aacFrameSize, tick);
		printf("Send AAC Timestamp : %u ms\n", tick);
        // 更新时间戳
        tick += 1024 * 1000 / metaData.nAudioSampleRate; // 假设每帧1024个采样点

        // 移动到下一个帧
        pos += currentHeader.frameLength;

    }

    printf("AAC push complete.\n");
    return TRUE;
}


bool CRTMPStream::ReadOneNaluFromBuf(NaluUnit &nalu)
{
	int i = m_nCurPos;
	while(i<m_nFileBufSize)
	{
		if(m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x01
			)
		{
			int pos = i;
			while (pos<m_nFileBufSize)
			{
				if(m_pFileBuf[pos++] == 0x00 &&
					m_pFileBuf[pos++] == 0x00 &&
					m_pFileBuf[pos++] == 0x00 &&
					m_pFileBuf[pos++] == 0x01
					)
				{
					break;
				}
			}
			if(pos == m_nFileBufSize)
			{
				nalu.size = pos-i;
			}
			else
			{
				nalu.size = (pos-4)-i;
			}
			nalu.type = m_pFileBuf[i]&0x1f;
			nalu.data = &m_pFileBuf[i];

			m_nCurPos = pos-4;
			return TRUE;
		}
	}
	return FALSE;
}
