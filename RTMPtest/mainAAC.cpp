// #include "RTMPStream.h"
// #include <iostream>
//
// int main()
// {
//     // 创建CRTMPStream实例
//     CRTMPStream rtmpStream;
//
//     // 连接到RTMP服务器
//     const char* rtmpUrl = "rtmp://localhost/live/livestream";
//     if(!rtmpStream.Connect(rtmpUrl))
//     {
//         std::cerr << "Connect RTMP Server error: " << rtmpUrl << std::endl;
//         return -1;
//     }
//
//     // 发送AAC文件
//     const char* aacFilePath = "buweishui.aac";
//     if(!rtmpStream.SendAACFile(aacFilePath))
//     {
//         std::cerr << "Push AAC fail: " << aacFilePath << std::endl;
//         rtmpStream.Close();
//         return -1;
//     }
//
//     // 断开连接
//     rtmpStream.Close();
//     std::cout << "Complete" << std::endl;
//
//     return 0;
// }