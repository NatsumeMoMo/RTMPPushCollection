// /**************************************************************
//  * File: main_rtmp_push.cpp
//  * Description:
//  *   1) 读取本地YUV文件（I420或YUV420P格式）
//  *   2) 使用H264Encoder编码
//  *   3) 使用FFmpeg 6.1 API 推流至RTMP服务器
//  **************************************************************/
//
// #include <chrono>
// #include <thread>
//
// extern "C" {
// #include <libavformat/avformat.h>
// #include <libavcodec/avcodec.h>
// #include <libavutil/imgutils.h>
// #include <libavutil/time.h>
// }
//
// #include <iostream>
// #include <fstream>
// #include <string>
// #include <memory>
// #include "H264Encoder.h"
//
//
// /**
//  * 读取一帧YUV420P数据
//  * @param inFile   已打开的YUV文件输入流
//  * @param buffer   存放读取结果的缓冲区指针
//  * @param frameSize 一帧YUV所需的字节数： width*height*3/2
//  * @return true: 成功读取一帧; false: 文件读取完毕或异常
//  */
// bool readOneFrame(std::ifstream &inFile, uint8_t* buffer, int frameSize) {
//     if (!inFile.good()) {
//         return false;
//     }
//     inFile.read(reinterpret_cast<char*>(buffer), frameSize);
//     if (inFile.gcount() < frameSize) {
//         return false;
//     }
//     return true;
// }
//
// static double r2d(AVRational r)
// {
//     return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
// }
//
//
// int main(int argc, char* argv[])
// {
//
//     std::string yuvFile   = "720x480_25fps_420p.yuv";
//     int width             = 720;
//     int height            = 480;
//     std::string rtmpUrl   = "rtmp://localhost/live/livestream";
//
//     // 2) 打开YUV文件
//     std::ifstream inFile(yuvFile, std::ios::binary);
//     if (!inFile.is_open()) {
//         std::cerr << "Failed to open input YUV file: " << yuvFile << std::endl;
//         return -1;
//     }
//
//     // 3) 初始化H264编码器
//     //    3.1) 先设置Properties
//     Properties props;
//     props.SetProperty("width",    width);
//     props.SetProperty("height",   height);
//     props.SetProperty("fps",      25);         // 可根据实际需求调整
//     props.SetProperty("bitrate",  600000);     // 600 kbps
//     props.SetProperty("gop",      25);         // gop大小
//     props.SetProperty("b_frames", 0);          // 不使用B帧，降低延迟
//
//     //    3.2) 创建并初始化H264Encoder
//     H264Encoder encoder;
//     if (encoder.Init(props) < 0) {
//         std::cerr << "Encoder Init failed." << std::endl;
//         return -1;
//     }
//
//     // 4) 初始化 FFmpeg RTMP 推流
//     //    4.1) 注册所有组件(对于FFmpeg6.x 通常不用手动注册, 这里写着以防环境不同)
//     // av_register_all();
//     avformat_network_init();
//
//     //    4.2) 为输出分配一个AVFormatContext
//     //         使用FLV格式 (RTMP通常封装为FLV)
//     AVFormatContext* pOutFormatCtx = nullptr;
//     avformat_alloc_output_context2(&pOutFormatCtx, nullptr, "flv", rtmpUrl.c_str());
//     if (!pOutFormatCtx) {
//         std::cerr << "Could not create Output Context." << std::endl;
//         return -1;
//     }
//
//     //    4.3) 新建一个视频流
//     AVStream* outStream = avformat_new_stream(pOutFormatCtx, nullptr);
//     if (!outStream) {
//         std::cerr << "Failed to create new stream." << std::endl;
//         avformat_free_context(pOutFormatCtx);
//         return -1;
//     }
//     outStream->time_base = {1, 25}; // 跟编码器配置保持一致, 或者用 encoder.getCodecCtx()->time_base
//
//     //    4.4) 复制编码器参数到输出流的codecpar (H264 extradata 同时要传过去)
//     //         (如果后续有重新打开编码器的需求, 可以先avcodec_parameters_from_context)
//     AVCodecContext* enc_ctx = encoder.getCodecCtx();
//     int ret = avcodec_parameters_from_context(outStream->codecpar, enc_ctx);
//     if (ret < 0) {
//         std::cerr << "Failed to copy codec parameters from encoder context." << std::endl;
//         avformat_free_context(pOutFormatCtx);
//         return -1;
//     }
//     outStream->codecpar->codec_tag = 0; // some FLV encoders want this to be 0
//
//     //    4.5) 打开输出的IO (如果是RTMP, 这里会进行网络连接)
//     if (!(pOutFormatCtx->oformat->flags & AVFMT_NOFILE)) {
//         ret = avio_open(&pOutFormatCtx->pb, rtmpUrl.c_str(), AVIO_FLAG_WRITE);
//         if (ret < 0) {
//             std::cerr << "Failed to open rtmp URL output." << std::endl;
//             avformat_free_context(pOutFormatCtx);
//             return -1;
//         }
//     }
//
//     //    4.6) 写文件头（即向 RTMP 服务器发送Header）
//     ret = avformat_write_header(pOutFormatCtx, nullptr);
//     if (ret < 0) {
//         std::cerr << "Error occurred when writing header to output URL." << std::endl;
//         if (!(pOutFormatCtx->oformat->flags & AVFMT_NOFILE)) {
//             avio_close(pOutFormatCtx->pb);
//         }
//         avformat_free_context(pOutFormatCtx);
//         return -1;
//     }
//
//     // 5) 循环读取YUV数据 -> 编码 -> 推流
//     //    一帧大小: YUV420P: width * height * 3/2
//     int frameSize = width * height * 3 / 2;
//     std::unique_ptr<uint8_t[]> yuvBuf(new uint8_t[frameSize]);
//
//     int64_t frameIndex = 0;
//     auto start_time = std::chrono::steady_clock::now();
//     while (true) {
//         // 5.1) 从文件读取一帧YUV
//         if (!readOneFrame(inFile, yuvBuf.get(), frameSize)) {
//             std::cout << "Reach end of YUV file or read error.\n";
//             break;
//         }
//
//         // 5.2) 进行H264编码，返回AVPacket*
//         //      这里调用的是: AVPacket* H264Encoder::Encode(uint8_t *yuv, uint64_t pts, const int flush)
//         AVPacket* pkt = encoder.Encode(yuvBuf.get(), /*pts=*/frameIndex, /*flush=*/0);
//         if (!pkt) {
//             // 可能是编码器内部缓冲没产出或者错误
//             // 继续读下一帧
//             frameIndex++;
//             continue;
//         }
//
//         // 5.3) 推流(把packet写到RTMP服务器)
//         //      需要注意时间戳（dts/pts 基于 outStream->time_base）
//         pkt->stream_index = outStream->index;
//         pkt->pts = frameIndex;
//         pkt->dts = frameIndex;
//         pkt->duration = 1;  // 每帧1个时间单位
//         frameIndex++;
//
//         // 时间基转换: 如果编码器的time_base跟outStream->time_base不一致时，需要用av_rescale_q
//         pkt->pts = av_rescale_q(pkt->pts, enc_ctx->time_base, outStream->time_base);
//         pkt->dts = av_rescale_q(pkt->dts, enc_ctx->time_base, outStream->time_base);
//
//
//         // 控制推流速率以匹配真实时间
//         auto expected_time = start_time + std::chrono::milliseconds(static_cast<int>(frameIndex * 1000.0 / 25)); // 0 + 5 * 40ms
//         auto now = std::chrono::steady_clock::now(); // 173ms ,  199
//         if (expected_time > now) {
//             std::this_thread::sleep_until(expected_time); // 27ms , 1ms
//         }
//
//         ret = av_interleaved_write_frame(pOutFormatCtx, pkt);
//         if (ret < 0) {
//             std::cerr << "Error muxing packet. ret=" << ret << std::endl;
//             av_packet_free(&pkt);
//             break;
//         }
//         av_packet_free(&pkt);
//         // sleep(40)-> 40ms
//     }
//
//     // 6) 文件读取完毕后, flush编码器(若编码器内部还有缓冲帧)
//     //    6.1) 这里调一下Encode传参 flush=1, 看是否还剩余数据
//     while (true) {
//         AVPacket* pkt = encoder.Encode(nullptr, 0, /*flush=*/1);
//         if (!pkt) {
//             break; // 编码器缓存flush完毕
//         }
//         pkt->stream_index = outStream->index;
//         pkt->pts = frameIndex;
//         pkt->dts = frameIndex;
//         pkt->duration = 1;
//         frameIndex++;
//
//         ret = av_interleaved_write_frame(pOutFormatCtx, pkt);
//         if (ret < 0) {
//             std::cerr << "Error muxing flush packet. ret=" << ret << std::endl;
//             av_packet_free(&pkt);
//             break;
//         }
//         av_packet_free(&pkt);
//     }
//
//     // 7) 写文件尾
//     av_write_trailer(pOutFormatCtx);
//
//     // 8) 资源释放
//     if (!(pOutFormatCtx->oformat->flags & AVFMT_NOFILE)) {
//         avio_close(pOutFormatCtx->pb);
//     }
//     avformat_free_context(pOutFormatCtx);
//     inFile.close();
//
//     std::cout << "RTMP push finished." << std::endl;
//     return 0;
// }
