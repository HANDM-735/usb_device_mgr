#ifndef SAFE_QUEUE_TEMPLATE_H_H
#define SAFE_QUEUE_TEMPLATE_H_H

#include <chrono>
#include <list>
#include <limits>
#include <atomic>
#include <condition_variable>
#include <mutex>

template<typename T>
struct SafeQueue : private std::mutex
{
    //static const int wait_infinite = std::numeric_limits<int>::max();
    static const int wait_infinite = std::numeric_limits<int>::infinity();
    SafeQueue(size_t capacity = 5000) : capacity_(capacity), exit_(false) {}

    bool Push(T& v);
    bool PushEx(T& v);
    T Pop_Wait(int waitMs = wait_infinite);
    bool Pop_Wait(T* v, int waitMs = wait_infinite);
    bool Pop(T* v);
    size_t Size();
    void Exit();
    void Clear();
    bool Empty();
    bool Exited() { return exit_; }
    void SetCapacity(size_t capacity) {capacity_ = capacity;}

private:
    std::list<T> items_;
    std::condition_variable ready_;
    size_t capacity_;
    std::atomic<bool> exit_;
    void Wait_Ready(std::unique_lock<std::mutex>& lk, int waitMs);
};

template <typename T> void SafeQueue<T>::Clear()
{
    std::lock_guard<std::mutex> lk(*this);
    return items_.clear();
}

template <typename T> bool SafeQueue<T>::Empty()
{
    std::lock_guard<std::mutex> lk(*this);
    return items_.empty();
}

template<typename T> size_t SafeQueue<T>::Size() {
    std::lock_guard<std::mutex> lk(*this);
    return items_.size();
}

template<typename T> void SafeQueue<T>::Exit() {
    exit_ = true;
    std::lock_guard<std::mutex> lk(*this);
    ready_.notify_all();
}

//Only the back insertion becomes the front insertion, which gets priority processing
template<typename T> bool SafeQueue<T>::PushEx(T& v) {
    std::lock_guard<std::mutex> lk(*this);
    if (exit_) {
        return false;
    }
    items_.push_front(std::move(v));
    ready_.notify_all();
    return true;
}

template<typename T> bool SafeQueue<T>::Push(T& v) {
    std::lock_guard<std::mutex> lk(*this);
    if (exit_ || (capacity_ && items_.size() >= capacity_)) {
        //LOG_d("input queue capacity(%d) <= size:%d", capacity_,items_.size());
        if (exit_) {
            return false;
        }
        if (!items_.empty()) {
            items_.pop_front();
        }
    }
    items_.push_back(std::move(v));
    ready_.notify_all();
    return true;
}

template<typename T> void SafeQueue<T>::Wait_Ready(
    std::unique_lock<std::mutex>& lk, int waitMs)
{
    if (exit_ || !items_.empty()) {
        return;
    }

    if (waitMs == wait_infinite) {
        ready_.wait(lk, [this] { return exit_ || !items_.empty(); });
    }
    else if (waitMs > 0) {
        auto tp = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(waitMs);
        while (ready_.wait_until(lk, tp) != std::cv_status::timeout
            && items_.empty() && !exit_) {
        }
    }
}

template<typename T> bool SafeQueue<T>::Pop_Wait(T* v, int waitMs) {
    std::unique_lock<std::mutex> lk(*this);
    Wait_Ready(lk, waitMs);
    if (items_.empty()) {
        return false;
    }
    *v = std::move(items_.front());
    items_.pop_front();
    return true;
}

template<typename T> T SafeQueue<T>::Pop_Wait(int waitMs) {
    std::unique_lock<std::mutex> lk(*this);
    Wait_Ready(lk, waitMs);
    if (items_.empty()) {
        return T();
    }
    T r = std::move(items_.front());
    items_.pop_front();
    return r;
}

template <typename T> bool SafeQueue<T>::Pop(T* v)
{
    std::unique_lock<std::mutex> lk(*this);

    if (items_.empty()) {
        return false;
    }
    *v = std::move(items_.front());
    items_.pop_front();
    return true;
}

#endif
