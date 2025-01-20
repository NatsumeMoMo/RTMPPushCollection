//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-29.
//

#include "VideoOutSDL.h"
#include "dlog.h"

// 自定义SDL事件
#define FRAME_REFRESH_EVENT (SDL_USEREVENT+1)

VideoOutSDL::VideoOutSDL()
{

}

VideoOutSDL::~VideoOutSDL()
{
    if(texture_)
        SDL_DestroyTexture(texture_);
    if(renderer_)
        SDL_DestroyRenderer(renderer_);
    if(win_)
        SDL_DestroyWindow(win_);
    if(mtx_)
        SDL_DestroyMutex(mtx_);
    SDL_Quit();
}

RET_CODE VideoOutSDL::Init(const Properties &props)
{
    // 初始化SDL
    if(SDL_Init(SDL_INIT_VIDEO))
    {
        LogError("Could not initialize SDL - %s", SDL_GetError());
        return RET_FAIL;
    }

    int x = props.GetProperty("win_x", (int)SDL_WINDOWPOS_UNDEFINED);
    video_width_ = props.GetProperty("video_width", 320);
    video_height_ = props.GetProperty("video_height", 240);

    // 创建窗口
    win_ = SDL_CreateWindow("Simple YUV player", x, SDL_WINDOWPOS_UNDEFINED,
                                video_width_, video_height_,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!win_)
    {
        LogError("SDL: could not create window, err:%s",SDL_GetError());
        return RET_FAIL;
    }

    // 基于窗口创建渲染器
    renderer_ = SDL_CreateRenderer(win_, -1, 0);
    if(!renderer_)
    {
        LogError("SDL: could not create renderer, err:%s",SDL_GetError());
        return RET_FAIL;
    }

    // 基于渲染器窗口纹理
    texture_ = SDL_CreateTexture(renderer_, pixformat_,
                                SDL_TEXTUREACCESS_STREAMING,
                                video_width_, video_height_);
    if(!texture_)
    {
        LogError("SDL: could not create texture, err:%s",SDL_GetError());
        return RET_FAIL;
    }

    videoBufSize_ = video_width_ * video_height_ * 1.5;
    videoBuf_ = (uint8_t*)malloc(videoBufSize_);
    mtx_ = SDL_CreateMutex();
    return RET_OK;
}

RET_CODE VideoOutSDL::Cache(uint8_t *video_buf, uint32_t size)
{
    SDL_LockMutex(mtx_);
    memcpy(videoBuf_, video_buf, size);
    SDL_UnlockMutex(mtx_);
    SDL_Event event;
    event.type = FRAME_REFRESH_EVENT;
    SDL_PushEvent(&event);
    return RET_OK;
}

RET_CODE VideoOutSDL::Output(uint8_t *video_buf, uint32_t size)
{
    SDL_LockMutex(mtx_);
    SDL_UpdateTexture(texture_, nullptr, video_buf, video_width_); // 设置纹理数据

    rect_ = SDL_Rect{ 0, 0, video_width_, video_height_ };

    SDL_RenderClear(renderer_); // 清除当前显示
    SDL_RenderCopy(renderer_, texture_, nullptr, &rect_); // 将纹理数据拷贝给渲染器
    SDL_RenderPresent(renderer_); // 显示
    SDL_UnlockMutex(mtx_);
    return RET_OK;
}

RET_CODE VideoOutSDL::Loop()
{
    while (1)
    {
        if(SDL_WaitEvent(&event_) != 1)
            continue;

        switch (event_.type)
        {
        case SDL_KEYDOWN:
            if(event_.key.keysym.sym == SDLK_ESCAPE)
                return RET_OK;
            if(event_.key.keysym.sym == SDLK_SPACE)
                return RET_OK;
            break;

        case SDL_QUIT:
            return RET_OK;
        case FRAME_REFRESH_EVENT:
            Output(videoBuf_, videoBufSize_);
            break;

        default:
            break;
        }
    }
}

