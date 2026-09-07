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
#include "mutex/latch.h"
#include "status/status.h"
#include "type/types.h"

namespace UC {

struct Task {
    enum class Type : uint8_t { Load, Dump };
    TaskHandle handle;
    TaskDesc desc;
    Type type;
    std::atomic<Status::Error> status;
    Latch waiter;

    Task(Type type, TaskDesc desc)
        : handle{Next()}, desc{std::move(desc)}, type{type}, status{Status::Error::Ok}
    {
    }

private:
    static TaskHandle Next()
    {
        static std::atomic<TaskHandle> idSeed{1};
        return idSeed.fetch_add(1, std::memory_order_relaxed);
    }
};

}  // namespace UC
