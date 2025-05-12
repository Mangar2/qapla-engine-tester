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

#include "engine-process.h"
#include "game-start-position.h"
#include "game-state.h"

using OptionMap = std::unordered_map<std::string, std::string>;

/**
 * @brief limits for calculating a single move.
 */
struct GoLimits {
    int64_t wtimeMs = 0;
    int64_t btimeMs = 0;
    int64_t wincMs = 0;
    int64_t bincMs = 0;
    int32_t movesToGo = 0;

    std::optional<int> depth;
    std::optional<int> nodes;
    std::optional<int> mateIn;
    std::optional<int64_t> movetimeMs;
    std::optional<std::vector<std::string>> limitMoves;
    bool infinite = false;
};

struct SearchInfo {
    std::optional<int> depth;
    std::optional<int> selDepth;
    std::optional<int64_t> timeMs;
    std::optional<int64_t> nodes;
    std::optional<int64_t> nps;
    std::vector<std::string> pv;
};

struct EngineEvent {
    enum class Type {
        ComputeMoveSent,
        ReadyOk,
        BestMove,
        Info,
        PonderHit,
        Error,
        Unknown,
        NoData
    };

    Type type;
    int64_t timestampMs;
    std::string rawLine;

    std::optional<std::string> bestMove;
    std::optional<std::string> ponderMove;
    std::optional<std::string> errorMessage;

    std::optional<SearchInfo> searchInfo;
};

enum class EngineState {
    Uninitialized,
    Initialized,     // After uciok
    Terminating      // Quitting
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
    virtual void ponder(const GameState& game, GoLimits& limits) = 0;

    /**
     * @brief Requests the engine to calculate a move.
	 * @param game        Current game state.
	 * @param limits      Calculation limits (time, depth, etc.).
	 * @param limitMoves  Optional list of moves to consider.
	 * @returns the timestamp the calculate move commad has been sent to the engine.
     */
    virtual int64_t computeMove(const GameState& game, const GoLimits& limits) = 0;

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
     * @brief Returns the current engine option list.
     */
    virtual const OptionMap& getOptionMap() const = 0;

    /**
     * @brief Applies a new engine option list.
     */
    virtual void setOptionMap(const OptionMap& list) = 0;

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
		return state_ == EngineState::Initialized;
    }

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
    mutable std::function<void(std::string_view, bool)> logger_;
    std::atomic<EngineState> state_ = EngineState::Uninitialized;
    EngineProcess process_;
    std::mutex commandMutex_;

};
