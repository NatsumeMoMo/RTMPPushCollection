//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-29.
//

#ifndef VIDEOOUTSDL_H
#define VIDEOOUTSDL_H

#include <SDL.h>
#include <SDL_thread.h>
#include "mediabase.h"

// 窗口分辨率
#define WIN_WIDTH 320
#define WIN_HEIGHT 240

// YUV 像素分辨率
#define YUV_WIDTH 320
#define YUV_HEIGHT 240

// YUV格式
#define YUV_FORMAT SDL_PIXELFORMAT_IYUV

class VideoOutSDL
{
public:
    VideoOutSDL();
    ~VideoOutSDL();
    RET_CODE Init(const Properties& props);
    RET_CODE Cache(uint8_t* video_buf, uint32_t size);
    RET_CODE Output(uint8_t* video_buf, uint32_t size);
    RET_CODE Loop();

private:
    // SDL
    SDL_Event event_;                        // 事件
    SDL_Rect rect_;                          // 矩形
    SDL_Window* win_ = nullptr;              // 窗口
    SDL_Renderer* renderer_ = nullptr;       // 渲染
    SDL_Texture* texture_ = nullptr;         // 纹理
    uint32_t pixformat_ = YUV_FORMAT;        // YUV420P，即是SDL_PIXELFORMAT_IYUV
    SDL_mutex* mtx_;

    // 分辨率
    int video_width_ = YUV_WIDTH;
    int video_height_ = YUV_HEIGHT;
    int win_width_ = YUV_WIDTH;
    int win_height_ = YUV_HEIGHT;
    int videoBufSize_;
    uint8_t* videoBuf_ = nullptr;
};



#endif //VIDEOOUTSDL_H
