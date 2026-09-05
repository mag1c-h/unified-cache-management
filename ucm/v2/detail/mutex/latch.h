/**
 * MIT License
 *
 * Copyright (c) 2026 Huawei Technologies Co., Ltd. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * */
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include "time/stopwatch.h"

namespace UC {

class Latch {
    double startTp_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<size_t> counter_{0};
    std::function<void(void)> finish_{nullptr};

public:
    Latch() : startTp_{Stopwatch::NowTp()} {}
    void Set(size_t expected) { this->counter_.store(expected); }
    void SetEpilog(std::function<void(void)> finish) { finish_ = std::move(finish); }
    void Up() { ++this->counter_; }
    void Done()
    {
        auto counter = this->counter_.load(std::memory_order_acquire);
        while (counter > 0) {
            auto desired = counter - 1;
            if (this->counter_.compare_exchange_weak(counter, desired, std::memory_order_acq_rel)) {
                if (desired == 0) {
                    if (finish_) { finish_(); }
                    std::lock_guard<std::mutex> lg(this->mutex_);
                    this->cv_.notify_all();
                }
                return;
            }
        }
    }
    double Elapsed() const { return Stopwatch::NowTp() - startTp_; }
    bool TimedOut(size_t timeoutMs)
    {
        using namespace std::chrono;
        auto elapsed = duration_cast<milliseconds>(duration<double>(Elapsed()));
        return elapsed >= milliseconds(timeoutMs);
    }
    bool Finished() { return this->counter_ == 0; }
    void Wait()
    {
        std::unique_lock<std::mutex> lk(this->mutex_);
        if (this->counter_ == 0) { return; }
        this->cv_.wait(lk, [this] { return this->counter_ == 0; });
    }
    bool WaitUntil(size_t timeoutMs)
    {
        std::unique_lock<std::mutex> lk(this->mutex_);
        if (this->counter_ == 0) { return true; }
        auto elapsed = std::chrono::duration<double>(Stopwatch::NowTp() - startTp_);
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        auto timeMs = std::chrono::milliseconds(timeoutMs);
        if (timeMs <= elapsedMs) { return false; }
        auto remainMs = timeMs - elapsedMs;
        return this->cv_.wait_for(lk, remainMs, [this] { return this->counter_ == 0; });
    }
    bool WaitFor(size_t timeoutMs)
    {
        std::unique_lock<std::mutex> lk(this->mutex_);
        if (this->counter_ == 0) { return true; }
        return this->cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                  [this] { return this->counter_ == 0; });
    }
};

}  // namespace UC
