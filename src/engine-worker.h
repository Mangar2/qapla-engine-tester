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
#include <thread>
#include "game-record.h"
#include "engine-adapter.h"
#include "engine-config.h"

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
	 * @param identifier A unique identifier for the engine represented by the worker.
	 * @param engineConfig Configuration for the engine, including executable path and options.
	 */
	explicit EngineWorker(std::unique_ptr<EngineAdapter> adapter,
		std::string identifier,
		const EngineConfig& engineConfig);

	/**
	 * @brief Destructs the worker and cleanly shuts down its thread.
	 */
	~EngineWorker();

	EngineWorker(const EngineWorker&) = delete;
	EngineWorker& operator=(const EngineWorker&) = delete;


	const std::string& getIdentifier() const {
		return identifier_;
	}

	/**
	 * @brief Terminates the engine process and stops the worker thread.
	 * @param wait If true, waits for the write and read threads to finish before returning.
	 */
	void stop(bool wait = true);

	/**
	 * @brief Führt isready/readyok durch. 
	 * @param timeout Zeitspanne, die maximal gewartet werden soll.
	 * @return true, wenn readyok empfangen wurde, andernfalls false (z. B. bei Hänger).
	 */
	bool requestReady(std::chrono::milliseconds timeout = ReadyTimeoutNormal);

	/**
	 * Requests the engine to compute the best move for the given game state and search limits.
	 *
	 * @param gameRecord The current game information (includes starting position and move history).
	 * @param limits The constraints for the upcoming search (time, depth, nodes, etc.).
	 * @param ponderHit If true, indicates that the engine is currently pondering on the right move.
	 */
	void computeMove(const GameRecord& gameRecord, const GoLimits& limits, bool ponderHit = false);

	/**
	 * @brief Allows the engine to ponder during its turn.
	 * @param game The current game with start position and moves played so far.
	 * @param limits The time limits for the next move.
	 * @param ponderMove The move to ponder, if any.
	 */
	void allowPonder(const GameRecord& game, const GoLimits& limits, std::string ponderMove);

	/**
	 * @brief Sends a command to the engine to prepare for a new game.
	 */
	void newGame(const GameRecord& gameRecord, bool engineIsWhite) {
		post([=](EngineAdapter& adapter) {
			adapter.newGame(gameRecord, engineIsWhite);
			});
	}

	/**
	 * @brief Sends a command to the engine to stop the current move calculation and send the best move.
	 * @param wait If true, waits for the best move to be received before returning. 
	 * Note: the best move is not sent to the GameManager in this case.
	 * @param timeout The maximum time to wait for the best move response.
	 * @return true if the best move was received, false if the timeout was reached or the engine is not ready.
	 */
	bool moveNow(bool wait = false, std::chrono::milliseconds timeout = BestMoveTimeout);

	/**
	 * @brief Sets the event sink for engine events.
	 *
	 * The event sink is a callback function that will be called with engine events.
	 * This allows the main application to handle engine events asynchronously.
	 *
	 * @param sink The event sink function.
	 */
	void setEventSink(std::function<void(EngineEvent&&)> sink) {
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
	}

	/**
	 * @brief Sets the engine's option with the given name to the specified value.
	 *
	 * @param name The name of the option to set.
	 * @param value The value to set for the option.
	 * @return true if the option isready/readyok handshake came in time.
	 */
	bool setOption(const std::string& name, const std::string& value);

	bool setOptionValues(const OptionValues& values);

	/**
	 * @brief Gets a map of the supported options of the engine with the default values.
	 *
	 */
	EngineOptions getSupportedOptions() {
		return adapter_->getSupportedOptions();
	}

	/**
	 * @brief Checks if the worker is in a failure state.
	 * @return true if the worker encountered a failure, e.g., during startup or runtime.
	 */
	bool failure() const {
		return workerState_ == WorkerState::failure;
	}

	/**
	 * @brief Returns the name of the engine.
	 */
	std::string getEngineName() const {
		return adapter_->getEngineName();
	}

	/**
	 * @brief Returns the author of the engine.
	 */
	std::string getEngineAuthor() const {
		return adapter_->getEngineAuthor();
	}

	/**
	 * @brief Returns the welcome message of the engine.
	 */
	std::string getWelcomeMessage() const {
		return adapter_->getWelcomeMessage();
	}

	/**
	 * @brief Sets the type of handshake to wait for. It is public to be used for testing purposes.
	 */
	void setWaitForHandshake(EngineEvent::Type type) {
		waitForHandshake_ = type;
	}

	/**
	 * @brief Waits for the engine to be ready for the next command.
	 *
	 * It blocks until the engine is ready or the timeout is reached.
	 *
	 * @param timeout The maximum time to wait for the engine to send the handshake (readyok, uciok).
	 * @return true if the engine is ready, false if the timeout was reached.
	 */
	bool waitForHandshake(std::chrono::milliseconds timeout);

	const EngineConfig& getConfig() const {
		return engineConfig_;
	}

	EngineConfig& getConfigMutable() {
		return engineConfig_;
	}

	enum class EventFilter {
		None = 0,
		EmptyEvent = 1,
		InfoEvent = 2
	};

	/**
	 * @brief Sets the trace level for the engine's CLI output.
	 * @param traceLevel The trace level to set.
	 */
	void setTraceLevel(TraceLevel traceLevel) {
		cliTraceLevel_ = traceLevel;
	}

private:

	enum class WorkerState {
		notStarted,
		starting,
		running,
		failure,
		stopped,
		terminated
	};

	/*
	 * @brief processs the startup of the engine asynchronously.
	 * @param adapter The engine adapter to control. Ownership is transferred.
	 * @param optionValues option values to set for the engine in the startup process.
	 */
	void asyncStartup(const OptionValues& optionValues);

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
	 * @brief loop to get engine output.
	 *
	 */
	void readLoop();
	void writeLoop();
	
	static constexpr std::chrono::seconds ReadyTimeoutNormal{ 3 };
	static constexpr std::chrono::seconds BestMoveTimeout{ 2 };
	static constexpr std::chrono::seconds ReadyTimeoutProtocolOk{ 5 };
	static constexpr std::chrono::seconds ReadyTimeoutOption{ 10 };

	std::queue<std::optional<std::function<void(EngineAdapter&)>>> writeQueue_;
	std::string identifier_;

	std::atomic<WorkerState> workerState_ = WorkerState::notStarted;
	std::atomic<bool> disconnected_ = false;

	// Startup synchronization
	std::promise<void> startupPromise_;
	std::future<void> startupFuture_;

	// Handshake synchronization
	std::mutex handshakeMutex_;
	std::condition_variable handshakeCv_;
	bool handshakeReceived_ = false;
	EngineEvent::Type waitForHandshake_ = EngineEvent::Type::None;

	// Read thread
	std::thread readThread_;

	// Work thread
	std::thread writeThread_;
		std::mutex mutex_;
	std::condition_variable cv_;
	std::unique_ptr<EngineAdapter> adapter_;

	// GameManager communication
	std::function<void(EngineEvent&&)> eventSink_;

	// Engine configuration
	EngineConfig engineConfig_;

	TraceLevel cliTraceLevel_ = TraceLevel::info;

};
