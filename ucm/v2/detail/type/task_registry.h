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

#include <memory>
#include <mutex>
#include <unordered_map>
#include "logger/logger.h"
#include "type/task.h"

namespace UC {

template <size_t SliceBits = 6>
class TaskRegistry {
    static_assert(SliceBits <= 10, "SliceBits too large");
    static constexpr size_t nSlice = size_t{1} << SliceBits;

protected:
    using TaskPtr = std::shared_ptr<Task>;
    virtual void Dispatch(TaskPtr) = 0;
    virtual void Cancel(TaskPtr) {}

private:
    struct alignas(64) Slice {
        std::mutex mutex;
        std::unordered_map<TaskHandle, TaskPtr> taskSet;
    };
    Slice tasks_[nSlice];
    size_t timeoutMs_{0};

    Slice& SliceOf(const TaskHandle& handle) { return tasks_[handle & (nSlice - 1)]; }

public:
    virtual ~TaskRegistry() = default;
    void Init(size_t timeoutMs) { timeoutMs_ = timeoutMs; }
    Expected<TaskHandle> Submit(Task::Type type, TaskDesc desc)
    {
        TaskPtr t;
        TaskHandle handle = 0;
        try {
            t = std::make_shared<Task>(type, std::move(desc));
            handle = t->handle;
            auto& slice = SliceOf(handle);
            std::lock_guard<std::mutex> lock(slice.mutex);
            slice.taskSet.emplace(handle, t);
        } catch (const std::exception& e) {
            return Status::Make(Status::Error::General, "{}", e.what());
        }
        Dispatch(t);
        return handle;
    }
    Status Wait(TaskHandle handle)
    {
        auto& slice = SliceOf(handle);
        TaskPtr t = nullptr;
        {
            std::lock_guard<std::mutex> lock(slice.mutex);
            auto iter = slice.taskSet.find(handle);
            if (iter == slice.taskSet.end()) { return Status::NotFound(); }
            t = iter->second;
            slice.taskSet.erase(iter);
        }
        if (timeoutMs_ == 0) {
            t->waiter.Wait();
        } else {
            if (!t->waiter.WaitUntil(timeoutMs_)) {
                auto expected = Status::Error::Ok;
                t->status.compare_exchange_strong(expected, Status::Error::Timeout,
                                                  std::memory_order_acq_rel);
                Cancel(t);
                constexpr size_t drainSliceMs = 2000;
                while (!t->waiter.WaitFor(drainSliceMs)) {
                    UC_WARN("Task({}) has not finished after ({}) ms.", handle, drainSliceMs);
                }
            }
        }
        return Status::Make(t->status.load(std::memory_order_acquire));
    }
};

}  // namespace UC
