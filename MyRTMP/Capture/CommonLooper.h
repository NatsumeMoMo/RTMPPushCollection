//
// Created JasonWang(https://github.com/NatsumeMoMo) on 24-12-26.
//

#ifndef COMMONLOPPER_H
#define COMMONLOPPER_H

#include <thread>
#include "mediabase.h"

#include <functional>
using CAPTURECALLBACK = std::function<void(uint8_t*, int32_t, unsigned int)>;

class CommonLooper {
public:
    CommonLooper();
    virtual ~CommonLooper();
    virtual RET_CODE Start();
    virtual void Stop();
    virtual void Loop() = 0;

private:
    static void* trampoline(void* arg);

protected:
    std::thread* thread_;
    bool request_exit_ = false;
    bool running_ = false;
};



#endif //COMMONLOPPER_H
