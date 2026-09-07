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
 */
#include <gtest/gtest.h>
#include <memory>
#include <sys/wait.h>
#include <unistd.h>
#include "cache/cache_buffer.h"
#include "cache/cache_config.h"
#include "store_v2.h"

extern "C" UC::Store::StoreV2* UcmMakeCacheStore();

namespace {

UC::BlockId MakeBlockId(char c)
{
    UC::BlockId b;
    b.fill(static_cast<std::byte>(c));
    return b;
}

UC::Dictionary MakeWorkerDict(int32_t deviceId, int32_t physicalDeviceId)
{
    UC::Dictionary dict;
    dict.SetNumber("device_id", deviceId);
    dict.SetNumber("physical_device_id", physicalDeviceId);
    dict.SetNumber("shard_size", static_cast<int64_t>(4096));
    dict.SetNumber("cache_capacity_gb", static_cast<int64_t>(1));
    dict.SetNumber("cache_load_exclusive_slot_number", static_cast<int64_t>(0));
    dict.SetNumber("cache_timeout_ms", static_cast<int64_t>(5000));
    return dict;
}

UC::Store::Cache::Config MakeBufConfig(int32_t rank, size_t slots)
{
    UC::Store::Cache::Config cfg;
    cfg.deviceId = rank;
    cfg.physicalDeviceId = rank;
    cfg.shardSize = 4096;
    cfg.alignSize = 4096;
    cfg.capacity = 4096 * slots;
    cfg.loadExclusiveSlotNumber = 0;
    cfg.timeoutMs = 5000;
    return cfg;
}

}  // namespace

TEST(UcmV2CacheStoreTest, SchedulerCreatorWithoutParamsRejected)
{
    UC::Dictionary dict;
    dict.SetNumber("device_id", -1);
    std::unique_ptr<UC::Store::StoreV2> store(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(dict).Failure());
}

TEST(UcmV2CacheStoreTest, SchedulerJoinerWithoutParams)
{
    UC::Store::Cache::Buffer wbuf;
    ASSERT_TRUE(wbuf.Setup(MakeBufConfig(0, 4)).Success());
    auto blk = MakeBlockId('x');
    {
        auto h = wbuf.Get(blk, 0);
        h.MarkReady();
    }

    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        UC::Dictionary dict;
        dict.SetNumber("device_id", -1);
        dict.SetNumber("cache_timeout_ms", static_cast<int64_t>(5000));
        std::unique_ptr<UC::Store::StoreV2> store(UcmMakeCacheStore());
        if (store->Setup(dict).Failure()) { _exit(2); }
        UC::BlockId blocks[1] = {blk};
        auto r = store->LookupOnPrefix(blocks, 1);
        _exit(r.HasValue() && r.Value() == 0 ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status)) << "child crashed";
    EXPECT_EQ(WEXITSTATUS(status), 0) << "zero-param scheduler joiner failed";
}

TEST(UcmV2CacheStoreTest, WorkerInvalidParamsRejected)
{
    auto d = MakeWorkerDict(0, 0);
    d.SetNumber("cache_align_size", static_cast<int64_t>(0));
    std::unique_ptr<UC::Store::StoreV2> store(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(d).Failure());

    d = MakeWorkerDict(0, 0);
    d.SetNumber("cache_align_size", static_cast<int64_t>(3));
    store.reset(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(d).Failure());

    /* 2 GiB shard cannot fit into a 1 GiB capacity. */
    d = MakeWorkerDict(0, 0);
    d.SetNumber("shard_size", static_cast<int64_t>(2) * 1024 * 1024 * 1024);
    store.reset(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(d).Failure());

    d = MakeWorkerDict(0, static_cast<int32_t>(UC::Store::Cache::kMaxRanks));
    store.reset(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(d).Failure());

    d = MakeWorkerDict(0, -1);
    store.reset(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(d).Failure());

    /* 1 GiB / 4096 = 262144 slots; reserving them all is rejected. */
    d = MakeWorkerDict(0, 0);
    d.SetNumber("cache_load_exclusive_slot_number", static_cast<int64_t>(262144));
    store.reset(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(d).Failure());
}

TEST(UcmV2CacheStoreTest, DefaultAlignSize4096)
{
    UC::Store::Cache::Config cfg;
    EXPECT_EQ(cfg.alignSize, 4096u);
    auto from = UC::Store::Cache::Config::From(UC::Dictionary{});
    EXPECT_EQ(from.alignSize, 4096u);

    /* 512 MiB shards in a 1 GiB cache give m = 2 slots; success here also proves the
     * GB -> bytes conversion in Config::From end-to-end (without it capacity would be
     * 1 byte and Setup would fail). */
    auto dict = MakeWorkerDict(0, 0);
    dict.SetNumber("shard_size", static_cast<int64_t>(512) * 1024 * 1024);
    std::unique_ptr<UC::Store::StoreV2> store(UcmMakeCacheStore());
    EXPECT_TRUE(store->Setup(dict).Success());
}

TEST(UcmV2CacheStoreTest, WorkerJoinerAdoptsHeaderParams)
{
    UC::Store::Cache::Buffer wbuf;
    ASSERT_TRUE(wbuf.Setup(MakeBufConfig(0, 8)).Success());
    auto blk0 = MakeBlockId('p');
    {
        auto h = wbuf.Get(blk0, 0);
        h.MarkReady();
    }

    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        UC::Store::Cache::Buffer cbuf;
        if (cbuf.Setup(MakeBufConfig(1, 4)).Failure()) { _exit(2); }
        if (!cbuf.Exist(blk0, 0)) { _exit(3); }
        auto h = cbuf.Get(blk0, 0);
        if (!h) { _exit(4); }
        if (h.Data() == nullptr) { _exit(5); }
        if (h.Owner()) { _exit(6); }
        auto blk1 = MakeBlockId('q');
        auto h2 = cbuf.Get(blk1, 0);
        if (!h2 || !h2.Owner()) { _exit(7); }
        h2.MarkReady();
        if (!cbuf.Exist(blk1, 0)) { _exit(8); }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status)) << "child crashed";
    EXPECT_EQ(WEXITSTATUS(status), 0) << "worker joiner failed to adopt header params";
}
