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
#ifndef UNIFIEDCACHE_ASYNC_STORE_CC_BLOCK_OPENER_H
#define UNIFIEDCACHE_ASYNC_STORE_CC_BLOCK_OPENER_H

#include <atomic>
#include <condition_variable>
#include <fcntl.h>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include "space_layout.h"
#include "type/types.h"

namespace UC::AsyncStore {

class BlockOpener {
public:
    struct Result {
        int32_t fd;
        int32_t error;
    };
    using Callback = std::function<void(Result)>;
    struct Task {
        Detail::BlockId id;
        bool activated;
        int32_t flags;
        mode_t mode;
        Callback callback;
    };

    ~BlockOpener()
    {
        {
            std::lock_guard<std::mutex> lock{mutex_};
            stop_ = false;
            cv_.notify_all();
        }
        for (auto& worker : workers_) {
            if (worker.joinable()) { worker.join(); }
        }
    }
    void Setup(const SpaceLayout* layout, const size_t nWorker)
    {
        layout_ = layout;
        nWorker_ = nWorker;
        for (size_t i = 0; i < nWorker; ++i) {
            workers_.push_back(std::thread{[this] { WorkerLoop(); }});
        }
    }
    void Submit(Task&& task)
    {
        std::lock_guard<std::mutex> lock{mutex_};
        tasks_.push_back(std::move(task));
        cv_.notify_one();
    }
    void Submit(std::list<Task>&& tasks)
    {
        std::lock_guard<std::mutex> lock{mutex_};
        tasks_.splice(tasks_.end(), tasks);
        cv_.notify_all();
    }

private:
    void WorkerLoop()
    {
        for (;;) {
            Task task;
            {
                std::unique_lock<std::mutex> lock{mutex_};
                cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_) { break; }
                if (tasks_.empty()) { continue; }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            const auto path = layout_->DataFilePath(task.id, task.activated);
            auto fd = ::open(path.c_str(), task.flags, task.mode);
            auto err = (fd < 0) ? errno : 0;
            if (task.callback) { task.callback(Result{fd, err}); }
        }
    }

    std::atomic_bool stop_{false};
    const SpaceLayout* layout_;
    size_t nWorker_;
    std::list<std::thread> workers_;
    std::list<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace UC::AsyncStore

#endif
