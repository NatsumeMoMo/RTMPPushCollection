//
// Created by JasonWang(https://github.com/NatsumeMoMo) on 25-1-2.
//

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <mutex>
#include <condition_variable>

/* 实现了一个基于互斥锁 (std::mutex) 和条件变量 (std::condition_variable_any) 的信号量机制。
 * 其主要功能包括信号量的初始化、增加 (post) 和等待 (wait) 操作。 */

class Semaphore
{
public:
    explicit Semaphore(unsigned int initial = 0) : count_(0) {}
    ~Semaphore() {}

    void post(unsigned int n = 1) {
        std::unique_lock<std::mutex> lock(mtx_);
        count_ += n;
        /* 根据 n 的值，选择调用 notify_one 或 notify_all。如果 n 为 1，则唤醒一个等待线程；否则唤醒所有等待线程。 */
        if(n == 1) {
            cv_.notify_one();

        } else {
            cv_.notify_all();
        }
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        /* 当 count_ 为 0 时，线程进入等待状态，直到有信号量被 post。 */
        while(count_ == 0) {
            cv_.wait(lock);
        }
        /* 一旦 count_ 大于 0，减小计数器 count_，继续执行。 */
        --count_;
    }

private:
    int count_;
    std::mutex mtx_;
    std::condition_variable_any cv_;
};

#endif //SEMAPHORE_H
