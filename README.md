# RTMP Push Collection

## Introduction

本项目是基于C++11编写的，第三方库为SDL2-2.30.10、librtmp和FFmpeg 6.1版本实现的。RTMP服务器使用的是添加了RTMP模块的Nginx（运行在WSL2中），系统为Windows11 24H2、Ubuntu24.04.1 LTS（WSL2），编译器为MSVC 2022。代码使用CMake或者VS sln进行管理。

这些代码是以相关博客或者开源代码为思路，然后经过我的处理整合后搭建起整体的代码框架，最后再由我去或多或少地修改调整这些代码中的细节问题，达到一个可以接受的效果



## Project Explanation

### RTMPTest

该目录中包含的是简单的推流单一类型数据的Demo。即使用librtmp推流AAC文件、编码YUV为H.264后再推流、编码为PCM为AAC后再推流，以及使用FFmpeg进行RTMP推流。详细说明请看该目录下的README。该目录下的MarkDown文件中有对单一推流的整理文档，便于进一步了解代码，使用CMake进行管理。开始前请先阅读该目录下的README



### RTMPPushMedia

该目录包含的是简单的同时采集原生YUV和PCM数据，对其进行编码后再推流的Demo。使用Visual Studio Sln进行管理



### MyRTMP

该目录下包含的同样是同时采集原生YUV和PCM数据，对其进行编码后再推流的Demo，但具有进一步的架构设计，`RTMPPushMedia` 可以看作是该Demo的简化版本。该项目在推流的同时还会使用SDL将读取的YUV数据渲染出来，便于对比拉流的延迟。该目录下有该Demo的架构图，便于快速上手。





## Quick Start

H.264、AAC、YUV和PCM这些流媒体数据，如果您使用Visual Studio打开CMake项目的话，那么先构建，然后将这两份文件放在 `.\out\build\x64-Debug` 路径下，对于sln项目则将数据放在 `.h\.cpp` 文件的同级路径下。若您使用使用CLion打开CMake项目，那么就将数据放在 `.\cmake-build-debug` 下。或者使用修改代码使用绝对路径。

推流地址需要根据您的实际地址进行替换。



第三方库放在了和媒体数据分别上传到了百度网盘中. 下载后将第三方库中的ffmpeg、librtmp和SDL放在 `thirdparty` 目录下



thirdparty：

```
https://pan.baidu.com/s/1AnyBmGXG32RBlEP0LJmh2w?pwd=jkj9 提取码: jkj9 
```



MediaData:

```
https://pan.baidu.com/s/1LJWdTMiJn97spy_15Cd1Ng?pwd=ffay 提取码: ffay 
```





**Gitee地址**：

https://gitee.com/montybot/rtmppush-collection
