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
#include "cache_config.h"

namespace UC::Store::Cache {

class CacheStore : public StoreV2 {
public:
    Status Setup(const Dictionary& dict) override
    {
        auto config = Config::From(dict);
        config.Show();
        return Status::Ok();
    }
    std::string Readme() const override { return "CacheStore"; }
    Expected<ssize_t> LookupOnPrefix(const BlockId* blocks, size_t num) override
    {
        return Status::Unsupported();
    }
    Expected<ssize_t> LookupOnReverse(const BlockId* blocks, size_t num) override
    {
        return Status::Unsupported();
    }
    void Prefetch(const BlockId* blocks, size_t num) {}
    void Touch(const BlockId* blocks, size_t num) {}
    Expected<TaskHandle> Load(TaskDesc task) override { return Status::Unsupported(); }
    Expected<TaskHandle> Dump(TaskDesc task) override { return Status::Unsupported(); }
    Status Wait(TaskHandle taskId) override { return Status::Unsupported(); }
};

}  // namespace UC::Store::Cache

extern "C" __attribute__((visibility("default"))) UC::Store::StoreV2* UcmMakeCacheStore()
{
    return new UC::Store::Cache::CacheStore();
}
