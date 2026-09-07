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
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "type/task_registry.h"

namespace {
struct SuccessEngine : UC::TaskRegistry<> {
    void Dispatch(TaskPtr t) override { t->waiter.Set(0); }
};

struct FailEngine : UC::TaskRegistry<> {
    void Dispatch(TaskPtr t) override
    {
        t->waiter.Set(1);
        t->status.store(UC::Status::Error::OsApiError, std::memory_order_release);
        t->waiter.Done();
    }
};

struct HangEngine : UC::TaskRegistry<> {
    void Dispatch(TaskPtr t) override { t->waiter.Set(1); }
    void Cancel(TaskPtr t) override { t->waiter.Done(); }
};

struct MultiShardEngine : UC::TaskRegistry<> {
    explicit MultiShardEngine(size_t n) : n_(n) {}
    void Dispatch(TaskPtr t) override
    {
        t->waiter.Set(n_);
        for (size_t i = 0; i < n_; ++i) { t->waiter.Done(); }
    }

private:
    size_t n_;
};

struct PartialFailEngine : UC::TaskRegistry<> {
    explicit PartialFailEngine(size_t n) : n_(n) {}
    void Dispatch(TaskPtr t) override
    {
        t->waiter.Set(n_);
        t->status.store(UC::Status::Error::NoSpace, std::memory_order_release);
        for (size_t i = 0; i < n_; ++i) { t->waiter.Done(); }
    }

private:
    size_t n_;
};

struct AsyncEngine : UC::TaskRegistry<> {
    explicit AsyncEngine(size_t delayMs) : delayMs_(delayMs) {}
    void Dispatch(TaskPtr t) override
    {
        t->waiter.Set(1);
        std::thread([t, delay = delayMs_]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            t->waiter.Done();
        }).detach();
    }

private:
    size_t delayMs_;
};
}  // namespace

class UcmV2TaskRegistryTest : public testing::Test {};

TEST_F(UcmV2TaskRegistryTest, SubmitReturnsUniqueHandle)
{
    SuccessEngine eng;
    eng.Init(1000);
    auto r1 = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
    auto r2 = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
    auto r3 = eng.Submit(UC::Task::Type::Dump, UC::TaskDesc{});
    ASSERT_TRUE(r1.HasValue());
    ASSERT_TRUE(r2.HasValue());
    ASSERT_TRUE(r3.HasValue());
    EXPECT_NE(*r1, *r2);
    EXPECT_NE(*r2, *r3);
    EXPECT_NE(*r1, *r3);
    EXPECT_GT(*r2, *r1);
    EXPECT_GT(*r3, *r2);
}

TEST_F(UcmV2TaskRegistryTest, WaitUnknownHandleReturnsNotFound)
{
    SuccessEngine eng;
    eng.Init(1000);
    EXPECT_EQ(eng.Wait(0), UC::Status::NotFound());
}

TEST_F(UcmV2TaskRegistryTest, SuccessPathReturnsOk)
{
    SuccessEngine eng;
    eng.Init(1000);
    auto r = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(eng.Wait(*r), UC::Status::Ok());
}

TEST_F(UcmV2TaskRegistryTest, FailurePathReturnsWorkerError)
{
    FailEngine eng;
    eng.Init(1000);
    auto r = eng.Submit(UC::Task::Type::Dump, UC::TaskDesc{});
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(eng.Wait(*r), UC::Status::OsApiError());
}

TEST_F(UcmV2TaskRegistryTest, TimeoutPathCancelsAndDrains)
{
    HangEngine eng;
    eng.Init(50);
    auto r = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
    ASSERT_TRUE(r.HasValue());
    auto start = std::chrono::steady_clock::now();
    auto s = eng.Wait(*r);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
    EXPECT_EQ(s, UC::Status::Timeout());
    EXPECT_LT(elapsedMs, 1000);
}

TEST_F(UcmV2TaskRegistryTest, ZeroTimeoutBlocksUntilWorkerDone)
{
    AsyncEngine eng(50);
    eng.Init(0);
    auto r = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
    ASSERT_TRUE(r.HasValue());
    auto start = std::chrono::steady_clock::now();
    auto s = eng.Wait(*r);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
    EXPECT_EQ(s, UC::Status::Ok());
    EXPECT_GE(elapsedMs, 30);
    EXPECT_LT(elapsedMs, 5000);
}

TEST_F(UcmV2TaskRegistryTest, MultiShardSuccessCountsDown)
{
    MultiShardEngine eng(4);
    eng.Init(5000);
    auto r = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(eng.Wait(*r), UC::Status::Ok());
}

TEST_F(UcmV2TaskRegistryTest, MultiShardPartialFailureVisible)
{
    PartialFailEngine eng(4);
    eng.Init(5000);
    auto r = eng.Submit(UC::Task::Type::Dump, UC::TaskDesc{});
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(eng.Wait(*r), UC::Status::NoSpace());
}

TEST_F(UcmV2TaskRegistryTest, DoubleWaitReturnsNotFound)
{
    SuccessEngine eng;
    eng.Init(1000);
    auto r = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(eng.Wait(*r), UC::Status::Ok());
    EXPECT_EQ(eng.Wait(*r), UC::Status::NotFound());
}

TEST_F(UcmV2TaskRegistryTest, ConcurrentSubmitWaitAcrossSlices)
{
    SuccessEngine eng;
    eng.Init(2000);
    constexpr int nThreads = 16;
    constexpr int perThread = 64;
    std::vector<std::thread> threads;
    threads.reserve(nThreads);
    std::atomic<int> okCount{0};
    std::atomic<int> errCount{0};
    for (int i = 0; i < nThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < perThread; ++j) {
                auto r = eng.Submit(UC::Task::Type::Load, UC::TaskDesc{});
                if (!r.HasValue()) {
                    ++errCount;
                    continue;
                }
                if (!eng.Wait(*r).Success()) {
                    ++errCount;
                    continue;
                }
                ++okCount;
            }
        });
    }
    for (auto& t : threads) { t.join(); }
    EXPECT_EQ(errCount.load(), 0);
    EXPECT_EQ(okCount.load(), nThreads * perThread);
}
