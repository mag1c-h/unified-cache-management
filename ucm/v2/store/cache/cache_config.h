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

#include "logger/logger.h"
#include "store_v2.h"

namespace UC::Store::Cache {

struct Config {
    StoreV2* backend{nullptr};
    int32_t deviceId{-1};
    int32_t physicalDeviceId{-1};
    size_t shardSize{};
    size_t capacity{}; /* bytes */
    size_t alignSize{4096};
    size_t timeoutMs{};
    bool enableDirectIo{true};
    bool enableDpc{false};
    bool enableLoadThrough{false};
    size_t loadExclusiveSlotNumber{1024};

    static Config From(const Dictionary& dict)
    {
        constexpr size_t kBytesPerGiB = 1024ULL * 1024ULL * 1024ULL;
        Config config;
        dict.Get("store_backend", config.backend);
        dict.GetNumber("device_id", config.deviceId);
        dict.GetNumber("physical_device_id", config.physicalDeviceId);
        dict.GetNumber("shard_size", config.shardSize);
        int64_t capacityGb = 0;
        dict.GetNumber("cache_capacity_gb", capacityGb);
        config.capacity = capacityGb > 0 ? static_cast<size_t>(capacityGb) * kBytesPerGiB : 0;
        dict.GetNumber("cache_align_size", config.alignSize);
        dict.GetNumber("cache_timeout_ms", config.timeoutMs);
        dict.Get("cache_enable_direct_io", config.enableDirectIo);
        dict.Get("cache_enable_dpc", config.enableDpc);
        dict.Get("cache_enable_load_through", config.enableLoadThrough);
        dict.GetNumber("cache_load_exclusive_slot_number", config.loadExclusiveSlotNumber);
        return config;
    }
    void Show() const
    {
        const char* ns = "UC::Store::Cache::Config";
        UC_INFO_UNLIMITED("{}.backend = {} .", ns, backend ? backend->Readme() : "nullptr");
        UC_INFO_UNLIMITED("{}.deviceId = {} .", ns, deviceId);
        UC_INFO_UNLIMITED("{}.physicalDeviceId = {} .", ns, physicalDeviceId);
        UC_INFO_UNLIMITED("{}.shardSize = {} .", ns, shardSize);
        UC_INFO_UNLIMITED("{}.capacity = {} bytes .", ns, capacity);
        UC_INFO_UNLIMITED("{}.alignSize = {} .", ns, alignSize);
        UC_INFO_UNLIMITED("{}.timeoutMs = {} .", ns, timeoutMs);
        UC_INFO_UNLIMITED("{}.enableDirectIo = {} .", ns, enableDirectIo);
        UC_INFO_UNLIMITED("{}.enableDpc = {} .", ns, enableDpc);
        UC_INFO_UNLIMITED("{}.enableLoadThrough = {} .", ns, enableLoadThrough);
        UC_INFO_UNLIMITED("{}.loadExclusiveSlotNumber = {} .", ns, loadExclusiveSlotNumber);
    }
};

}  // namespace UC::Store::Cache
