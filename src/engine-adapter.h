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

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <functional>
#include <mutex>
#include <ostream>

#include "time-control.h"
#include "engine-process.h"
#include "game-start-position.h"
#include "game-record.h"
#include "engine-event.h"

struct EngineOption {
    enum class Type { Check, Spin, Combo, Button, String, Unknown };

    std::string name;
    Type type = Type::Unknown;
    std::string defaultValue;
    std::optional<int> min;
    std::optional<int> max;
    std::vector<std::string> vars; // for combo
};

using EngineOptions = std::unordered_map<std::string, EngineOption>;



enum class EngineState {
    Uninitialized,
    Initialized,     // After uciok
    Terminating      // Quitting
};

enum class TraceLevel: int {
	error,
    commands,
    handshake,
    info,
    none
};

 /**
  * @brief Abstract interface for communicating with and controlling a chess engine,
  *        independent of the underlying protocol (e.g. UCI, XBoard).
  */
class EngineAdapter {
public:
    EngineAdapter(std::filesystem::path enginePath,
        const std::optional<std::filesystem::path>& workingDirectory)
        : process_(enginePath, workingDirectory) {
    }
    virtual ~EngineAdapter() = default;

    /**
     * @brief Starts the engine process and initializes communication.
     * @return true if the engine was started successfully.
     */
    virtual void runEngine() = 0;

    /**
     * @brief Forcefully terminates the engine process and performs cleanup.
     */
    virtual void terminateEngine() = 0;

    /**
     * Blocks until a new engine output line is available and returns it as an interpreted EngineEvent.
     * This method is called exclusively by the read loop of the EngineWorker.
     *
     * @return A semantically interpreted EngineEvent.
     */
    virtual EngineEvent readEvent() = 0;

	/**
	 * @brief Sends a are you ready command to the engine.
	 */
	virtual void askForReady() = 0;

    /**
     * @brief Prepares the engine for a new game.
     * @param info Game-specific initialization parameters.
     */
    virtual void newGame() = 0;

    /**
     * @brief Immediately requests the engine to produce a move, e.g. in force mode.
     */
    virtual void moveNow() = 0;

    /**
     * @brief Enables or disables ponder mode.
     */
    virtual void setPonder(bool enabled) = 0;

    /**
     * @brief Called once per second. Useful for time-based monitoring or updates.
     */
    virtual void ticker() = 0;

    /**
     * @brief Informs the engine that pondering is permitted.
     * @param game        Current game state.
     * @param limits      Calculation limits (time, depth, etc.).
     */
    virtual void ponder(const GameRecord& game, GoLimits& limits) = 0;

    /**
     * @brief Requests the engine to calculate a move.
	 * @param game        Current game state.
	 * @param limits      Calculation limits (time, depth, etc.).
	 * @param limitMoves  Optional list of moves to consider.
	 * @returns the timestamp the calculate move commad has been sent to the engine.
     */
    virtual int64_t computeMove(const GameRecord& game, const GoLimits& limits) = 0;

    /**
     * @brief Instructs the engine to stop calculation.
     */
    virtual void stopCalc() = 0;

	/**
	 * @brief Sends a command to the engine's stdin.
	 * @param command Command to send (without newline).
	 */
    int64_t writeCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(commandMutex_);
        logToEngine(command);
        return process_.writeLine(command);
    }

    /**
     * @brief Assigns a logger function to use for engine communication output.
     *        Typically called by the EngineWorker to inject context.
     */
    void setLogger(std::function<void(std::string_view, bool)> logger) {
        logger_ = std::move(logger);
    }

	/**
	 * @brief Checks if the engine is currently running.
	 * @return true if the engine is initialized and running.
	 */
    bool isRunning() {
		return (state_ == EngineState::Initialized) && process_.isRunning();
    }

    /**
     * Returns the current memory usage (in bytes) of the engine process.
     */
    std::size_t getEngineMemoryUsage() const {
        return process_.getMemoryUsage();
    };

    /**
     * @brief Sends a UCI 'setoption' command to the engine with the given name and value.
     *
     * This method does not validate option names or values. It is intended for testing
     * purposes, including sending intentionally invalid options.
     *
     * @param name The name of the UCI option to set.
     * @param value The value to assign to the option. May be empty.
     */
    virtual void setOption(const std::string& name, const std::string& value = {}) = 0;

    /**
     * @brief Returns the current engine option list.
     */
    const EngineOptions& getSupportedOptions() const { return supportedOptions_; }


    /**
     * Sets the protocol output file for logging communication.
     *
     * @param filename Path to the file to append protocol lines.
     */
    void setProtocolFile(const std::string& filename);

    /**
     * Writes a single protocol line to the configured file.
     *
     * @param line Text line to be appended and flushed immediately.
     */
    void writeProtocolLine(const std::string& line);
protected:
    /**
     * @brief Emits a log message using the configured logger, if any.
     */
    void logFromEngine(std::string_view message) const {
        if (logger_) {
            logger_(message, true);
        }
    }
	void logToEngine(std::string_view message) const {
		if (logger_) {
			logger_(message, false);
		}
	}
    EngineOptions supportedOptions_;
    mutable std::function<void(std::string_view, bool)> logger_;
    std::atomic<EngineState> state_ = EngineState::Uninitialized;
    EngineProcess process_;
    std::mutex commandMutex_;
	TraceLevel traceLevel_ = TraceLevel::handshake;
    std::ofstream protocolStream_;

};
