/**
 * MIT License
 *
 * Copyright (c) 2025 Huawei Technologies Co., Ltd. All rights reserved.
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
#ifndef UNIFIEDCACHE_POSIX_STORE_CC_TRANS_WORKER_POOL_H
#define UNIFIEDCACHE_POSIX_STORE_CC_TRANS_WORKER_POOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include "status/status.h"

namespace UC::PosixStore {

template <class Task>
class TransWorkerPool {
    using WorkerFn = std::function<void(Task&)>;
    static constexpr size_t RING_BUFFER_SIZE = 16384;
    static constexpr size_t RING_BUFFER_MASK = RING_BUFFER_SIZE - 1;
    static constexpr size_t FLUSH_BATCH_SIZE = 64;
    static constexpr auto FLUSH_TIMEOUT = std::chrono::microseconds(100);
    static constexpr size_t WORK_BATCH_SIZE = 8;
    struct alignas(64) Slot {
        Task data;
        std::atomic<bool> ready{false};
    };

public:
    TransWorkerPool() = default;
    TransWorkerPool(const TransWorkerPool&) = delete;
    TransWorkerPool(TransWorkerPool&&) = delete;
    TransWorkerPool& operator=(const TransWorkerPool&) = delete;
    TransWorkerPool& operator=(TransWorkerPool&&) = delete;
    ~TransWorkerPool()
    {
        shutdown_.store(true, std::memory_order_release);
        flushCv_.notify_all();
        cv_.notify_all();
        if (flusher_.joinable()) { flusher_.join(); }
        std::for_each(workers_.begin(), workers_.end(), [](auto& w) {
            if (w.joinable()) { w.join(); }
        });
    }
    TransWorkerPool& SetWorkerFn(WorkerFn&& fn)
    {
        fn_ = std::move(fn);
        return *this;
    }
    TransWorkerPool& SetNWorker(const size_t nWorker)
    {
        nWorker_ = nWorker;
        return *this;
    }
    Status Run()
    {
        if (!fn_ || nWorker_ == 0) { return Status::InvalidParam("invalid pool params"); }
        try {
            workers_.reserve(nWorker_);
            for (size_t i = 0; i < nWorker_; ++i) {
                workers_.emplace_back([this] { WorkLoop(); });
            }
            flusher_ = std::thread{[this] { FlushLoop(); }};
        } catch (const std::exception& e) {
            return Status::Error(fmt::format("{} at pool running", e.what()));
        }
        return Status::OK();
    }
    void Push(std::list<Task>& tasks)
    {
        auto notify = false;
        {
            std::lock_guard<std::mutex> lock{flushMtx_};
            buffer_.splice(buffer_.end(), tasks);
            notify = (buffer_.size() >= FLUSH_BATCH_SIZE);
        }
        if (notify) { flushCv_.notify_one(); }
    }

private:
    void FlushLoop()
    {
        std::list<Task> localBuffer;
        while (!shutdown_.load(std::memory_order_acquire)) {
            localBuffer.clear();
            {
                std::unique_lock<std::mutex> lock{flushMtx_};
                flushCv_.wait_for(lock, FLUSH_TIMEOUT, [this] {
                    return shutdown_.load(std::memory_order_acquire) || !buffer_.empty();
                });
                if (buffer_.empty()) { continue; }
                if (shutdown_.load(std::memory_order_acquire)) { return; }
                localBuffer.swap(buffer_);
            }
            Flush(localBuffer);
        }
    }
    void Flush(std::list<Task>& toFlush)
    {
        const auto n = toFlush.size();
        const auto start = enqueue_.load(std::memory_order_relaxed);
        size_t dequeue = dequeue_.load(std::memory_order_acquire);
        size_t available = (dequeue + RING_BUFFER_SIZE - start - 1) & RING_BUFFER_MASK;
        while (available < n) {
            dequeue = dequeue_.load(std::memory_order_acquire);
            available = (dequeue + RING_BUFFER_SIZE - start - 1) & RING_BUFFER_MASK;
            std::this_thread::yield();
        }
        auto it = toFlush.begin();
        for (size_t i = 0; i < n; ++i, ++it) {
            size_t pos = (start + i) & RING_BUFFER_MASK;
            ring_[pos].data = std::move(*it);
            ring_[pos].ready.store(true, std::memory_order_release);
        }
        size_t newEnqueue = (start + n) & RING_BUFFER_MASK;
        enqueue_.store(newEnqueue, std::memory_order_release);
        cv_.notify_all();
    }
    void WorkLoop()
    {
        std::vector<Task> batch;
        batch.reserve(WORK_BATCH_SIZE);
        while (!shutdown_.load(std::memory_order_acquire)) {
            if (TryPopBatch(batch, WORK_BATCH_SIZE)) {
                std::for_each(batch.begin(), batch.end(), [this](auto& task) { fn_(task); });
                batch.clear();
                continue;
            }
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] {
                return shutdown_.load(std::memory_order_acquire) ||
                       (enqueue_.load(std::memory_order_acquire) !=
                        dequeue_.load(std::memory_order_relaxed));
            });
        }
    }
    bool TryPopBatch(std::vector<Task>& out, const size_t maxCount)
    {
        while (true) {
            size_t dePos = dequeue_.load(std::memory_order_relaxed);
            size_t enPos = enqueue_.load(std::memory_order_acquire);
            if (dePos == enPos) { return false; }
            size_t available = (enPos + RING_BUFFER_SIZE - dePos) & RING_BUFFER_MASK;
            size_t toPop = std::min(available, maxCount);
            size_t newDePos = (dePos + toPop) & RING_BUFFER_MASK;
            if (dequeue_.compare_exchange_weak(dePos, newDePos, std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
                for (size_t i = 0; i < toPop; ++i) {
                    size_t idx = (dePos + i) & RING_BUFFER_MASK;
                    Slot& slot = ring_[idx];
                    while (!slot.ready.load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }
                    out.push_back(std::move(slot.data));
                    slot.ready.store(false, std::memory_order_release);
                }
                return true;
            }
        }
    }

private:
    WorkerFn fn_{nullptr};
    size_t nWorker_{0};
    std::atomic<bool> shutdown_{false};
    std::list<Task> buffer_;
    std::mutex flushMtx_;
    std::condition_variable flushCv_;
    std::thread flusher_;
    std::array<Slot, RING_BUFFER_SIZE> ring_;
    alignas(64) std::atomic<size_t> enqueue_{0};
    alignas(64) std::atomic<size_t> dequeue_{0};
    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable cv_;
};

}  // namespace UC::PosixStore

#endif
