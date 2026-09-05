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

#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace UC {

using BlockId = std::array<std::byte, 16>; /* 16-byte block hash */
using TaskHandle = std::size_t;            /* Opaque task token (0 = invalid) */

/**
 * @brief Hasher of BlockId
 */
struct BlockIdHasher {
    std::size_t operator()(const BlockId& blockId) const noexcept
    {
        std::string_view sv(reinterpret_cast<const char*>(blockId.data()), blockId.size());
        return std::hash<std::string_view>{}(sv);
    }
};

/**
 * @brief Describes one Io (tensor) of a shard.
 */
struct Io {
    void* addr{};         /* Buffer address */
    std::size_t length{}; /* Byte length to transfer */
};

/**
 * @brief Describes one shard (slice) of a block.
 */
struct Shard {
    BlockId owner{};        /* Parent block identifier */
    std::size_t blockPos{}; /* Optional: position of the block within the request */
    std::size_t offset{};   /* Shard offset inside the block */
    bool last{false};       /* Optional: determine the last shard of owner */
    std::vector<Io> addrs;  /* Buffer addresses */
};

/**
 * @brief Batch descriptor for load or dump operations.
 */
struct TaskDesc {
    std::vector<Shard> shards; /* Shard being operated on */
    std::string brief;         /* Description of Task */
    /** Optional: prerequisite handle for dump. Cache stream waits before D2H. */
    uintptr_t prerequisiteHandle{0};
};

/**
 * @brief Key-value store backed by std::any for runtime type erasure.
 */
class Dictionary {
    std::unordered_map<std::string, std::any> data_;

public:
    /** Check whether a key exists in the dictionary. */
    bool Contains(const std::string& key) const { return data_.count(key) != 0; }
    /** Store a value of type T under the given key. */
    template <typename T>
    void Set(const std::string& key, T&& value)
    {
        data_[key] = std::forward<T>(value);
    }
    /** Store a numeric value as std::int64_t under the given key. */
    template <typename T>
    void SetNumber(const std::string& key, const T& value)
    {
        data_[key] = static_cast<std::int64_t>(value);
    }
    /** Retrieve a value of type T; leaves target unchanged if key is absent. */
    template <typename T>
    void Get(const std::string& key, T& target) const
    {
        auto it = data_.find(key);
        if (it != data_.end()) { target = std::any_cast<T>(it->second); }
    }
    /** Retrieve a numeric value cast to T; leaves target unchanged if key is absent. */
    template <typename T>
    void GetNumber(const std::string& key, T& target) const
    {
        auto it = data_.find(key);
        if (it != data_.end()) { target = static_cast<T>(std::any_cast<std::int64_t>(it->second)); }
    }
    /**
     * Retrieve a numeric vector cast element-wise to T; leaves target unchanged if key is absent.
     * */
    template <typename T>
    void GetNumbers(const std::string& key, std::vector<T>& target) const
    {
        auto it = data_.find(key);
        if (it == data_.end()) { return; }
        const auto& v = std::any_cast<const std::vector<std::int64_t>&>(it->second);
        target.reserve(target.size() + v.size());
        for (const auto& d : v) { target.push_back(static_cast<T>(d)); }
    }
};

}  // namespace UC
