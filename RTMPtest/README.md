`RTMPStream` 类是整个推流RTMP Demo中最关键的代码。其中包含了发送MetaData、发送H.264Packet、发送AACPacket这些功能函数，还封装了librtmp负责与RTMP服务器进行交互，还有推流本地YUV、AAC文件的函数。



## mainAAC

调用RTMPStream中的函数推流本地AAC文件



## mainAACEnc

将本地PCM文件编码为AAC文件



## mainFFmpeg

读取本地YUV数据，将其编码为H.264后再使用FFmpeg API推流给RTMP服务器



## mainPCM

读取本地PCM数据，将其编码为AAC后再使用librtmp推流给RTMP服务器



## mainYUV

读取本地YUV数据，将其编码为H.264后再使用librtmp推流给RTMP服务器



## DOC.md

使用ChatGPT以这些推流代码为基础整理的推流文档。除了GPT整理的文档之外，在这基础上还添加了相关博客以及我自己整理的内容。