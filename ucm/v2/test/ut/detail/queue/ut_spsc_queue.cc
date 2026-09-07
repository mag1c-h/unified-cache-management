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
#include <string>
#include <thread>
#include <vector>
#include "queue/spsc_queue.h"

namespace {
struct MoveOnly {
    int value;
    MoveOnly() = default;
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
};

// Tracer: copyable + movable, counts copy/move constructions to verify that
// ConsumerLoop forwards Args with their original value category.
struct Tracer {
    int* moves;
    int* copies;
    Tracer(int* m, int* c) : moves(m), copies(c) {}
    Tracer(Tracer&& o) noexcept : moves(o.moves), copies(o.copies) { ++*moves; }
    Tracer(const Tracer& o) : moves(o.moves), copies(o.copies) { ++*copies; }
};
}  // namespace

class UcmV2SpscQueueTest : public testing::Test {};

TEST_F(UcmV2SpscQueueTest, EmptyTryPopReturnsFalse)
{
    UC::SpscQueue<size_t> q;
    q.Setup(8);
    size_t v = 0;
    EXPECT_FALSE(q.TryPop(v));
}

TEST_F(UcmV2SpscQueueTest, TryPushTryPopRoundTrip)
{
    UC::SpscQueue<std::string> q;
    q.Setup(8);
    ASSERT_TRUE(q.TryPush(std::string("hello")));
    std::string out;
    ASSERT_TRUE(q.TryPop(out));
    EXPECT_EQ(out, "hello");
    EXPECT_FALSE(q.TryPop(out));
}

TEST_F(UcmV2SpscQueueTest, FIFOOrderPow2)
{
    UC::SpscQueue<size_t> q;
    q.Setup(16);
    constexpr size_t n = 10;
    for (size_t i = 0; i < n; ++i) { ASSERT_TRUE(q.TryPush(std::move(i))); }
    for (size_t i = 0; i < n; ++i) {
        size_t v = 0;
        ASSERT_TRUE(q.TryPop(v));
        EXPECT_EQ(v, i);
    }
    size_t v = 0;
    EXPECT_FALSE(q.TryPop(v));
}

TEST_F(UcmV2SpscQueueTest, FullPow2)
{
    constexpr size_t N = 8;
    UC::SpscQueue<size_t> q;
    q.Setup(N);
    for (size_t i = 0; i < N - 1; ++i) { ASSERT_TRUE(q.TryPush(std::move(i))); }
    EXPECT_FALSE(q.TryPush(999));
    size_t v = 0;
    ASSERT_TRUE(q.TryPop(v));
    EXPECT_EQ(v, 0);
    ASSERT_TRUE(q.TryPush(999));
    ASSERT_TRUE(q.TryPop(v));
    EXPECT_EQ(v, 1);
}

TEST_F(UcmV2SpscQueueTest, FullNonPow2)
{
    constexpr size_t N = 5;  // non power-of-two -> Mod takes the % branch
    UC::SpscQueue<size_t> q;
    q.Setup(N);
    for (size_t i = 0; i < N - 1; ++i) { ASSERT_TRUE(q.TryPush(std::move(i))); }
    EXPECT_FALSE(q.TryPush(999));
    size_t v = 0;
    ASSERT_TRUE(q.TryPop(v));
    EXPECT_EQ(v, 0);
    ASSERT_TRUE(q.TryPush(4));
    for (size_t i = 1; i < N; ++i) {
        ASSERT_TRUE(q.TryPop(v));
        EXPECT_EQ(v, i);
    }
    EXPECT_FALSE(q.TryPop(v));
}

TEST_F(UcmV2SpscQueueTest, MoveOnly)
{
    UC::SpscQueue<MoveOnly> q;
    q.Setup(9);
    EXPECT_TRUE(q.TryPush(MoveOnly(42)));
    MoveOnly out;
    EXPECT_TRUE(q.TryPop(out));
    EXPECT_EQ(out.value, 42);
}

TEST_F(UcmV2SpscQueueTest, PushBlocksUntilConsumed)
{
    UC::SpscQueue<size_t> q;
    q.Setup(4);  // usable capacity 3
    for (size_t i = 0; i < 3; ++i) { ASSERT_TRUE(q.TryPush(std::move(i))); }
    EXPECT_FALSE(q.TryPush(100));  // full
    std::atomic<bool> pushed{false};
    std::thread producer([&q, &pushed] {
        q.Push(999);  // blocks while the queue is full
        pushed.store(true, std::memory_order_release);
    });
    size_t v = 0;
    ASSERT_TRUE(q.TryPop(v));  // free one slot
    EXPECT_EQ(v, 0);
    for (int i = 0; i < 2000 && !pushed.load(std::memory_order_acquire); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(pushed.load());
    producer.join();
    ASSERT_TRUE(q.TryPop(v));
    EXPECT_EQ(v, 1);
    ASSERT_TRUE(q.TryPop(v));
    EXPECT_EQ(v, 2);
    ASSERT_TRUE(q.TryPop(v));
    EXPECT_EQ(v, 999);
    EXPECT_FALSE(q.TryPop(v));
}

TEST_F(UcmV2SpscQueueTest, ConsumerLoopProcessesInOrder)
{
    UC::SpscQueue<size_t> q;
    q.Setup(64);
    constexpr size_t n = 500;
    std::vector<size_t> seen(n, 0);
    std::atomic<size_t> received{0};
    std::atomic_bool stop{false};
    std::thread consumer([&q, &received, &seen, &stop] {
        q.ConsumerLoop(stop, [&received, &seen](size_t t) {
            seen[received.fetch_add(1, std::memory_order_acq_rel)] = t;
        });
    });
    for (size_t i = 0; i < n; ++i) { q.Push(std::move(i)); }
    for (int i = 0; i < 5000 && received.load() < n; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(received.load(), n);
    stop.store(true);
    consumer.join();
    for (size_t i = 0; i < n; ++i) { EXPECT_EQ(seen[i], i); }
}

TEST_F(UcmV2SpscQueueTest, ConsumerLoopForwardsArgsByMove)
{
    UC::SpscQueue<size_t> q;
    q.Setup(8);
    int moves = 0;
    int copies = 0;
    Tracer trc{&moves, &copies};
    std::atomic_bool stop{false};
    std::atomic<size_t> got{0};

    auto handler = [&got](Tracer t, size_t task) {
        (void)t;
        (void)task;
        ++got;
    };
    ASSERT_TRUE(q.TryPush(7));
    std::thread consumer(
        [&q, &stop, &handler, &trc] { q.ConsumerLoop(stop, handler, std::move(trc)); });
    for (int i = 0; i < 2000 && got.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop.store(true);
    consumer.join();
    EXPECT_EQ(got.load(), 1u);
    EXPECT_EQ(copies, 0);  // forwarded by move, never copied
    EXPECT_GE(moves, 1);
}

TEST_F(UcmV2SpscQueueTest, ConcurrentStress)
{
    UC::SpscQueue<size_t> q;
    q.Setup(64);
    constexpr size_t n = 10000;
    std::vector<size_t> seen(n, 0);
    std::atomic<size_t> received{0};
    std::atomic_bool stop{false};
    std::atomic_bool overflow{false};

    std::thread consumer([&q, &received, &seen, &stop, &overflow] {
        q.ConsumerLoop(stop, [&received, &seen, &overflow](size_t t) {
            size_t idx = received.fetch_add(1, std::memory_order_acq_rel);
            if (idx < seen.size()) {
                seen[idx] = t;
            } else {
                overflow.store(true);
            }
        });
    });
    std::thread producer([&q, &n] {
        for (size_t i = 0; i < n; ++i) { q.Push(std::move(i)); }
    });
    producer.join();
    for (int i = 0; i < 10000 && received.load() < n; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop.store(true);
    consumer.join();
    ASSERT_FALSE(overflow.load());
    ASSERT_EQ(received.load(), n);
    for (size_t i = 0; i < n; ++i) { EXPECT_EQ(seen[i], i); }
}
