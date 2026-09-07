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
#include <chrono>
#include <climits>
#include <cstddef>
#include <functional>
#include <memory>
#include <thread>

namespace UC {

template <typename T>
class SpscQueue {
    alignas(64) std::atomic<size_t> head_ = 0;
    alignas(64) std::atomic<size_t> tail_ = 0;
    struct alignas(64) {
        bool pow2{false};
        size_t mask{0};
        size_t capacity{0};
        std::unique_ptr<T[]> buffer;
    } cfg_;

    size_t Mod(size_t n) const noexcept
    {
        return cfg_.pow2 ? (n & cfg_.mask) : (n % cfg_.capacity);
    }

public:
    void Setup(size_t capacity)
    {
        cfg_.capacity = capacity;
        cfg_.mask = capacity - 1;
        cfg_.pow2 = (capacity & cfg_.mask) == 0;
        cfg_.buffer = std::make_unique<T[]>(capacity);
    }

    void Push(T&& value)
    {
        const size_t currentHead = head_.load(std::memory_order_relaxed);
        const size_t nextHead = Mod(currentHead + 1);
        constexpr size_t kSpinLimit = 16;
        size_t spinCount = 0;
        for (;;) {
            if (nextHead != tail_.load(std::memory_order_acquire)) {
                cfg_.buffer[currentHead] = std::move(value);
                head_.store(nextHead, std::memory_order_release);
                return;
            }
            // Adaptive backoff: yield first, then sleep to avoid
            // hammering the consumer's tail_ cache line under a slow consumer.
            if (++spinCount < kSpinLimit) {
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                spinCount = 0;
            }
        }
    }

    bool TryPush(T&& value)
    {
        const size_t currentHead = head_.load(std::memory_order_relaxed);
        const size_t nextHead = Mod(currentHead + 1);
        const size_t currentTail = tail_.load(std::memory_order_acquire);
        if (nextHead == currentTail) { return false; }
        cfg_.buffer[currentHead] = std::move(value);
        head_.store(nextHead, std::memory_order_release);
        return true;
    }

    bool TryPop(T& value)
    {
        const size_t currentHead = head_.load(std::memory_order_acquire);
        const size_t currentTail = tail_.load(std::memory_order_relaxed);
        if (currentTail == currentHead) { return false; }
        value = std::move(cfg_.buffer[currentTail]);
        tail_.store(Mod(currentTail + 1), std::memory_order_release);
        return true;
    }

    template <typename ConsumerHandler, typename... Args>
    void ConsumerLoop(const std::atomic_bool& stop, ConsumerHandler&& handler, Args&&... args)
    {
        constexpr size_t kSpinLimit = 16;
        constexpr size_t kTaskBatch = 64;
        size_t spinCount = 0;
        size_t taskCount = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            T task;
            if (TryPop(task)) {
                spinCount = 0;
                std::invoke(handler, std::forward<Args>(args)..., std::move(task));
                if (++taskCount % kTaskBatch == 0) {
                    if (stop.load(std::memory_order_acquire)) { break; }
                }
                continue;
            }
            if (++spinCount < kSpinLimit) {
                std::this_thread::yield();
            } else {
                if (stop.load(std::memory_order_acquire)) { break; }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                spinCount = 0;
            }
        }
    }
};

}  // namespace UC
