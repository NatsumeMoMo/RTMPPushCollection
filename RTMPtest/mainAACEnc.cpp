// // main.cpp
//
// #include "AACEncoder.h"
// #include <iostream>
// #include <fstream>
// #include <cstring> // for memset
//
// int main(int argc, char* argv[]) {
//     // 输入和输出文件路径
//     const char* input_pcm = "48000_2_s16le.pcm";
//     const char* output_aac = "output.aac";
//
//     // 打开输入 PCM 文件
//     std::ifstream pcm_file(input_pcm, std::ios::binary);
//     if (!pcm_file.is_open()) {
//         std::cerr << "Can't open input PCM file: " << input_pcm << std::endl;
//         return -1;
//     }
//
//     // 打开输出 AAC 文件
//     std::ofstream aac_file(output_aac, std::ios::binary);
//     if (!aac_file.is_open()) {
//         std::cerr << "Can't open output AAC file" << output_aac << std::endl;
//         pcm_file.close();
//         return -1;
//     }
//
//     // 创建并初始化 AACEncoder
//     AACEncoder encoder;
//     Properties props;
//     props.SetProperty("sample_rate", 48000);
//     props.SetProperty("bitrate", 128 * 1024); // 128 kbps
//     props.SetProperty("channels", 2);
//     props.SetProperty("channel_layout", static_cast<int>(av_get_default_channel_layout(2)));
//
//     RET_CODE ret = encoder.Init(props);
//     if (ret != RET_OK) {
//         std::cerr << "Init AACEncoder error" << std::endl;
//         pcm_file.close();
//         aac_file.close();
//         return -1;
//     }
//
//     // 计算每帧所需的字节数
//     int frame_size = encoder.getFrameSampleSize(); // 每帧的采样数
//     int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16); // 假设 PCM 是 S16 格式
//     int frame_byte_size = frame_size * bytes_per_sample * encoder.getChannels();
//
//     // 分配缓冲区
//     uint8_t* buffer = new (std::nothrow) uint8_t[frame_byte_size];
//     if (!buffer) {
//         std::cerr << "fail to allocate" << std::endl;
//         pcm_file.close();
//         aac_file.close();
//         return -1;
//     }
//
//     // 读取和编码 PCM 数据
//     while (pcm_file.read(reinterpret_cast<char*>(buffer), frame_byte_size)) {
//         AVPacket* pkt = encoder.Encode(buffer, frame_byte_size);
//         if (pkt) {
//             // 生成 ADTS 头
//             uint8_t adts_header[7];
//             encoder.getAdtsHeader(adts_header, pkt->size);
//             std::cout << "Encode size: " << pkt->size << std::endl;
//             // 写入 ADTS 头和 AAC 数据到输出文件
//             aac_file.write(reinterpret_cast<char*>(adts_header), 7);
//             aac_file.write(reinterpret_cast<char*>(pkt->data), pkt->size);
//
//             // 释放 AVPacket
//             av_packet_free(&pkt);
//         }
//     }
//
//     // 处理剩余的数据（不足一帧）
//     std::streamsize remaining = pcm_file.gcount();
//     if (remaining > 0) {
//         // 使用零填充不足的部分
//         memset(buffer + remaining, 0, frame_byte_size - remaining);
//         AVPacket* pkt = encoder.Encode(buffer, frame_byte_size);
//         if (pkt) {
//             uint8_t adts_header[7];
//             encoder.getAdtsHeader(adts_header, pkt->size);
//             aac_file.write(reinterpret_cast<char*>(adts_header), 7);
//             aac_file.write(reinterpret_cast<char*>(pkt->data), pkt->size);
//             av_packet_free(&pkt);
//         }
//     }
//
//     // 冲刷编码器，获取剩余的 AAC 数据
//     uint8_t* temp = nullptr;
//     AVPacket* pkt_flush = encoder.Encode(temp, 0);
//     while (pkt_flush) {
//         uint8_t adts_header[7];
//         encoder.getAdtsHeader(adts_header, pkt_flush->size);
//         aac_file.write(reinterpret_cast<char*>(adts_header), 7);
//         aac_file.write(reinterpret_cast<char*>(pkt_flush->data), pkt_flush->size);
//         av_packet_free(&pkt_flush);
//         pkt_flush = encoder.Encode(temp, 0);
//     }
//
//     // 清理资源
//     delete[] buffer;
//     pcm_file.close();
//     aac_file.close();
//
//     std::cout << "complete" << output_aac << std::endl;
//     return 0;
// }
