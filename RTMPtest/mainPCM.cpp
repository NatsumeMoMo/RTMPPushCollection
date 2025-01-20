 // #include <iostream>
 // #include <fstream>
 // #include <thread>
 // #include <chrono>
 // #include "RTMPStream.h"
 // #include "AACEncoder.h"
 // #include "mediabase.h"   // 你自定义的 Properties、RET_CODE 等头文件
 // #include "codecs.h"      // 你自定义的 AudioCodec 等头文件
 //
 // #ifdef _WIN32
 // #include <windows.h>
 // #endif
 //
 // // 简单的跨平台睡眠函数，Windows用Sleep(ms)，Linux/macOS用 usleep(ms*1000)
 // static void msleep(unsigned int ms)
 // {
 // #ifdef _WIN32
 //     ::Sleep(ms);
 // #else
 //     usleep(ms * 1000);
 // #endif
 // }
 //
 // /**
 //  * 示例说明:
 //  * 1. 假设输入 PCM 为 16bit little-endian，双声道(stereo)，采样率 48000。
 //  * 2. 代码会初始化 AACEncoder，帧大小由 FFmpeg 决定(通常AAC编码器内部会有一个frame_size，例如1024或其他)。
 //  * 3. 我们每次从 PCM 文件里读够一帧的样本数（frame_size * channels * 每采样字节数）后进行一次编码。
 //  * 4. 取得编码后返回的 AVPacket，即 AAC 码流（裸流，无ADTS头）。
 //  * 5. 调用 CRTMPStream::SendAACPacket() 推送到 RTMP Server。
 //  * 6. 其中音频元数据(例如 AudioSpecCfg)通过手动构造或从编码器信息中来，这里演示的是手动构造并调用 SendMetadata()。
 //  *
 //  * 注意事项:
 //  * - 如果项目中需要更精细的时间戳管理(例如更精准的同步)，可根据实际采样率和读取字节数来计算。示例中以比较简单的方式（累加帧时长）给出音频帧的时间戳。
 //  * - 如果需要更复杂的缓冲、时间戳调度、B 帧或延迟控制，请结合实际工程逻辑编写。
 //  */
 // int main()
 // {
 //     // ========== 1. 需要推送到的 RTMP 地址 & 本地 PCM 文件路径 =============
 //     std::string rtmp_url = "rtmp://localhost/live/livestream";
 //     std::string pcm_file = "48000_2_s16le.pcm";
 //     // 例如 test.pcm：双声道 48kHz 16bit LE
 //
 //     // ========== 2. 初始化 RTMP 连接 =============
 //     CRTMPStream rtmpSender;
 //     if (!rtmpSender.Connect(rtmp_url.c_str()))
 //     {
 //         std::cerr << "Failed to connect RTMP server: " << rtmp_url << std::endl;
 //         return -1;
 //     }
 //     std::cout << "Connected to RTMP Server: " << rtmp_url << std::endl;
 //
 //     // ========== 3. 初始化 AACEncoder =============
 //     // 根据实际情况设置属性, 这里示例假定 48000 Hz, 双声道, 128kbps
 //     Properties props;
 //     props.SetProperty("sample_rate", 48000);
 //     props.SetProperty("channels", 2);
 //     props.SetProperty("bitrate", 128 * 1024);
 //
 //     AACEncoder aacEncoder;
 //     if (aacEncoder.Init(props) != RET_OK)
 //     {
 //         std::cerr << "AACEncoder init failed!\n";
 //         return -1;
 //     }
 //     std::cout << "AACEncoder init OK.\n";
 //
 //     // ========== 4. 打开 PCM 文件 =============
 //     FILE* fp_pcm = fopen(pcm_file.c_str(), "rb");
 //     if (!fp_pcm)
 //     {
 //         std::cerr << "Failed to open PCM file: " << pcm_file << std::endl;
 //         return -1;
 //     }
 //     std::cout << "Opened PCM file: " << pcm_file << std::endl;
 //
 //     // ========== 5. 构造并发送音频 MetaData =============
 //     // 这里可以手动或者从编码器上下文里获取 AudioSpecCfg。演示使用与 RTMP 流兼容的方式。
 //     // 注意：AACEncoder 返回的是裸流，不包含 ADTS，所以我们只需将 AudioSpecCfg(2 字节) 发送给 RTMP
 //     RTMPMetadata metaData;
 //     memset(&metaData, 0, sizeof(RTMPMetadata));
 //     metaData.bHasAudio          = true;
 //     metaData.nAudioSampleRate   = aacEncoder.getSampleRate();    // 48000
 //     metaData.nAudioSampleSize   = 16;                            // 16 bit
 //     metaData.nAudioChannels     = aacEncoder.getChannels();       // 2
 //     metaData.nAudioSpecCfgLen   = 2;
 //
 //     {
 //         // 根据 AACEncoder 里的 sample_rate / channels / profile 等信息手动拼 2 字节 AudioSpecCfg
 //         // 参考 encode 中 getAdtsHeader 的方法，但只要前两字节
 //         // freqIdx + channelCfg + profile 组合成 AudioSpecCfg
 //         // 这里偷懒，用 aacEncoder 提供的 getAdtsHeader()，然后取前 2 字节即可
 //         uint8_t adtsHeader[7];
 //         aacEncoder.getAdtsHeader(adtsHeader, 0 /*aac_length先不重要*/);
 //         metaData.AudioSpecCfg[0] = 0x11;  // 第 2 字节为 profile(2bit) + freqIdx(4bit) + 第1bit通道
 //         metaData.AudioSpecCfg[1] = 0x90;  // 剩余bit + frameLen一部分，这里只保留通道信息相关bit
 //
 //         // 由于实际不同编码器可能 ADTS 中写法略有区别，如果对确切 bits 有严格要求，可根据你自己的采样率/声道数/profile
 //         // 拼出 2 字节 AudioSpecificConfig: 0x12, 0x10 等
 //     }
 //
 //     // 发送音视频MetaData(这里仅音频)
 //     rtmpSender.SendMetadata(&metaData);
 //     std::cout << "Sent audio metadata to RTMP server.\n";
 //
 //     // ========== 6. 读取PCM数据 -> 编码 -> 推流 =============
 //     // AAC 一帧通常为 1024 个采样点(具体取决于 FFmpeg 中 codec_ctx_->frame_size)，在双声道 16bit 下，一帧的字节大小可计算:
 //     //   frameSizeByte = frameSize * channelNum * 2
 //     // 不同情况下 frameSize 也可能是 960 等。我们从 codecCtx->frame_size 拿到一帧的样本数:
 //     int encoderFrameSize  = aacEncoder.getCodecCtx()->frame_size; // 通常1024
 //     int channels          = aacEncoder.getCodecCtx()->channels;   // 2
 //     int bytesPerSample    = 2;  // 假设16bit = 2字节
 //     int bytesPerFrame     = encoderFrameSize * channels * bytesPerSample;
 //
 //     // 用于读取 PCM 的临时缓冲
 //     std::vector<uint8_t> pcmBuffer(bytesPerFrame);
 //
 //     unsigned int audioTimestamp = 0;  // 递增单位: 毫秒
 //     // 可以用 chrono 进行时间戳同步
 //     auto start_time = std::chrono::steady_clock::now();
 //
 //     // 循环读取
 //     while (true)
 //     {
 //         // 6.1 从文件读取一帧 PCM
 //         size_t readLen = fread(pcmBuffer.data(), 1, bytesPerFrame, fp_pcm);
 //         if (readLen < (size_t)bytesPerFrame)
 //         {
 //             // 文件读完或不足一帧数据，这里简单退出，也可以补零帧
 //             std::cout << "End of PCM file or read not enough data.\n";
 //             break;
 //         }
 //
 //         // 6.2 编码这一帧 PCM -> AAC
 //         //     Encode() 如果编码成功，返回 AVPacket*(内部已分配)
 //         AVPacket* packet = aacEncoder.Encode(pcmBuffer.data(), bytesPerFrame);
 //         if (!packet)
 //         {
 //             std::cerr << "Encoder returned null packet, skip.\n";
 //             continue;
 //         }
 //
 //         // 6.3 获取编码后数据（裸 AAC frame, 无 ADTS），推送 RTMP
 //         // RTMP 不需要携带 ADTS 头，只要 AAC raw data 即可
 //         bool ok = rtmpSender.SendAACPacket(packet->data, packet->size, audioTimestamp);
 //         if (!ok)
 //         {
 //             std::cerr << "SendAACPacket failed!\n";
 //             av_packet_free(&packet);
 //             break;
 //         }
 //
 //         // 6.4 释放 packet
 //         av_packet_free(&packet);
 //
 //         // 6.5 根据采样率和一帧采样数，计算时间戳增量 (毫秒)
 //         //     对于 AAC，每帧通常 1024 个采样点 => 时间 = 1024 / sample_rate(秒)
 //         //     转成毫秒： (1024 / sample_rate) * 1000
 //         double frameDurationMs = 1024.0 / (double)metaData.nAudioSampleRate * 1000.0;
 //         audioTimestamp += (unsigned int)(frameDurationMs + 0.5);
 //
 //         // 6.6 控制推流速率，模拟实时发送
 //         //     若不需要“准实时”，可以省略
 //         auto expected_time = start_time + std::chrono::milliseconds(audioTimestamp);
 //         auto now = std::chrono::steady_clock::now();
 //         if (expected_time > now) {
 //             std::this_thread::sleep_until(expected_time);
 //         }
 //     }
 //
 //     // ========== 7. 收尾工作：关闭文件与 RTMP =============
 //     fclose(fp_pcm);
 //     rtmpSender.Close();
 //     std::cout << "RTMP push finished.\n";
 //     return 0;
 // }
