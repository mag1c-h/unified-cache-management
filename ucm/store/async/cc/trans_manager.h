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
#ifndef UNIFIEDCACHE_ASYNC_STORE_CC_TRANS_MANAGER_H
#define UNIFIEDCACHE_ASYNC_STORE_CC_TRANS_MANAGER_H

#include "global_config.h"
#include "space_layout.h"
#include "template/task_wrapper.h"
#include "trans_task.h"

namespace UC::AsyncStore {

class TransManager : public Detail::TaskWrapper<TransTask, Detail::TaskHandle> {
    size_t shardSize_;

public:
    Status Setup(const Config& config, const SpaceLayout* layout)
    {
        timeoutMs_ = config.timeoutMs;
        shardSize_ = config.shardSize;
        return Status::OK();
    }

private:
    void Dispatch(TaskPtr t, WaiterPtr w) override
    {
        const auto num = t->desc.size();
        const auto size = shardSize_ * num;
        t->metrics.Start(w->startTp);
        w->SetEpilog([t, num, size] {
            t->metrics.Tick();
            UC_DEBUG("Async task({},{},{},{}) finished, cost {}ms.", t->id, t->desc.brief, num,
                     size, t->metrics.Report());
        });
    }
};

}  // namespace UC::AsyncStore

#endif
