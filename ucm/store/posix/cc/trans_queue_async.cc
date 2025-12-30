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
#include "trans_queue_async.h"
#include <liburing.h>
#include "logger/logger.h"

namespace UC::PosixStore {

class TransQueueAsyncImpl {
    using TaskIdSet = HashSet<Detail::TaskHandle>;
    using TaskPtr = std::shared_ptr<TransTask>;
    using WaiterPtr = std::shared_ptr<Latch>;
    using TaskPair = std::pair<TaskPtr, WaiterPtr>;
    struct IoUringContext {
        io_uring ring;
        ~IoUringContext() { io_uring_queue_exit(&ring); }
    };

private:
    std::unique_ptr<IoUringContext> ioCtx_;
    TaskIdSet* failureSet_;
    const SpaceLayout* layout_;
    size_t ioSize_;
    size_t shardSize_;
    size_t nShardPerBlock_;
    bool ioDirect_;

public:
    Status Setup(const Config& config, TaskIdSet* failureSet, const SpaceLayout* layout)
    {
        try {
            ioCtx_ = std::make_unique<IoUringContext>();
        } catch (const std::exception& e) {
            UC_ERROR("Failed({}) to make io_uring object.", e.what());
            return Status::Error(e.what());
        }
        auto ret = io_uring_queue_init(config.streamNumber, &ioCtx_->ring, 0);
        if (ret < 0) {
            UC_ERROR("Failed({}) to init io_uring({}) object.", ret, config.streamNumber);
            return Status{ret, strerror(-ret)};
        }
        failureSet_ = failureSet;
        layout_ = layout;
        ioSize_ = config.tensorSize;
        shardSize_ = config.shardSize;
        nShardPerBlock_ = config.blockSize / config.shardSize;
        ioDirect_ = config.ioDirect;
        return Status::OK();
    }
    void Push(TaskPtr task, WaiterPtr waiter) {}
};

Status TransQueueAsync::Setup(const Config& config, TaskIdSet* failureSet,
                              const SpaceLayout* layout)
{
    try {
        impl_ = std::make_shared<TransQueueAsyncImpl>();
    } catch (const std::exception& e) {
        UC_ERROR("Failed({}) to make async trans queue object.", e.what());
        return Status::Error(e.what());
    }
    return impl_->Setup(config, failureSet, layout);
}

void TransQueueAsync::Push(TaskPtr task, WaiterPtr waiter) { impl_->Push(task, waiter); }

}  // namespace UC::PosixStore
