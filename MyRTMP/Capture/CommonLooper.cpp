//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 24-12-26.
//

#include "CommonLooper.h"
#include "dlog.h"

CommonLooper::CommonLooper()
{
    request_exit_ = false;
}

CommonLooper::~CommonLooper() {
    if (running_)
    {
        LogInfo("CommonLooper deleted while still running. Some messages will not be processed");
        Stop();
    }
}

RET_CODE CommonLooper::Start() {
    LogInfo("at CommonLooper create");
    thread_ = new std::thread(trampoline, this);
    if(thread_ == nullptr) {
        LogError("thread create failed");
        return RET_FAIL;
    }

    running_ = true;
    return RET_OK;

}

void CommonLooper::Stop() {
    request_exit_ = true;
    if(thread_) {
        thread_->join();
        delete thread_;
        thread_ = nullptr;
    }
    running_ = false;
}

void * CommonLooper::trampoline(void *arg) {
    LogInfo("at CommonLooper trampoline");
    ((CommonLooper*)arg)->Loop();
    return NULL;
}


