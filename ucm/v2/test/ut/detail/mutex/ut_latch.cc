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
#include <gtest/gtest.h>
#include <string>
#include "mutex/latch.h"

class UcmV2LatchTest : public testing::Test {};

TEST_F(UcmV2LatchTest, FreshLatchIsFinished)
{
    UC::Latch latch;
    EXPECT_TRUE(latch.Finished());
    EXPECT_GE(latch.Elapsed(), 0.0);
    EXPECT_TRUE(latch.WaitFor(0));
    EXPECT_TRUE(latch.WaitUntil(0));
}

TEST_F(UcmV2LatchTest, SetMakesUnfinished)
{
    UC::Latch latch;
    latch.Set(3);
    EXPECT_FALSE(latch.Finished());
}

TEST_F(UcmV2LatchTest, DoneDecrementsToZero)
{
    UC::Latch latch;
    latch.Set(2);
    EXPECT_FALSE(latch.Finished());
    latch.Done();
    EXPECT_FALSE(latch.Finished());
    latch.Done();
    EXPECT_TRUE(latch.Finished());
}

TEST_F(UcmV2LatchTest, UpIncrementsCounter)
{
    UC::Latch latch;
    latch.Set(0);
    EXPECT_TRUE(latch.Finished());
    latch.Up();
    EXPECT_FALSE(latch.Finished());
    latch.Up();
    EXPECT_FALSE(latch.Finished());
    latch.Done();
    EXPECT_FALSE(latch.Finished());
    latch.Done();
    EXPECT_TRUE(latch.Finished());
}

TEST_F(UcmV2LatchTest, EpilogRunsOnceOnCompletion)
{
    UC::Latch latch;
    int calls = 0;
    latch.Set(1);
    latch.SetEpilog([&] { calls++; });
    latch.Done();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(latch.Finished());
}

TEST_F(UcmV2LatchTest, EpilogNotRunBeforeCompletion)
{
    UC::Latch latch;
    int calls = 0;
    latch.Set(2);
    latch.SetEpilog([&] { calls++; });
    latch.Done();
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(latch.Finished());
    latch.Done();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(latch.Finished());
}

TEST_F(UcmV2LatchTest, DoneAfterFinishedDoesNotRerunEpilog)
{
    UC::Latch latch;
    int calls = 0;
    latch.Set(1);
    latch.SetEpilog([&] { calls++; });
    latch.Done();
    ASSERT_EQ(calls, 1);
    latch.Done();
    latch.Done();
    EXPECT_EQ(calls, 1);
}

TEST_F(UcmV2LatchTest, SetEpilogOverwritesPrevious)
{
    UC::Latch latch;
    std::string who;
    latch.Set(1);
    latch.SetEpilog([&] { who = "a"; });
    latch.SetEpilog([&] { who = "b"; });
    latch.Done();
    EXPECT_EQ(who, "b");
}

TEST_F(UcmV2LatchTest, WaitReturnsImmediatelyWhenFinished)
{
    UC::Latch latch;
    latch.Wait();
    SUCCEED();
}

TEST_F(UcmV2LatchTest, TimedOutOnFreshLatch)
{
    UC::Latch latch;
    EXPECT_TRUE(latch.TimedOut(0));
    EXPECT_FALSE(latch.TimedOut(1000000));
}
