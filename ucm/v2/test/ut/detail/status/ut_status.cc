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
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <string>
#include "status/status.h"

class UcmV2StatusTest : public testing::Test {};

TEST_F(UcmV2StatusTest, OkIsSuccess)
{
    auto s = UC::Store::Status::Ok();
    EXPECT_TRUE(s.Success());
    EXPECT_FALSE(s.Failure());
    EXPECT_EQ(s.Underlying(), 0);
}

TEST_F(UcmV2StatusTest, ErrorFactoriesReturnExpectedCodes)
{
    EXPECT_EQ(UC::Store::Status::Ok().Underlying(), 0);
    EXPECT_EQ(UC::Store::Status::General().Underlying(), -1);
    EXPECT_EQ(UC::Store::Status::InvalidParam().Underlying(), -50000);
    EXPECT_EQ(UC::Store::Status::OutOfMemory().Underlying(), -50001);
    EXPECT_EQ(UC::Store::Status::OsApiError().Underlying(), -50002);
    EXPECT_EQ(UC::Store::Status::DuplicateKey().Underlying(), -50003);
    EXPECT_EQ(UC::Store::Status::Retry().Underlying(), -50004);
    EXPECT_EQ(UC::Store::Status::NotFound().Underlying(), -50005);
    EXPECT_EQ(UC::Store::Status::Unsupported().Underlying(), -50008);
    EXPECT_EQ(UC::Store::Status::NoSpace().Underlying(), -50009);
    EXPECT_EQ(UC::Store::Status::Timeout().Underlying(), -50010);
    EXPECT_EQ(UC::Store::Status::Unhealthy().Underlying(), -50011);
}

TEST_F(UcmV2StatusTest, NonOkIsFailure)
{
    EXPECT_FALSE(UC::Store::Status::General().Success());
    EXPECT_TRUE(UC::Store::Status::General().Failure());
}

TEST_F(UcmV2StatusTest, EqualityComparesByCode)
{
    EXPECT_EQ(UC::Store::Status::Ok(), UC::Store::Status::Ok());
    EXPECT_NE(UC::Store::Status::Ok(), UC::Store::Status::NotFound());
    EXPECT_NE(UC::Store::Status::NotFound(), UC::Store::Status::Timeout());
}

TEST_F(UcmV2StatusTest, ToStringWithoutMessage)
{
    EXPECT_EQ(UC::Store::Status::NotFound().ToString(), "NotFound (-50005)");
}

TEST_F(UcmV2StatusTest, ToStringWithMessage)
{
    auto s = UC::Store::Status::Make(UC::Store::Status::Error::NotFound, "missing key {}", 42);
    EXPECT_EQ(s.ToString(), "NotFound (-50005):: missing key 42");
}

TEST_F(UcmV2StatusTest, MakeKeepsCodeAndFailure)
{
    auto s = UC::Store::Status::Make(UC::Store::Status::Error::InvalidParam, "bad input");
    EXPECT_EQ(s.Underlying(), -50000);
    EXPECT_TRUE(s.Failure());
}

TEST_F(UcmV2StatusTest, FormatAsMatchesToString)
{
    auto s = UC::Store::Status::Timeout();
    EXPECT_EQ(fmt::format("{}", s), s.ToString());
}

class UcmV2ExpectedTest : public testing::Test {};

TEST_F(UcmV2ExpectedTest, HoldsValue)
{
    UC::Store::Expected<int> e = 123;
    EXPECT_TRUE(e.HasValue());
    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.Value(), 123);
    EXPECT_EQ(*e, 123);
}

TEST_F(UcmV2ExpectedTest, HoldsError)
{
    UC::Store::Expected<int> e = UC::Store::Status::NotFound();
    EXPECT_FALSE(e.HasValue());
    EXPECT_FALSE(static_cast<bool>(e));
    EXPECT_EQ(e.Error().Underlying(), -50005);
}

TEST_F(UcmV2ExpectedTest, ValueOrReturnsValueWhenPresent)
{
    UC::Store::Expected<int> e = 7;
    EXPECT_EQ(e.ValueOr(0), 7);
}

TEST_F(UcmV2ExpectedTest, ValueOrReturnsDefaultOnError)
{
    UC::Store::Expected<int> e = UC::Store::Status::General();
    EXPECT_EQ(e.ValueOr(-1), -1);
}

TEST_F(UcmV2ExpectedTest, ArrowOperatorAccessesMembers)
{
    struct Foo {
        int a;
        int b;
        int Sum() const { return a + b; }
    };
    UC::Store::Expected<Foo> e = Foo{3, 4};
    EXPECT_EQ(e->a, 3);
    EXPECT_EQ(e->b, 4);
    EXPECT_EQ(e->Sum(), 7);
}

TEST_F(UcmV2ExpectedTest, MoveValueOut)
{
    UC::Store::Expected<std::string> e = std::string("hello");
    EXPECT_EQ(std::move(e).Value(), "hello");
}

TEST_F(UcmV2ExpectedTest, ValueOrOnRvalueError)
{
    UC::Store::Expected<int> e = UC::Store::Status::Timeout();
    EXPECT_EQ(std::move(e).ValueOr(99), 99);
}
