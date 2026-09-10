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
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include "cache_buffer.h"
#include "cache_config.h"
#include "type/task_registry.h"

namespace UC::Store::Cache {

namespace {

class StoreRole {
public:
    virtual ~StoreRole() = default;
    virtual Status Validate(const Config& cfg) const = 0;
    virtual Status Setup(const Config& cfg) = 0;
    virtual Expected<ssize_t> LookupOnPrefix(const BlockId* /*blocks*/, size_t /*num*/)
    {
        return Status::Unsupported();
    }
    virtual Expected<ssize_t> LookupOnReverse(const BlockId* /*blocks*/, size_t /*num*/)
    {
        return Status::Unsupported();
    }
    virtual void Prefetch(const BlockId* /*blocks*/, size_t /*num*/, int32_t /*targetDeviceId*/) {}
    virtual void Touch(const BlockId* /*blocks*/, size_t /*num*/) {}
    virtual Expected<TaskHandle> Load(TaskDesc task) { return Status::Unsupported(); }
    virtual Expected<TaskHandle> Dump(TaskDesc task) { return Status::Unsupported(); }
    virtual Status Wait(TaskHandle taskId) { return Status::Unsupported(); }

protected:
    StoreV2* backend_{nullptr};
    std::unique_ptr<Buffer> buffer_;
};

class SchedulerRole : public StoreRole {
public:
    Status Validate(const Config& cfg) const override
    {
        if (cfg.deviceId >= 0) {
            return Status::Make(Status::Error::InvalidParam, "SchedulerRole: deviceId must be < 0");
        }
        return Status::Ok();
    }
    Status Setup(const Config& cfg) override
    {
        backend_ = cfg.backend;
        buffer_ = std::make_unique<Buffer>();
        return buffer_->Setup(cfg);
    }
    Expected<ssize_t> LookupOnPrefix(const BlockId* blocks, size_t num) override
    {
        ssize_t last = -1;
        for (size_t i = 0; i < num; ++i) {
            if (!buffer_ || !buffer_->Exist(blocks[i], 0)) { return last; }
            last = static_cast<ssize_t>(i);
        }
        return last;
    }
    Expected<ssize_t> LookupOnReverse(const BlockId* blocks, size_t num) override
    {
        if (!buffer_) { return -1; }
        for (ssize_t i = static_cast<ssize_t>(num) - 1; i >= 0; --i) {
            if (buffer_->Exist(blocks[i], 0)) { return i; }
        }
        return -1;
    }
    void Prefetch(const BlockId* blocks, size_t num, int32_t targetDeviceId) override
    {
        if (targetDeviceId >= 0) {
            /* Directed affinity: enqueue only if the target worker is online; an
             * unreachable target is dropped (fire-and-forget contract). */
            auto rank = static_cast<size_t>(targetDeviceId);
            if (rank < kMaxRanks && buffer_ && buffer_->RankReady(rank)) {
                buffer_->EnqueuePrefetch(rank, blocks, num);
            }
            return;
        }
        if (!buffer_) {
            if (backend_) { backend_->Prefetch(blocks, num, -1); }
            return;
        }
        /* No affinity: deterministic round-robin over online workers, restarting from
         * the first block on every call so a given list position always maps to the
         * same rank (stable affinity across repeated prefetches of the same list). */
        size_t online[kMaxRanks];
        size_t n = 0;
        for (size_t r = 0; r < kMaxRanks; r++) {
            if (buffer_->RankReady(r)) { online[n++] = r; }
        }
        if (n == 0) {
            if (backend_) { backend_->Prefetch(blocks, num, -1); }
            return;
        }
        for (size_t i = 0; i < num; i++) { buffer_->EnqueuePrefetch(online[i % n], &blocks[i], 1); }
    }
    void Touch(const BlockId* blocks, size_t num) override
    {
        if (buffer_) { buffer_->Touch(blocks, num); }
        if (backend_) { backend_->Touch(blocks, num); }
    }
};

class WorkerRole : public StoreRole, private TaskRegistry<> {
    using Base = TaskRegistry<>;
    static constexpr size_t kPrefetchBatch = 64;

public:
    ~WorkerRole() override
    {
        stop_.store(true, std::memory_order_relaxed);
        if (prefetchThread_.joinable()) { prefetchThread_.join(); }
    }

    Status Validate(const Config& cfg) const override
    {
        if (cfg.deviceId < 0) {
            return Status::Make(Status::Error::InvalidParam, "WorkerRole: deviceId must be >= 0");
        }
        if (cfg.physicalDeviceId < 0 || cfg.physicalDeviceId >= static_cast<int32_t>(kMaxRanks)) {
            return Status::Make(Status::Error::InvalidParam,
                                "WorkerRole: physicalDeviceId must be in [0, {})", kMaxRanks);
        }
        if (cfg.alignSize == 0 || (cfg.alignSize & (cfg.alignSize - 1)) != 0) {
            return Status::Make(Status::Error::InvalidParam,
                                "WorkerRole: alignSize must be non-zero power of two");
        }
        auto slotSize = AlignUp(cfg.shardSize, cfg.alignSize);
        if (slotSize == 0 || cfg.capacity < slotSize) {
            return Status::Make(Status::Error::InvalidParam,
                                "WorkerRole: capacity must be no less than aligned shardSize");
        }
        if (cfg.loadExclusiveSlotNumber >= cfg.capacity / slotSize) {
            return Status::Make(
                Status::Error::InvalidParam,
                "WorkerRole: loadExclusiveSlotNumber must be less than slots per rank");
        }
        return Status::Ok();
    }
    Status Setup(const Config& cfg) override
    {
        backend_ = cfg.backend;
        Base::Init(cfg.timeoutMs);
        buffer_ = std::make_unique<Buffer>();
        auto s = buffer_->Setup(cfg);
        if (s.Failure()) { return s; }
        prefetchThread_ = std::thread([this] { PrefetchLoop(); });
        return Status::Ok();
    }
    Expected<TaskHandle> Load(TaskDesc task) override
    {
        return Base::Submit(Task::Type::Load, std::move(task));
    }
    Expected<TaskHandle> Dump(TaskDesc task) override
    {
        return Base::Submit(Task::Type::Dump, std::move(task));
    }
    Status Wait(TaskHandle taskId) override { return Base::Wait(taskId); }

protected:
    void Dispatch(TaskPtr) override
    {
        // todo
    }

private:
    /* Prefetch executor: drains this rank's command ring and loads the first shard of
     * each requested block from the backend into the cache. Sequential on purpose —
     * prefetch is a background hint; integration point for the future Dispatch engine. */
    void PrefetchLoop()
    {
        constexpr auto idle = std::chrono::milliseconds(1);
        auto rank = buffer_->MyRank();
        if (rank == kInvalidIndex) { return; }
        BlockId batch[kPrefetchBatch];
        while (!stop_.load(std::memory_order_relaxed)) {
            auto n = buffer_->DrainPrefetch(rank, batch, kPrefetchBatch);
            if (n == 0) {
                std::this_thread::sleep_for(idle);
                continue;
            }
            do {
                PrefetchBatch(batch, n);
                n = buffer_->DrainPrefetch(rank, batch, kPrefetchBatch);
            } while (n > 0);
        }
    }

    /* One backend Load per drained batch. Failure is batch-granular (Wait reports a
     * single status): on failure the whole batch is marked Failed and healed by the
     * next Get/prefetch that takes ownership. */
    void PrefetchBatch(const BlockId* blocks, size_t num)
    {
        if (!backend_) { return; }
        TaskDesc task;
        std::vector<Buffer::Handle> handles;
        handles.reserve(num);
        for (size_t i = 0; i < num; i++) {
            /* First shard only; allowReserved=false keeps the load-exclusive region
             * for real loads. A non-owner handle means the block is cached or being
             * loaded by any rank — ensure-cached semantics, nothing to do. */
            auto h = buffer_->Get(blocks[i], 0, false);
            if (!h || !h.Owner()) { continue; }
            if (h.Data() == nullptr) {
                h.MarkFailed();
                continue;
            }
            Shard shard;
            shard.owner = blocks[i];
            shard.offset = 0;
            shard.addrs.push_back(Io{h.Data(), buffer_->SlotSize()});
            task.shards.push_back(std::move(shard));
            handles.emplace_back(std::move(h));
        }
        if (task.shards.empty()) { return; }
        auto r = backend_->Load(std::move(task));
        auto ok = r.HasValue() && backend_->Wait(r.Value()).Success();
        for (auto& h : handles) {
            if (ok) {
                h.MarkReady();
            } else {
                h.MarkFailed();
            }
        }
    }

    std::atomic<bool> stop_{false};
    std::thread prefetchThread_;
};

std::unique_ptr<StoreRole> MakeStoreRole(const Config& cfg)
{
    if (cfg.deviceId < 0) { return std::make_unique<SchedulerRole>(); }
    return std::make_unique<WorkerRole>();
}

}  // namespace

class CacheStore : public StoreV2 {
public:
    Status Setup(const Dictionary& dict) override
    {
        try {
            auto config = Config::From(dict);
            config.Show();
            role_ = MakeStoreRole(config);
            if (auto s = role_->Validate(config); s.Failure()) { return s; }
            return role_->Setup(config);
        } catch (const std::exception& e) {
            return Status::Make(Status::Error::General, "Failed({}) to setup CacheStore", e.what());
        }
        return Status::Ok();
    }
    std::string Readme() const override { return "CacheStore"; }
    Expected<ssize_t> LookupOnPrefix(const BlockId* blocks, size_t num) override
    {
        return role_->LookupOnPrefix(blocks, num);
    }
    Expected<ssize_t> LookupOnReverse(const BlockId* blocks, size_t num) override
    {
        return role_->LookupOnReverse(blocks, num);
    }
    void Prefetch(const BlockId* blocks, size_t num, int32_t targetDeviceId) override
    {
        role_->Prefetch(blocks, num, targetDeviceId);
    }
    void Touch(const BlockId* blocks, size_t num) { role_->Touch(blocks, num); }
    Expected<TaskHandle> Load(TaskDesc task) override { return role_->Load(std::move(task)); }
    Expected<TaskHandle> Dump(TaskDesc task) override { return role_->Dump(std::move(task)); }
    Status Wait(TaskHandle taskId) override { return role_->Wait(taskId); }

private:
    std::unique_ptr<StoreRole> role_;
};

}  // namespace UC::Store::Cache

extern "C" __attribute__((visibility("default"))) UC::Store::StoreV2* UcmMakeCacheStore()
{
    return new UC::Store::Cache::CacheStore();
}
