/********************************************************************
filename:   RTMPStream.cpp
created:    2013-04-3
author:     firehood
purpose:    ����H264��Ƶ��RTMP Server��ʹ��libRtmp��
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

#pragma warning(disable:4996)

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

char* put_byte(char* output, uint8_t nVal)
{
	output[0] = nVal;
	return output + 1;
}
char* put_be16(char* output, uint16_t nVal)
{
	output[1] = nVal & 0xff;
	output[0] = nVal >> 8;
	return output + 2;
}
char* put_be24(char* output, uint32_t nVal)
{
	output[2] = nVal & 0xff;
	output[1] = nVal >> 8;
	output[0] = nVal >> 16;
	return output + 3;
}
char* put_be32(char* output, uint32_t nVal)
{
	output[3] = nVal & 0xff;
	output[2] = nVal >> 8;
	output[1] = nVal >> 16;
	output[0] = nVal >> 24;
	return output + 4;
}
char* put_be64(char* output, uint64_t nVal)
{
	output = put_be32(output, nVal >> 32);
	output = put_be32(output, nVal);
	return output;
}
char* put_amf_string(char* c, const char* str)
{
	uint16_t len = strlen(str);
	c = put_be16(c, len);
	memcpy(c, str, len);
	return c + len;
}
char* put_amf_double(char* c, double d)
{
	*c++ = AMF_NUMBER;  /* type: Number */
	{
		unsigned char* ci, * co;
		ci = (unsigned char*)&d;
		co = (unsigned char*)c;
		co[0] = ci[7];
		co[1] = ci[6];
		co[2] = ci[5];
		co[3] = ci[4];
		co[4] = ci[3];
		co[5] = ci[2];
		co[6] = ci[1];
		co[7] = ci[0];
	}
	return c + 8;
}

CRTMPStream::CRTMPStream(void) :
	m_pRtmp(NULL),
	m_nFileBufSize(0),
	m_nCurPos(0)
{
	m_pFileBuf = new unsigned char[FILEBUFSIZE];
	memset(m_pFileBuf, 0, FILEBUFSIZE);
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
	if (RTMP_SetupURL(m_pRtmp, (char*)url) < 0)
	{
		return FALSE;
	}
	RTMP_EnableWrite(m_pRtmp);
	if (RTMP_Connect(m_pRtmp, NULL) < 0)
	{
		return FALSE;
	}
	if (RTMP_ConnectStream(m_pRtmp, 0) < 0)
	{
		return FALSE;
	}
	return TRUE;
}

void CRTMPStream::Close()
{
	if (m_pRtmp)
	{
		RTMP_Close(m_pRtmp);
		RTMP_Free(m_pRtmp);
		m_pRtmp = NULL;
	}
}

int CRTMPStream::SendPacket(unsigned int nPacketType, unsigned char* data, unsigned int size, unsigned int nTimestamp)
{
	if (m_pRtmp == NULL)
	{
		return FALSE;
	}

	RTMPPacket packet;
	RTMPPacket_Reset(&packet);
	RTMPPacket_Alloc(&packet, size);

	packet.m_packetType = nPacketType;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nTimeStamp = nTimestamp;
	packet.m_nInfoField2 = m_pRtmp->m_stream_id;
	packet.m_nBodySize = size;

	packet.m_hasAbsTimestamp = 0;
	memcpy(packet.m_body, data, size);

	int nRet = RTMP_SendPacket(m_pRtmp, &packet, 0);

	RTMPPacket_Free(&packet);

	return nRet;
}

// TODO: H264 and YUV MetaData

// bool CRTMPStream::SendMetadata(LPRTMPMetadata lpMetaData)
// {
// 	if(lpMetaData == NULL)
// 	{
// 		return false;
// 	}
// 	char body[1024] = {0};;
//
//     char * p = (char *)body;
// 	p = put_byte(p, AMF_STRING );
// 	p = put_amf_string(p , "@setDataFrame" );
//
// 	p = put_byte( p, AMF_STRING );
// 	p = put_amf_string( p, "onMetaData" );
//
// 	p = put_byte(p, AMF_OBJECT );
// 	p = put_amf_string( p, "copyright" );
// 	p = put_byte(p, AMF_STRING );
// 	p = put_amf_string( p, "firehood" );
//
// 	p =put_amf_string( p, "width");
// 	p =put_amf_double( p, lpMetaData->nWidth);
//
// 	p =put_amf_string( p, "height");
// 	p =put_amf_double( p, lpMetaData->nHeight);
//
// 	p =put_amf_string( p, "framerate" );
// 	p =put_amf_double( p, lpMetaData->nFrameRate);
//
// 	p =put_amf_string( p, "videocodecid" );
// 	p =put_amf_double( p, FLV_CODECID_H264 );
//
// 	p =put_amf_string( p, "" );
// 	p =put_byte( p, AMF_OBJECT_END  );
//
// 	int index = p-body;
//
// 	SendPacket(RTMP_PACKET_TYPE_INFO,(unsigned char*)body,p-body,0);
//
// 	int i = 0;
// 	body[i++] = 0x17; // 1:keyframe  7:AVC
// 	body[i++] = 0x00; // AVC sequence header
//
// 	body[i++] = 0x00;
// 	body[i++] = 0x00;
// 	body[i++] = 0x00; // fill in 0;
//
// 	// AVCDecoderConfigurationRecord.
// 	body[i++] = 0x01; // configurationVersion
// 	body[i++] = lpMetaData->Sps[1]; // AVCProfileIndication
// 	body[i++] = lpMetaData->Sps[2]; // profile_compatibility
// 	body[i++] = lpMetaData->Sps[3]; // AVCLevelIndication
//     body[i++] = 0xff; // lengthSizeMinusOne
//
//     // sps nums
// 	body[i++] = 0xE1; //&0x1f
// 	// sps data length
// 	body[i++] = lpMetaData->nSpsLen>>8;
// 	body[i++] = lpMetaData->nSpsLen&0xff;
// 	// sps data
// 	memcpy(&body[i],lpMetaData->Sps,lpMetaData->nSpsLen);
// 	i= i+lpMetaData->nSpsLen;
//
// 	// pps nums
// 	body[i++] = 0x01; //&0x1f
// 	// pps data length
// 	body[i++] = lpMetaData->nPpsLen>>8;
// 	body[i++] = lpMetaData->nPpsLen&0xff;
// 	// sps data
// 	memcpy(&body[i],lpMetaData->Pps,lpMetaData->nPpsLen);
// 	i= i+lpMetaData->nPpsLen;
//
// 	return SendPacket(RTMP_PACKET_TYPE_VIDEO,(unsigned char*)body,i,0);
//
// }

// TODO: AAC MetaData
bool CRTMPStream::SendMetadata(LPRTMPMetadata lpMetaData)
{
	if (lpMetaData == NULL)
	{
		return false;
	}
	char body[1024] = { 0 };
	char* p = (char*)body;
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
	if (lpMetaData->bHasAudio)
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

	p = put_amf_string(p, "");
	p = put_byte(p, AMF_OBJECT_END);

	// ����MetaData��
	SendPacket(RTMP_PACKET_TYPE_INFO, (unsigned char*)body, p - body, 0);

	// ������ƵMetaData��AVCDecoderConfigurationRecord
	if (lpMetaData->bHasVideo)
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

	// ������ƵMetaData��AudioSpecificConfig
	if (lpMetaData->bHasAudio)
	{
		int i = 0;
		char audioBody[2] = { 0 };

		audioBody[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo)
		audioBody[i++] = 0x00; // AACPacketType: 0 (sequence header)

		// AudioSpecificConfig
		// ͨ��Ϊ2�ֽڣ����Դ�AACEncoder����ƵԴ��ȡ
		char aacSeqConfig[2];
		memcpy(aacSeqConfig, lpMetaData->AudioSpecCfg, lpMetaData->nAudioSpecCfgLen);
		// ������������Ƶ��
		char fullAudioBody[4];
		fullAudioBody[0] = 0xAF; // ����
		fullAudioBody[1] = 0x00; // ����ͷ
		fullAudioBody[2] = aacSeqConfig[0];
		fullAudioBody[3] = aacSeqConfig[1];

		// ������ƵMetaData
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
		return 4; // Ĭ��44100Hz
	}
}


// bool CRTMPStream::SendAACSequenceHeader(AACEncoder *encoder) {
//
//     AVCodecContext* ctx = encoder->getCodecCtx();
//
//     // ��ȡ AAC �������
//     uint8_t profile = ctx->profile; // FFmpeg �� AAC LC ��Ӧ�� profile ͨ��Ϊ 2
//     int sample_rate = ctx->sample_rate;
//     int channels = ctx->channels;
//
//     // ��ȡ����������
//     int freq_index = get_freq_index(sample_rate);
//     if (freq_index == -1) {
//         std::cerr << "Invalid Index" << std::endl;
//         return false;
//     }
//
//     // ��������
//     uint8_t chan_config = channels; // ͨ������������ͬ
//
//     // ��װ AudioSpecificConfig��2 �ֽڣ�
//     uint8_t asc[2] = {0};
//     asc[0] = ((profile + 1) << 3) | (freq_index >> 1);
//     asc[1] = ((freq_index & 0x1) << 7) | (chan_config << 3);
//
//     // ���� FLV Audio Tag Data
//     uint8_t audio_tag[4] = {0};
//
//     // ���õ�һ���ֽڣ�
//     // - 4λ SoundFormat = 10��AAC��
//     // - 2λ SoundRate  = 3��44kHz������AAC����3��
//     // - 1λ SoundSize = 1��16 λ������AAC����1��
//     // - 1λ SoundType = 1��������������AAC����1��
//     audio_tag[0] = (10 << 4) | (3 << 2) | (1 << 1) | 1;
//
//     // �ڶ����ֽ�: AACPacketType = 0������ͷ��
//     audio_tag[1] = 0x00;
//
//     // ��� AudioSpecificConfig
//     audio_tag[2] = asc[0];
//     audio_tag[3] = asc[1];
//
//     // ���䲢���� RTMPPacket
//     RTMPPacket packet;
//     RTMPPacket_Reset(&packet);
//     RTMPPacket_Alloc(&packet, sizeof(audio_tag));
//
//     if (!packet.m_body) {
//         std::cerr << "SendAACSequenceHeader: RTMPPacket_Alloc error" << std::endl;
//         return false;
//     }
//
//     memcpy(packet.m_body, audio_tag, sizeof(audio_tag));
//     packet.m_nBodySize = sizeof(audio_tag);
//     packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
//     packet.m_nChannel = 0x04;     // ��Ƶͨ��ID
//     packet.m_nTimeStamp = 0;      // ����ͷʱ���Ϊ0
//     packet.m_hasAbsTimestamp = 0; // ���ʱ���
//     packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
//     packet.m_nInfoField2 = m_pRtmp->m_stream_id;
//
//     // ��� RTMP ����״̬
//     if (!RTMP_IsConnected(m_pRtmp)) {
//         std::cerr << "SendAACSequenceHeader: RTMP Connect error" << std::endl;
//         RTMPPacket_Free(&packet);
//         return false;
//     }
//
//     // ���� RTMP Packet��ʹ�ö��з��ͣ�����������Ϊ1��
//     if (!RTMP_SendPacket(m_pRtmp, &packet, 0)) {
//         std::cerr << "SendAACSequenceHeader: RTMP_SendPacket error" << std::endl;
//         RTMPPacket_Free(&packet);
//         return false;
//     }
//
//     // �ͷ� RTMPPacket ��Դ
//     RTMPPacket_Free(&packet);
//     return true;
// }

// bool CRTMPStream::SendAACPacket(const uint8_t *data, int len, uint32_t timestamp) {
// 	if (!data || len <= 0) {
// 		std::cerr << "SendAACPacket: Invalid data" << std::endl;
// 		return false;
// 	}
//
// 	// ���� FLV Audio Tag Data
// 	// - 1 �ֽڣ�[SoundFormat][SoundRate][SoundSize][SoundType]
// 	// - 1 �ֽڣ�AACPacketType = 1��AAC ԭʼ���ݣ�
// 	// - n �ֽڣ�AAC ����
// 	int bodySize = 2 + len;
// 	uint8_t* body = new uint8_t[bodySize];
// 	if (!body) {
// 		std::cerr << "Fail to allocate memory" << std::endl;
// 		return false;
// 	}
//
// 	// ���õ�һ���ֽ�
// 	// 4λ SoundFormat = 10��AAC��
// 	// 2λ SoundRate  = 3��44kHz������AAC����3��
// 	// 1λ SoundSize = 1��16 λ������AAC����1��
// 	// 1λ SoundType = 1��������������AAC����1��
// 	body[0] = (10 << 4) | (3 << 2) | (1 << 1) | 1;
//
// 	// ���õڶ����ֽ�
// 	body[1] = 0x01; // AACPacketType = 1��AAC ԭʼ���ݣ�
//
// 	// ���� AAC ����
// 	memcpy(&body[2], data, len);
//
// 	// ���� RTMP Packet
// 	bool result = SendPacket(RTMP_PACKET_TYPE_AUDIO, body, bodySize, timestamp);
//
// 	// �ͷ��ڴ�
// 	delete[] body;
//
// 	return result;
// }

// bool CRTMPStream::SendPCMFile(const char *pcmFileName) {
//     if (!pcmFileName) {
//         std::cerr << "Invalid pFile" << std::endl;
//         return false;
//     }
//     FILE* fp = fopen(pcmFileName, "rb");
//     if (!fp) {
//         std::cerr << "Open file error" << std::endl;
//         return false;
//     }
//
//     // ��ʼ�� AAC ������
//     Properties props;
//     props.SetProperty("sample_rate", 48000);
//     props.SetProperty("bitrate", 128 * 1024);
//     props.SetProperty("channels", 2);
//     props.SetProperty("channel_layout", (int)av_get_default_channel_layout(2));
//
//     AACEncoder aacEnc;
//     if (aacEnc.Init(props) != RET_OK) {
//         std::cerr << "AACEncoder Init Error" << std::endl;
//         fclose(fp);
//         return false;
//     }
//
// 	/* ------------ Send RTMP MetaData ------------- */
// 	char body[1024] = {0};;
//
// 	char * p = (char *)body;
// 	p = put_byte(p, AMF_STRING );
// 	p = put_amf_string(p , "@setDataFrame" );
//
// 	p = put_byte( p, AMF_STRING );
// 	p = put_amf_string( p, "onMetaData" );
//
// 	p = put_byte(p, AMF_OBJECT );
// 	p = put_amf_string( p, "copyright" );
// 	p = put_byte(p, AMF_STRING );
// 	p = put_amf_string( p, "firehood" );
//
// 	p = put_amf_string(p, "audiodatarate"); // (��Ƶ�������� / ������): ָ���Ǳ����� AAC ��Ƶ���ı����ʣ���λͨ���� bps��bits per second����
// 	p = put_amf_double(p, (double)128);
//
// 	p = put_amf_string(p, "audiosamplerate"); // (��Ƶ������): ָ����ԭʼ PCM ��Ƶ�Ĳ����ʣ���ÿ���Ӳ����Ĵ�������λ�� Hz (Hertz)��
// 	p = put_amf_double(p, (double)48000);
//
// 	p = put_amf_string(p, "audiosamplesize"); // (��Ƶ������С): ָ����ԭʼ PCM ��Ƶ��ÿ���������ö���λ����ʾ����λ�� bits��
// 	p = put_amf_double(p, (double)16);
//
// 	p = put_amf_string(p, "stereo"); //  (������): ��ʾ��Ƶ�Ƿ�Ϊ��������1 �����ǣ�0 �����ǣ�����������
// 	p = put_amf_double(p, (double)1);
//
// 	p = put_amf_string(p, "audiocodecid");
// 	p = put_amf_double(p, (double)10);
//
// 	p =put_amf_string( p, "" );
// 	p =put_byte( p, AMF_OBJECT_END  );
//
// 	int index = p-body;
//
// 	SendPacket(RTMP_PACKET_TYPE_INFO,(unsigned char*)body,p-body,0);
// 	/* ------------------------- */
//
//
//     // ���� AAC ����ͷ
//     if (!SendAACSequenceHeader(&aacEnc)) {
//         std::cerr << "Fail to run SendAACSequenceHeader" << std::endl;
//         fclose(fp);
//         return false;
//     }
//
//     // ��ȡ PCM ���ݲ����뷢��
//     // int frame_size = aacEnc.getFrameSampleSize(); // ͨ��Ϊ1024
// 	int frameSizeSamples = 1024;
// 	int sampleRate = 48000;
//     int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLTP); // float planar��ÿ������4�ֽ�
//     int frame_bytes = frameSizeSamples * bytes_per_sample * aacEnc.getChannels();
//
//     uint8_t* pcmBuf = new uint8_t[frame_bytes];
//
// 	// // ���� AAC ֡�ĳ���ʱ�� (����)
// 	// double aacFrameDurationMs = static_cast<double>(frameSizeSamples) / sampleRate * 1000.0;
// 	// int frameIndex = 0;
//
// 	// ����ÿ֡��ʱ������(����)
// 	uint32_t dts_inc = 1024 * 1000 / sampleRate;  // ��ZLMediaKitһ���ļ��㷽ʽ
// 	uint64_t current_dts = 0;  // ��ǰ֡DTS
//
// 	auto start_time = std::chrono::steady_clock::now();
//     while (true) {
//         size_t readCount = fread(pcmBuf, 1, frame_bytes, fp);
//         if (readCount < frame_bytes) {
//             // ��ȡ��ϻ����
//             break;
//         }
//
//         // ���� PCM ����
//         AVPacket* pkt = aacEnc.Encode(pcmBuf, frame_bytes);
//         if (!pkt) {
//             // ������������Ҫ��������
//             continue;
//         }
//
//      //    // ����ʱ���
//     	// uint32_t ptsMs = static_cast<uint32_t>(frameIndex * aacFrameDurationMs);
//      //
//     	// auto expected_time = start_time + std::chrono::milliseconds(static_cast<int>(frameIndex * aacFrameDurationMs));
//     	// auto now = std::chrono::steady_clock::now();
//     	// if (expected_time > now) {
//     	// 	std::this_thread::sleep_until(expected_time);
//     	// }
//
//
//     	// ���㵱ǰ֡Ӧ�÷��͵�����ʱ���
//     	auto expected_time = start_time + std::chrono::milliseconds(current_dts);
//
//     	// ��ȡ��ǰʱ��
//     	auto now = std::chrono::steady_clock::now();
//
//     	// �����û������ʱ��,��ȴ�
//     	if (expected_time > now) {
//     		std::this_thread::sleep_until(expected_time);
//     	} else {
//     		// ����ӳ�̫��(���糬��500ms),����У׼ʱ���׼
//     		auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(now - expected_time).count();
//     		if (delay > 500) {
//     			start_time = now;
//     			current_dts = 0;
//     		}
//     	}
//
//
//
//         // ���� AAC ���ݰ�
//         if (!SendAACPacket(pkt->data, pkt->size, current_dts)) {
//             std::cerr << "Fail to run SendAACPacket" << std::endl;
//             av_packet_free(&pkt);
//             break;
//         }
//
//         av_packet_free(&pkt);
//         // frameIndex++;
//     	// ������һ֡��DTS
//     	current_dts += dts_inc;
//     }
//
//     delete[] pcmBuf;
//     fclose(fp);
//     return true;
// }




bool CRTMPStream::SendH264Packet(unsigned char* data, unsigned int size, bool bIsKeyFrame, unsigned int nTimeStamp)
{
	if (data == NULL && size < 11)
	{
		return false;
	}

	unsigned char* body = new unsigned char[size + 9];

	int i = 0;
	if (bIsKeyFrame)
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
	body[i++] = size >> 24;
	body[i++] = size >> 16;
	body[i++] = size >> 8;
	body[i++] = size & 0xff;;

	// NALU data
	memcpy(&body[i], data, size);

	bool bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO, body, i + size, nTimeStamp);

	delete[] body;

	return bRet;
}

bool CRTMPStream::SendH264File(const char* pFileName)
{
	if (pFileName == NULL)
	{
		return FALSE;
	}
	FILE* fp = fopen(pFileName, "rb");
	if (!fp)
	{
		printf("ERROR:open file %s failed!", pFileName);
	}
	fseek(fp, 0, SEEK_SET);
	m_nFileBufSize = fread(m_pFileBuf, sizeof(unsigned char), FILEBUFSIZE, fp);
	if (m_nFileBufSize >= FILEBUFSIZE)
	{
		printf("warning : File size is larger than BUFSIZE\n");
	}
	fclose(fp);

	RTMPMetadata metaData;
	memset(&metaData, 0, sizeof(RTMPMetadata));

	NaluUnit naluUnit;
	// ��ȡSPS֡
	ReadOneNaluFromBuf(naluUnit);
	metaData.nSpsLen = naluUnit.size;
	memcpy(metaData.Sps, naluUnit.data, naluUnit.size);

	// ��ȡPPS֡
	ReadOneNaluFromBuf(naluUnit);
	metaData.nPpsLen = naluUnit.size;
	memcpy(metaData.Pps, naluUnit.data, naluUnit.size);

	// ����SPS,��ȡ��Ƶͼ�������Ϣ
	int width = 0, height = 0;
	h264_decode_sps(metaData.Sps, metaData.nSpsLen, width, height);
	metaData.nWidth = width;
	metaData.nHeight = height;
	metaData.nFrameRate = 25;

	// ����MetaData
	SendMetadata(&metaData);

	unsigned int tick = 0;
	while (ReadOneNaluFromBuf(naluUnit))
	{
		bool bKeyframe = (naluUnit.type == 0x05) ? TRUE : FALSE;
		// ����H264����֡
		SendH264Packet(naluUnit.data, naluUnit.size, bKeyframe, tick);
		msleep(40);
		tick += 40;
	}

	return TRUE;
}

bool CRTMPStream::SendYUVFile(const char* pFileName) {
	int width = 720;
	int height = 480;

	// ==== 2. ��YUV�ļ� ====
	FILE* fp_yuv = fopen(pFileName, "rb");
	if (!fp_yuv) {
		std::cerr << "Failed to open YUV file: " << pFileName << std::endl;
		return -1;
	}
	std::cout << "Opened YUV file: " << pFileName << "\n";

	// ==== 3. ��ʼ��H264������ ====
	// ����ʹ��Properties���Լ��ֶ����ò���������
	Properties props;
	props.SetProperty("width", width);
	props.SetProperty("height", height);
	props.SetProperty("fps", 25);            // Ŀ��֡��
	props.SetProperty("bitrate", 800 * 1024);   // 800kbps ��ʾ��
	props.SetProperty("b_frames", 0);           // ��ʹ��B֡�������ӳ�
	props.SetProperty("gop", 25);               // GOP��С=֡��

	H264Encoder encoder;
	if (encoder.Init(props) < 0) {
		std::cerr << "H264Encoder init failed.\n";
		fclose(fp_yuv);
		return -1;
	}
	std::cout << "H264Encoder init OK.\n";

	// ==== 4. ��ȡSPS/PPS ��Ϣ����������MetaData ====
	// ����H264Encoder���Ѿ���Init�׶ν����� extradata�����Կ���ֱ��getSPS/getPPS
	uint8_t spsData[1024] = { 0 };
	uint8_t ppsData[1024] = { 0 };
	int spsLen = 0;
	int ppsLen = 0;

	// ������H264Encoder���ṩ�� getSPS/ getPPS ����
	// Ҳ������ encoder.getSPSData(), getSPSSize() ֱ�ӻ�ȡ
	spsLen = encoder.getSPSSize();
	ppsLen = encoder.getPPSSize();
	if (spsLen > 0) {
		memcpy(spsData, encoder.getSPSData(), spsLen);
	}
	if (ppsLen > 0) {
		memcpy(ppsData, encoder.getPPSData(), ppsLen);
	}

	// ���� MetaData
	RTMPMetadata metaData;
	memset(&metaData, 0, sizeof(RTMPMetadata));
	metaData.nSpsLen = spsLen;
	memcpy(metaData.Sps, spsData, spsLen);
	metaData.nPpsLen = ppsLen;
	memcpy(metaData.Pps, ppsData, ppsLen);
	metaData.nWidth = encoder.getWidth();
	metaData.nHeight = encoder.getHeight();
	metaData.nFrameRate = 25;  // �������һ��
	metaData.bHasVideo = true;
	// �������Ƶ�������� metaData.bHasAudio = true �ȣ����˴�����ʾ��Ƶ
	// ����MetaData(SPS/PPS)��RTMP���������öԶ�֪���������
	SendMetadata(&metaData);
	std::cout << "Sent MetaData (SPS/PPS) to RTMP server.\n";

	// ==== 6. ��YUV���ݣ����벢���� ====
	// YUV420Pһ֡��С = width * height * 3 / 2
	const int frameSize = width * height * 3 / 2;
	uint8_t* yuvBuffer = new uint8_t[frameSize];

	int frameIndex = 0;
	auto start_time = std::chrono::steady_clock::now();
	while (true) {
		// 6.1 ��ȡYUVһ֡
		int ret = fread(yuvBuffer, 1, frameSize, fp_yuv);
		if (ret < frameSize) {
			std::cout << "End of YUV file or read error.\n";
			break;
		}

		// 6.2 ʹ��H264Encoder���룬��ȡH.264����
		//     ������ʾʹ�� encoder.Encode(yuv, pts) -> AVPacket*
		AVPacket* packet = encoder.Encode(yuvBuffer, 0 /*pts*/);
		if (!packet) {
			// ������������Ҫ�������ݻ��߳�����EAGAIN
			// Ҳ����ֱ�ӷ��ؿգ��˴��򵥴���
			std::cerr << "Encoder returned null packet. skip.\n";
			continue;
		}

		// 6.3 ���AVPacket->data �е� NALU�����������
		//     ��������һ������� FFmpeg Ĭ�ϻ��� packet->data ���һ������NALU
		//     ���������NALU ǰ����� 00 00 00 01 startcode
		//     ���ǿ���ֱ�ӵ��� rtmpSender.SendH264Packet() ������һ���飬�����жϹؼ�֡
		//     �����ü򵥷������� nal_unit_type == 5 => keyframe
		bool bIsKeyFrame = false;
		// �����ʱ�����ͨ�� (packet->flags & AV_PKT_FLAG_KEY) �ж�
		if (packet->flags & AV_PKT_FLAG_KEY) {
			bIsKeyFrame = true;
		}

		// RTMP ����ʱ����Ҫȥ������� 00 00 00 01 start code ͷ��
		// ��H264Encoder�����ǿ����Ѿ�����4�ֽ�startcode��Ҳ���ܱ���
		// ����ٶ� FFmpeg�����packet�� startcode���������ͷ
		// ������ encoder �� avcodec_send_frame/ avcodec_receive_packet ��ʵ��
		int skipBytes = 0;
		if (packet->size > 4 && packet->data[0] == 0x00 &&
			packet->data[1] == 0x00 &&
			packet->data[2] == 0x00 &&
			packet->data[3] == 0x01) {
			skipBytes = 4; // ����startcode
		}

		uint8_t* sendData = packet->data + skipBytes;
		int sendSize = packet->size - skipBytes;

		// 6.4 �͵� RTMP ��ý�������
		unsigned int timestamp = (unsigned int)(frameIndex * 40); // ����25fps => 40ms һ֡

		auto expected_time = start_time + std::chrono::milliseconds(static_cast<int>(frameIndex * 1000.0 / 25));
		auto now = std::chrono::steady_clock::now();
		if (expected_time > now) {
			std::this_thread::sleep_until(expected_time);
		}

		// sendData����ܰ������NALU(��SPS/PPS/SEI/IDR��)��RTMP����һ�η���
		SendH264Packet(sendData, sendSize, bIsKeyFrame, timestamp);

		// 6.5 �ͷű���Packet
		av_packet_free(&packet);

		// 6.6 ����֡��(����Ҫ��׼ʵʱ����)
		//     ��25fpsΪ����sleep ~40ms�����Ҫ�����ӳ٣����Կ��ǲ�sleep��
		//     ��������+���뻺�廹�ǻ���һ���ӳ�
		// std::this_thread::sleep_for(std::chrono::milliseconds(5));

		frameIndex++;
	}

	// ==== 7. ��������, �ͷ���Դ ====
	delete[] yuvBuffer;
	fclose(fp_yuv);
	Close();
	std::cout << "RTMP Stream finished.\n";

	return 0;
}

bool CRTMPStream::SendAACPacket(unsigned char* data, unsigned int size, unsigned int nTimeStamp) {
	if (data == NULL || size == 0)
	{
		return false;
	}

	unsigned char* body = new unsigned char[size + 2];
	int i = 0;

	body[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo)
	body[i++] = 0x01; // AACPacketType: 1 (raw AAC frame)

	memcpy(&body[i], data, size);

	bool bRet = SendPacket(RTMP_PACKET_TYPE_AUDIO, body, i + size, nTimeStamp);

	delete[] body;

	return bRet;
}


// ��RTMPStream.cpp�Ķ������

// struct ADTSHeader
// {
// 	bool valid;
// 	int profile;
// 	int samplingFrequencyIndex;
// 	int channelConfiguration;
// 	int frameLength;
// 	unsigned char AudioSpecificConfig[2];
// };

// ��������������ADTSͷ��
bool ParseADTSHeader(const unsigned char* data, int dataLen, ADTSHeader& header)
{
	if (dataLen < 7) {
		header.valid = false;
		return false;
	}

	// ���ͬ����
	if (data[0] != 0xFF || (data[1] & 0xF0) != 0xF0) {
		header.valid = false;
		return false;
	}

	// ����ADTSͷ���ֶ�
	header.profile = ((data[2] & 0xC0) >> 6) + 1; // profile: 1~4
	header.samplingFrequencyIndex = (data[2] & 0x3C) >> 2; // ����������
	header.channelConfiguration = ((data[2] & 0x01) << 2) | ((data[3] & 0xC0) >> 6); // ������

	// ����֡����
	header.frameLength = ((data[3] & 0x03) << 11) |
		(data[4] << 3) |
		((data[5] & 0xE0) >> 5);

	// ��ȡAudioSpecificConfig��ǰ�����ֽڣ�
	// AudioSpecificConfig = profile (5 bits) + samplingFrequencyIndex (4 bits) + channelConfiguration (4 bits)
	header.AudioSpecificConfig[0] = ((header.profile & 0x07) << 3) | ((header.samplingFrequencyIndex & 0x0F) >> 1);
	header.AudioSpecificConfig[1] = ((header.samplingFrequencyIndex & 0x01) << 7) | ((header.channelConfiguration & 0x0F) << 3);

	header.valid = true;
	return true;
}

// ������������ȡ������
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
		return 44100; // Ĭ�ϲ�����

	return samplingFrequencies[samplingFrequencyIndex];
}


bool CRTMPStream::SendAACFile(const char* pFileName) {
	if (pFileName == NULL)
	{
		printf("Open file eror!\n");
		return FALSE;
	}
	FILE* fp = fopen(pFileName, "rb");
	if (!fp)
	{
		printf("Open file eror!\n", pFileName);
		return FALSE;
	}
	fseek(fp, 0, SEEK_SET);
	m_nFileBufSize = fread(m_pFileBuf, sizeof(unsigned char), FILEBUFSIZE, fp);
	if (m_nFileBufSize >= FILEBUFSIZE)
	{
	}
	fclose(fp);

	RTMPMetadata metaData;
	memset(&metaData, 0, sizeof(RTMPMetadata));

	// ������һ��ADTSͷ������ȡAudioSpecificConfig
	ADTSHeader adtsHeader;
	if (!ParseADTSHeader(m_pFileBuf, m_nFileBufSize, adtsHeader))
	{
		printf("ERROR: Parse ADTS Header fail!\n");
		return FALSE;
	}

	// ������ƵԪ����
	metaData.bHasAudio = true;
	metaData.nAudioSampleRate = GetSamplingFrequency(adtsHeader.samplingFrequencyIndex);
	metaData.nAudioSampleSize = 16; // ����16λ
	metaData.nAudioChannels = adtsHeader.channelConfiguration;
	metaData.AudioSpecCfg[0] = adtsHeader.AudioSpecificConfig[0];
	metaData.AudioSpecCfg[1] = adtsHeader.AudioSpecificConfig[1];
	metaData.nAudioSpecCfgLen = 2;

	// ������ƵԪ����
	SendMetadata(&metaData);

	unsigned int tick = 0;
	unsigned int pos = 0;
	// ��ȡ��ʼʱ���
	auto start_time = std::chrono::steady_clock::now();
	while (pos + 7 <= m_nFileBufSize) // ������Ҫ7�ֽڵ�ADTSͷ��
	{
		ADTSHeader currentHeader;
		if (!ParseADTSHeader(&m_pFileBuf[pos], m_nFileBufSize - pos, currentHeader))
		{
			pos += 1;
			continue;
		}

		// AAC֡���ݿ�ʼ��λ��
		int aacFrameStart = pos + 7; // ADTSͷ��ͨ��Ϊ7�ֽ�
		if (aacFrameStart + (currentHeader.frameLength - 7) > m_nFileBufSize)
		{
			break;
		}

		// ��ȡAAC֡����
		unsigned char* aacData = &m_pFileBuf[aacFrameStart];
		int aacFrameSize = currentHeader.frameLength - 7;

		auto expected_time = start_time + std::chrono::milliseconds(tick);
		auto now = std::chrono::steady_clock::now();
		if (expected_time > now) {
			std::this_thread::sleep_until(expected_time);
		}
		// ����AAC֡
		SendAACPacket(aacData, aacFrameSize, tick);
		// printf("����AAC֡: %d �ֽ�, ʱ���: %u ms\n", aacFrameSize, tick);
		printf("Send AAC Timestamp : %u ms\n", tick);
		// ����ʱ���
		tick += 1024 * 1000 / metaData.nAudioSampleRate; // ����ÿ֡1024��������

		// �ƶ�����һ��֡
		pos += currentHeader.frameLength;

	}

	printf("AAC push complete.\n");
	return TRUE;
}


bool CRTMPStream::ReadOneNaluFromBuf(NaluUnit& nalu)
{
	int i = m_nCurPos;
	while (i < m_nFileBufSize)
	{
		if (m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x00 &&
			m_pFileBuf[i++] == 0x01
			)
		{
			int pos = i;
			while (pos < m_nFileBufSize)
			{
				if (m_pFileBuf[pos++] == 0x00 &&
					m_pFileBuf[pos++] == 0x00 &&
					m_pFileBuf[pos++] == 0x00 &&
					m_pFileBuf[pos++] == 0x01
					)
				{
					break;
				}
			}
			if (pos == m_nFileBufSize)
			{
				nalu.size = pos - i;
			}
			else
			{
				nalu.size = (pos - 4) - i;
			}
			nalu.type = m_pFileBuf[i] & 0x1f;
			nalu.data = &m_pFileBuf[i];

			m_nCurPos = pos - 4;
			return TRUE;
		}
	}
	return FALSE;
}
