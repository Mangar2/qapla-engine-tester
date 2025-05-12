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
#include <future>
#include "game-state.h"
#include "engine-adapter.h"

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


	void stop();

	/**
	 * @brief Führt einmalig nach dem Start der Engine ein erweitertes isready/readyok durch.
	 *        Timeout ist festgelegt intern.
	 * @return true, wenn readyok empfangen wurde, andernfalls false (z. B. bei Hänger).
	 */
	bool startupReady();

	/**
	 * Requests the engine to compute the best move for the given game state and search limits.
	 *
	 * @param gameState The current game state (includes starting position and move history).
	 * @param limits The constraints for the upcoming search (time, depth, nodes, etc.).
	 */
	void computeMove(const GameState& gameState, const GoLimits& limits);

	/**
	 * @brief Sends a command to the engine to prepare for a new game.
	 */
	void newGame() {
		post([this](EngineAdapter& adapter) {
			adapter.newGame();
			});
	}

	/**
	 * @brief Sets the event sink for engine events.
	 *
	 * The event sink is a callback function that will be called with engine events.
	 * This allows the main application to handle engine events asynchronously.
	 *
	 * @param sink The event sink function.
	 */
	void setEventSink(std::function<void(const EngineEvent&)> sink) {
		eventSink_ = std::move(sink);
	}

	/**
	 * Returns a future that becomes ready when the engine has completed its startup phase.
	 * This includes process launch, UCI handshake, and isready/readyok confirmation.
	 */
	std::future<void> getStartupFuture() {
		return std::move(startupFuture_);
	}

	/**
	 * Returns the current memory usage (in bytes) of the engine process.
	 */
	std::size_t getEngineMemoryUsage() const {
		if (!adapter_) {
			return 0;
		}
		return adapter_->getEngineMemoryUsage();
	};

	void setOption(const std::string& name, const std::string& value) {
		post([this, name, value](EngineAdapter& adapter) {
			adapter.setOption(name, value);
			});
	}

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

	// Startup synchronization
	std::promise<void> startupPromise_;
	std::future<void> startupFuture_;

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

	// GameManager communication
private:
	std::function<void(const EngineEvent&)> eventSink_;
};
