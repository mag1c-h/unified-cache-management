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
#ifndef UNIFIEDCACHE_ASYNC_STORE_CC_GLOBAL_CONFIG_H
#define UNIFIEDCACHE_ASYNC_STORE_CC_GLOBAL_CONFIG_H

#include <string>
#include <vector>
#include "logger/logger.h"
#include "status/status.h"
#include "type/dictionary.h"

namespace UC::AsyncStore {

class Config {
public:
    std::vector<std::string> storageBackends{};
    int32_t deviceId{-1};
    size_t shardSize{0};
    size_t blockSize{0};
    size_t lookupConcurrency{8};
    size_t openConcurrency{32};
    size_t commitConcurrency{16};
    size_t timeoutMs{30000};
    size_t dataDirShardBytes{3};

    Status Parse(const Detail::Dictionary& param)
    {
        param.Get("storage_backends", storageBackends);
        param.GetNumber("device_id", deviceId);
        param.GetNumber("shard_size", shardSize);
        param.GetNumber("block_size", blockSize);
        param.GetNumber("async_lookup_concurrency", lookupConcurrency);
        param.GetNumber("async_open_concurrency", openConcurrency);
        param.GetNumber("async_commit_concurrency", commitConcurrency);
        param.GetNumber("timeout_ms", timeoutMs);
        param.GetNumber("data_dir_shard_bytes", dataDirShardBytes);
        if (storageBackends.empty()) { return Status::InvalidParam("invalid storage backends"); }
        if (deviceId < -1) { return Status::InvalidParam("invalid device({})", deviceId); }
        if (lookupConcurrency == 0 || openConcurrency == 0 || commitConcurrency == 0) {
            return Status::InvalidParam("invalid concurrency({},{}, {})", lookupConcurrency,
                                        openConcurrency, commitConcurrency);
        }
        if (dataDirShardBytes > 5) {
            return Status::InvalidParam("invalid shard bytes({})", dataDirShardBytes);
        }
        if (deviceId == -1) { return Status::OK(); }
        if (blockSize < shardSize || blockSize % shardSize != 0) {
            return Status::InvalidParam("invalid size({},{})", shardSize, blockSize);
        }
        return Status::OK();
    }
    void Show()
    {
        constexpr const char* ns = "AsyncStore";
        std::string buildType = UCM_BUILD_TYPE;
        if (buildType.empty()) { buildType = "Release"; }
        UC_INFO("{}-{}({}).", ns, UCM_COMMIT_ID, buildType);
        UC_INFO("Set {}::StorageBackends to {}.", ns, storageBackends);
        UC_INFO("Set {}::DeviceId to {}.", ns, deviceId);
        UC_INFO("Set {}::ShardSize to {}.", ns, shardSize);
        UC_INFO("Set {}::BlockSize to {}.", ns, blockSize);
        UC_INFO("Set {}::LookupConcurrency to {}.", ns, lookupConcurrency);
        UC_INFO("Set {}::OpenConcurrency to {}.", ns, openConcurrency);
        UC_INFO("Set {}::CommitConcurrency to {}.", ns, commitConcurrency);
        UC_INFO("Set {}::TimeoutMs to {}.", ns, timeoutMs);
        UC_INFO("Set {}::DataDirShardBytes to {}.", ns, dataDirShardBytes);
    }
};

}  // namespace UC::AsyncStore

#endif
