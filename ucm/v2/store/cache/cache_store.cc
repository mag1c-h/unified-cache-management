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
#include <memory>
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
    virtual void Prefetch(const BlockId* /*blocks*/, size_t /*num*/) {}
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
    void Prefetch(const BlockId* blocks, size_t num) override
    {
        if (backend_) { backend_->Prefetch(blocks, num); }
    }
    void Touch(const BlockId* blocks, size_t num) override
    {
        if (buffer_) { buffer_->Touch(blocks, num); }
        if (backend_) { backend_->Touch(blocks, num); }
    }
};

class WorkerRole : public StoreRole, private TaskRegistry<> {
    using Base = TaskRegistry<>;

public:
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
        return buffer_->Setup(cfg);
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
    void Prefetch(const BlockId* blocks, size_t num) { role_->Prefetch(blocks, num); }
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
