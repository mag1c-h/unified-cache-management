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

#include "status/status.h"
#include "type/types.h"

namespace UC::Store {

class StoreV2 {
public:
    virtual ~StoreV2() = default;

    /**
     * @brief Sets up and configures the Store instance with the provided configuration.
     *
     * @param dict A dictionary containing configuration key-value pairs specific
     *             to this Store instance. The content and structure of the dictionary
     *             may vary depending on the concrete implementation.
     *
     * @return Status indicating the result of the setup operation:
     *         - Status::Ok() if setup was successful
     *         - Error status with appropriate code and message if configuration is
     *           invalid or setup fails
     *
     * @note Implementations should validate all required configuration parameters
     *       and perform any necessary resource allocation or initialization.
     * @note This method is called after object construction but before any processing
     *       operations that depend on the configuration.
     */
    virtual Status Setup(const Dictionary& dict) = 0;

    /**
     * @brief Get the readme information of the Store instance.
     *
     * @return Self descriptive information.
     */
    virtual std::string Readme() const = 0;

    /**
     * @brief Report the health of this Store instance.
     *
     * Performs a best-effort, non-blocking self-check of internal state and
     * resources (cache integrity, backing-store reachability, free space,
     * internal queues, etc.).
     *
     * @return Status
     *   - Status::Ok() if the instance is healthy and ready to serve.
     *   - Status::Unhealthy() (optionally via Status::Make with a descriptive
     *     message) if the instance is degraded or unavailable.
     *
     * @note Default implementation reports healthy (no-op); concrete stores
     *       that do not implement self-checking inherit a permissive default.
     * @note Thread-safe; may be called concurrently with other operations.
     */
    virtual Status HealthCheck() { return Status::Ok(); }

    /**
     * @brief Find the longest contiguous prefix of blocks present in storage.
     *
     * Scans the block array from the first element toward the last and returns
     * the index of the last block of the longest contiguous prefix [0..i] for
     * which every block is present in storage.
     *
     * @param blocks Array of block identifiers to test.
     * @param num Number of block identifiers to test.
     * @return Expected<ssize_t>
     *   - On success: the index of the last block of the longest contiguous
     *     prefix all present in storage; returns -1 if blocks[0] is absent.
     *   - On failure: appropriate Status code.
     */
    virtual Expected<ssize_t> LookupOnPrefix(const BlockId* blocks, size_t num) = 0;

    /**
     * @brief Find the last block present in storage.
     *
     * Scans the block array from the last element toward the first and returns
     * the index of the first block found in storage. Presence is not required
     * to be contiguous.
     *
     * @param blocks Array of block identifiers to test.
     * @param num Number of block identifiers to test.
     * @return Expected<ssize_t>
     *   - On success: the maximum index @p i such that blocks[i] is present in
     *     storage; returns -1 if none are found.
     *   - On failure: appropriate Status code.
     */
    virtual Expected<ssize_t> LookupOnReverse(const BlockId* blocks, size_t num) = 0;

    /**
     * @brief Hint the store to prefetch given blocks into high-speed cache.
     *
     * This call is **non-blocking** and **fire-and-forget**; it returns
     * immediately and carries no completion guarantee. Implementations may
     * ignore the hint if prefetching is not supported or resources are
     * unavailable.
     *
     * @param blocks Array of block identifiers to be prefetched.
     * @param num Number of block identifiers to be prefetched.
     *
     * @note Thread-safe; may be called concurrently with other operations.
     * @note Default implementation does nothing.
     */
    virtual void Prefetch(const BlockId* blocks, size_t num) {}

    /**
     * @brief Refresh hotness of the given blocks across the store stack.
     *
     * Advisory, fire-and-forget hint: the caller has determined these blocks
     * are (or will soon be) accessed and asks the store to refresh their
     * hotness so the store's own eviction policy keeps them resident.
     * Implementations may ignore the hint or interpret "hotness" per their
     * eviction policy (bump recency, increment a hotness score, move to MRU
     * position, soft-reference with decay, etc.). There is NO paired release
     * call; hotness decays per the store's own policy.
     *
     * Cascading contract: implementations MUST forward the SAME blocks to
     * their backend (lower-layer store), not only the misses. A block that
     * hits in this layer must also be warmed in lower layers, so that if this
     * layer later evicts it, the lower layer still holds a warm copy and a
     * re-load is served from the lower layer rather than the source.
     *
     * @param blocks Array of block identifiers to touch.
     * @param num Number of block identifiers.
     *
     * @note Thread-safe; may be called concurrently with other operations.
     * @note Default implementation does nothing.
     */
    virtual void Touch(const BlockId* blocks, size_t num) {}

    /**
     * @brief Start an asynchronous load (storage → device) transfer.
     *
     * For each shard in @p task.shards, the store transfers data from storage
     * into the device buffers listed in shard.addrs. Each Io specifies a
     * destination buffer (@p io.addr) and a byte length to transfer
     * (@p io.length). Io's within a shard are contiguous, non-overlapping, and
     * tiled from offset 0; the storage-side byte offset of each Io is obtained
     * as the prefix sum of preceding Io lengths.
     *
     * @param task Batch descriptor (see TaskDesc): shards to load.
     * @return Expected<TaskHandle>
     *   - On success: a task handle that can be passed to Wait() or Check().
     *   - On failure: relevant Status code.
     */
    virtual Expected<TaskHandle> Load(TaskDesc task) = 0;

    /**
     * @brief Start an asynchronous dump (device → storage) transfer.
     *
     * For each shard in @p task.shards, the store transfers data from the
     * device buffers listed in shard.addrs into storage. Each Io specifies a
     * source buffer (@p io.addr) and a byte length to transfer (@p io.length).
     * Io's within a shard are contiguous, non-overlapping, and tiled from
     * offset 0; the storage-side byte offset of each Io is obtained as the
     * prefix sum of preceding Io lengths.
     *
     * @param task Batch descriptor (see TaskDesc): shards to dump.
     * @return Expected<TaskHandle>
     *   - On success: a task handle that can be passed to Wait() or Check().
     *   - On failure: relevant Status code.
     */
    virtual Expected<TaskHandle> Dump(TaskDesc task) = 0;

    /**
     * @brief Block until the specified task completes.
     *
     * @param taskId Task handle returned by Load() or Dump().
     * @return Status::Ok on successful completion, otherwise an error code
     *         describing the failure.
     */
    virtual Status Wait(TaskHandle taskId) = 0;
};

}  // namespace UC::Store
