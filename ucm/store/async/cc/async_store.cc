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
#include "space_manager.h"
#include "ucmstore_v1.h"

namespace UC::AsyncStore {

class AsyncStore : public StoreV1 {
    bool llmWorker_{false};
    SpaceManager spaceMgr_;

public:
    Status Setup(const Detail::Dictionary& param) override
    {
        Config config;
        auto status = config.Parse(param);
        if (status.Failure()) {
            UC_ERROR("Failed to parse config: {}.", status);
            return status;
        }
        llmWorker_ = config.deviceId >= 0;
        status = spaceMgr_.Setup(config);
        if (status.Failure()) { return status; }
        config.Show();
        return Status::OK();
    }
    std::string Readme() const override { return "AsyncStore"; }
    Expected<std::vector<uint8_t>> Lookup(const Detail::BlockId* blocks, size_t num) override
    {
        std::vector<uint8_t> results(num, false);
        auto res = LookupOnPrefix(blocks, num);
        if (!res) [[unlikely]] {
            UC_ERROR("Failed({}) to lookup blocks({}).", res.Error(), num);
            return res.Error();
        }
        const auto index = res.Value();
        for (ssize_t i = 0; i <= index; ++i) { results[i] = true; }
        return results;
    }
    Expected<ssize_t> LookupOnPrefix(const Detail::BlockId* blocks, size_t num) override
    {
        if (llmWorker_) [[unlikely]] { return Status::Unsupported(); }
        auto res = spaceMgr_.LookupOnPrefix(blocks, num);
        if (!res) [[unlikely]] { UC_ERROR("Failed({}) to lookup blocks({}).", res.Error(), num); }
        return res;
    }
    void Prefetch(const Detail::BlockId* blocks, size_t num) override {}
    Expected<Detail::TaskHandle> Load(Detail::TaskDesc task) override
    {
        return Status::Unsupported();
    }
    Expected<Detail::TaskHandle> Dump(Detail::TaskDesc task) override
    {
        return Status::Unsupported();
    }
    Expected<bool> Check(Detail::TaskHandle taskId) override { return Status::Unsupported(); }
    Status Wait(Detail::TaskHandle taskId) override { return Status::Unsupported(); }
};

}  // namespace UC::AsyncStore

extern "C" UC::StoreV1* MakeAsyncStore() { return new UC::AsyncStore::AsyncStore(); }
