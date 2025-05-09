/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 */
#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <optional>

class EngineAdapter;

/**
 * @brief Asynchronous execution wrapper for an EngineAdapter.
 *
 * EngineWorker owns an EngineAdapter instance and processes tasks in a dedicated thread.
 * This allows the main application to remain responsive while interacting with the engine synchronously.
 *
 * Tasks are posted via the `post()` method and are executed in the order they were received.
 * This enforces a serialized command stream to the engine and avoids race conditions.
 */
class EngineWorker {
public:
    /**
     * @brief Constructs the worker and starts its internal thread.
     * @param adapter The engine adapter to control. Ownership is transferred.
     */
    explicit EngineWorker(std::unique_ptr<EngineAdapter> adapter);

    /**
     * @brief Destructs the worker and cleanly shuts down its thread.
     */
    ~EngineWorker();

    EngineWorker(const EngineWorker&) = delete;
    EngineWorker& operator=(const EngineWorker&) = delete;

    /**
     * @brief Gracefully shuts down the engine worker and the associated engine.
     *
     * Sends "quit" to the engine, attempts a clean shutdown, and forcibly terminates
     * the engine process if necessary. Then joins the worker thread.
     */
    void stop();

private:

    /**
     * @brief Posts a task to be executed on the worker thread.
     *
     * The task receives a reference to the internal EngineAdapter and can perform any synchronous
     * sequence of operations. Tasks are executed sequentially in FIFO order.
     *
     * @param task The task to execute.
     */
    void post(std::optional<std::function<void(EngineAdapter&)>> task);

    void threadLoop();

    std::queue<std::optional<std::function<void(EngineAdapter&)>>> taskQueue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    bool running_ = true;
    std::unique_ptr<EngineAdapter> adapter_;
};
