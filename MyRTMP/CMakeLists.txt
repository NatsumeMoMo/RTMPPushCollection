# CMakeList.txt: MyRTMP 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)




# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("MyRTMP")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Encoder)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Push)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Tools)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/UI)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Capture)

# FFmpeg
set(FFMPEG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ffmpeg-6.1)
include_directories(${FFMPEG_DIR}/include)

# librtmp
set(LIBRTMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/librtmp)
include_directories(${LIBRTMP_DIR}/include)

# SDL
set(SDL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SDL2-2.30.10)
include_directories(${SDL2_DIR}/include)

# 源文件列表
set(SOURCES
    MyRTMP.cpp
        RTMP/RTMPBase.cpp
    Tools/dlog.cpp
    Capture/CommonLooper.cpp
    Capture/VideoCapture.cpp
    Capture/AudioCapture.cpp
    UI/VideoOutSDL.cpp
        Encoder/H264Encoder.h
        Tools/codecs.h
        Resampler/AudioResampler.cpp
        Resampler/AudioResampler.h
        Tools/Semaphore.h
        Tools/AVTimeBase.h
        Push/RTMPPusher.cpp
        Push/RTMPPusher.h
        Tools/mediabase.cpp
        Tools/AVTimeBase.cpp
)

# 将源代码添加到此项目的可执行文件。
add_executable (MyRTMP ${SOURCES} "Encoder/AACEncoder.cpp" "Encoder/H264Encoder.cpp"
    "Push/PushWork.cpp"
)

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
    ${SDL2_DIR}/lib/x64/SDL2.lib
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MyRTMP PROPERTY CXX_STANDARD 20)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DIR}/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# TODO: 如有需要，请添加测试并安装目标。
