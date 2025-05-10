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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */
#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <optional>
#include "game-state.h"

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
    explicit EngineWorker(std::unique_ptr<EngineAdapter> adapter, std::string identifier);

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

    /**
     * Requests the engine to compute the best move for the given game state and search limits.
     *
     * @param gameState The current game state (includes starting position and move history).
     * @param limits The constraints for the upcoming search (time, depth, nodes, etc.).
     */
    void computeMove(const GameState& gameState, const GoLimits& limits);

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

	/**
	 * @brief Waits for the engine to be ready for the next command.
	 *
	 * It blocks until the engine is ready or the timeout is reached.
	 *
	 * @param timeout The maximum time to wait for the engine to be ready.
	 * @return true if the engine is ready, false if the timeout was reached.
	 */
    bool waitForReady(std::chrono::milliseconds timeout);

	/**
	 * @brief loop to get engine output.
	 *
	 */
    void readLoop();

    static constexpr std::chrono::milliseconds ReadyTimeoutNormal{ 2000 };
    static constexpr std::chrono::milliseconds ReadyTimeoutStartup{ 10000 };

    void threadLoop();

    std::queue<std::optional<std::function<void(EngineAdapter&)>>> taskQueue_;
    std::string identifier_;

    // Ready synchronization
    std::mutex readyMutex_;
    std::condition_variable readyCv_;
    bool readyReceived_ = false;

    // Read thread
	std::thread readThread_;

    // Work thread
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread workThread_;
    std::atomic<bool> running_ = false;
    std::unique_ptr<EngineAdapter> adapter_;
};
