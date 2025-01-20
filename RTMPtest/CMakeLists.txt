# CMakeList.txt: MyRTMP 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)




# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("RTMPtest")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# FFmpeg
set(FFMPEG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ffmpeg-6.1)
include_directories(${FFMPEG_DIR}/include)

# librtmp
set(LIBRTMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/librtmp)
include_directories(${LIBRTMP_DIR}/include)


# 源文件列表
set(SOURCES
        SpsDecode.h
        RTMPStream.h
        RTMPStream.cpp
        H264Encoder.h
        H264Encoder.cpp
        mediabase.h
        mediabase.cpp
        mainFFmpeg.cpp
        AACEncoder.cpp
        AACEncoder.h
        codecs.h
        mainPCM.cpp
        AVTimeBase.cpp
        AVTimeBase.h
        TimeUtil.h
        mainAACEnc.cpp
        mainAAC.cpp
        mainYUV.cpp
        #        dlog.h
#        dlog.cpp
)

# 将源代码添加到此项目的可执行文件。
add_executable (RTMPtest ${SOURCES} )

# 链接FFmpeg库
target_link_libraries(${PROJECT_NAME}
        ${FFMPEG_DIR}/lib/avformat.lib
        ${FFMPEG_DIR}/lib/avcodec.lib
        ${FFMPEG_DIR}/lib/avdevice.lib
        ${FFMPEG_DIR}/lib/avfilter.lib
        ${FFMPEG_DIR}/lib/avutil.lib
        ${FFMPEG_DIR}/lib/postproc.lib
        ${FFMPEG_DIR}/lib/swresample.lib
        ${FFMPEG_DIR}/lib/swscale.lib
    ws2_32
    ${LIBRTMP_DIR}/lib/librtmp.lib
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RTMPtest PROPERTY CXX_STANDARD 20)
endif()


# TODO: 如有需要，请添加测试并安装目标。
