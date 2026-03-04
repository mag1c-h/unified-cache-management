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
#ifndef UNIFIEDCACHE_INFRA_TIMER_TICK_H
#define UNIFIEDCACHE_INFRA_TIMER_TICK_H

#include <algorithm>
#include <chrono>
#include <fmt/format.h>
#include <string>
#include <vector>

namespace UC {

class TimerTick {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

public:
    explicit TimerTick(size_t reserve = 8) { stamps_.reserve(reserve); }
    void Start() { Start(Clock::now()); }
    void Start(double startTime)
    {
        using namespace std::chrono;
        Start(TimePoint(duration_cast<typename Clock::duration>(duration<double>(startTime))));
    }
    void Start(TimePoint startTime)
    {
        stamps_.clear();
        stamps_.push_back(startTime);
    }
    void Tick() { stamps_.push_back(Clock::now()); }
    template <typename Unit = std::chrono::milliseconds>
    std::string Report(std::string_view f = "{:.3f}", std::string_view s = "-") const
    {
        fmt::memory_buffer out;
        for (size_t i = 1; i < stamps_.size(); ++i) {
            auto diff = TimeDiff<Unit>(stamps_[i - 1], stamps_[i]);
            fmt::format_to(std::back_inserter(out), f, diff);
            if (i + 1 != stamps_.size()) { fmt::format_to(std::back_inserter(out), s); }
        }
        if (stamps_.size() > 2) {
            fmt::format_to(std::back_inserter(out), "=");
            auto total = TimeDiff<Unit>(stamps_.front(), stamps_.back());
            fmt::format_to(std::back_inserter(out), f, total);
        }
        return std::string(out.data(), out.size());
    }

private:
    template <typename Unit>
    static double TimeDiff(const TimePoint& t1, const TimePoint& t2)
    {
        using namespace std::chrono;
        return duration_cast<duration<double, typename Unit::period>>(t2 - t1).count();
    }

private:
    std::vector<TimePoint> stamps_;
};

}  // namespace UC

#endif
