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

#include "engine-process.h"
#include "engine-option.h"

#include "../base-elements/time-control.h"
#include "../chess-game/game-record.h"
#include "../chess-game/engine-event.h"
#include "../base-elements/string-helper.h"

#include <string>
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <functional>
#include <atomic>
#include <mutex>

namespace QaplaTester {

using OptionValues = std::unordered_map<std::string, std::string>;

 /**
  * @brief Abstract interface for communicating with and controlling a chess engine,
  *        independent of the underlying protocol (e.g. UCI, XBoard).
  */
class EngineAdapter {
public:
    explicit EngineAdapter(const EngineStartupParams& params);
    virtual ~EngineAdapter();

    /**
     * @brief Starts the engine protokoll.
     */
    virtual void startProtocol() = 0;

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
     * @param game        Current game state.
	 * @param engineIsWhite True if the engine plays as white, false for black.
     */
    virtual void newGame(const GameRecord& game, bool engineIsWhite) = 0;

    /**
     * @brief Sets the time control for the engine. 
     * 
     * Depending on protocol it might do nothing or send a new game command 
     * and then the new time control
     * 
     * @param game Current game state.
	 * @param engineIsWhite True if the engine plays as white, false for black.
     */
    virtual void setTimeControl(const GameRecord& game, bool engineIsWhite) = 0;

    /**
     * @brief Notifies the engine that the best move has been received.
     * @param sanMove The best move in SAN notation.
     * @param lanMove The best move in LAN notation.
     */
    virtual void bestMoveReceived(
        [[maybe_unused]] const std::string& sanMove,
        [[maybe_unused]] const std::string& lanMove) {};

    /**
     * @brief Immediately requests the engine to produce a move, e.g. in force mode.
     */
    virtual void moveNow() = 0;

    /**
     * @brief Stops the engine's current calculation.
     */
    virtual void stop() = 0;

    /**
     * @brief Enables or disables ponder mode.
     */
    virtual void setPonder(bool enabled) { ponderMode_ = enabled; }

    /**
     * @brief Called once per second. Useful for time-based monitoring or updates.
     */
    virtual void ticker() = 0;

    /**
     * @brief Is called after a moveNow command with wait=true. Runs handshake steps if needed.
     * @return The event to wait for completing the handshake.
     */
    [[nodiscard]] virtual EngineEvent::Type waitAfterMoveNowHandshake() = 0;

    /**
     * @brief Returns the handshake event type a ponder miss produces, without sending anything.
     * (BestMove for UCI, None for XBoard). Allows arming the handshake before handlePonderMiss()
     * sends any command, so the engine's reply cannot cross with the arming.
     */
    [[nodiscard]] virtual EngineEvent::Type waitAfterPonderMissHandshake() = 0;

    /**
     * @brief Handles a ponder miss (opponent played a different move than expected).
     */
    virtual void handlePonderMiss() = 0;

    /**
     * @brief Informs the engine that pondering is permitted.
     * @param game        Current game state.
     * @param limits      Calculation limits (time, depth, etc.).
     * @param ponderMove  Move to ponder on
     * @return Timestamp when the command was sent.
     */
    [[nodiscard]] virtual uint64_t allowPonder(const GameStruct& game, const GoLimits& limits, std::string ponderMove) = 0;

    /**
     * @brief Requests the engine to calculate a move.
	 * @param game        Current game state.
	 * @param limits      Calculation limits (time, depth, etc.).
	 * @param ponderHit   true, if the engine is currently pondering on the right move.
	 * @return the timestamp the calculate move command has been sent to the engine.
     */
    [[nodiscard]] virtual uint64_t computeMove(const GameStruct& game, const GoLimits& limits, bool ponderHit) = 0;

	/**
	 * @brief Sends a command to the engine's stdin.
	 * @param command Command to send (without newline).
	 * @return Timestamp when the command was sent.
	 */
    uint64_t writeCommand(const std::string& command);

    /**
     * @brief Assigns a logger function to use for engine communication output.
     *        Typically called by the EngineWorker to inject context.
     */
    void setProtocolLogger(std::function<void(std::string_view, bool, TraceLevel)> logger) {
        logger_ = std::move(logger);
    }

	/**
	 * @brief Checks if the engine is currently running.
	 * @return true if the engine is initialized and running.
	 */
    [[nodiscard]] bool isRunning() {
		return !terminating_ && process_.isRunning();
    }

    /**
     * Indicates whether this protocol requires an explicit confirmation (ProtocolOk)
     * to complete the startup handshake.
     *
     * For example:
     * - UCI requires a "uciok" response â†’ must return true.
     * - Winboard may omit "feature done=1" â†’ must return false.
     *
     * The EngineWorker uses this to decide whether a missing ProtocolOk
     * should be treated as a fatal error or as a valid timeout-based completion.
     *
     * @return true if ProtocolOk is mandatory for this protocol, false if optional.
     */
    [[nodiscard]] virtual bool isProtocolOkRequired() const = 0;


    /**
     * Returns the current memory usage (in bytes) of the engine process.
     */
    [[nodiscard]] std::size_t getEngineMemoryUsage() const {
        return process_.getMemoryUsage();
    }

    /**
     * @brief Sends a UCI 'setoption' command to the engine with the given name and value.
     *
     * This method does not validate option names or values. It is intended for testing
     * purposes, including sending intentionally invalid options.
     *
     * @param name The name of the UCI option to set.
     * @param value The value to assign to the option. May be empty.
     */
    virtual void setTestOption(const std::string& name, const std::string& value = {}) = 0;

	/**
	 * @brief Sets the engine's options based on the provided OptionValues.
	 *        This method should be implemented by derived classes to apply
	 *        the options to the engine.
	 *
	 * @param optionValues The option values to set.
	 */
    virtual void setOptionValues(const OptionValues& optionValues) = 0;

    /**
     * @brief Returns the current engine option list.
     */
    [[nodiscard]] const EngineOptions& getSupportedOptions() const { return supportedOptions_; }

	/**
	 * @brief Returns the engine's executable path.
	 */
	[[nodiscard]] std::string getExecutablePath() const {
		return process_.getExecutablePath();
	}

	/**
	 * @brief Returns the name of the engine.
	 */
	[[nodiscard]] std::string getEngineName() const {
		return engineName_;
	}

	/**
	 * @brief Returns the author of the engine.
	 */
	[[nodiscard]] std::string getEngineAuthor() const {
		return engineAuthor_;
	}

	/**
	 * @brief Returns the welcome message of the engine.
	 */
	[[nodiscard]] std::string getWelcomeMessage() const {
		return welcomeMessage_;
	}

	/**
	 * @brief Sets whether to suppress info lines.
	 * @param suppress True to suppress, false otherwise.
	 */
	void setSuppressInfoLines(bool suppress) {
		suppressInfoLines_ = suppress;
	}

protected:
    
    /**
     * @brief Emits a log message using the configured logger, if any.
     * @param message The message to log.
     * @param level The trace level.
     */
    void logFromEngine(std::string_view message, TraceLevel level) const {
        if (logger_) {
            logger_(message, true, level);
        }
    }
	/**
	 * @brief Emits a log message for commands sent to the engine.
	 * @param message The message to log.
	 * @param level The trace level.
	 */
	void logToEngine(std::string_view message, TraceLevel level) const {
		if (logger_) {
			logger_(message, false, level);
		}
	}
	/**
	 * @brief Gets the supported option by name.
	 * @param name The name of the option.
	 * @return The option if found, nullopt otherwise.
	 */
	[[nodiscard]] std::optional<EngineOption> getSupportedOption(const std::string& name) const {
        auto key = QaplaHelpers::to_lowercase(name);
		for (const auto& option : supportedOptions_) {
			if (QaplaHelpers::to_lowercase(option.name) == key) {
				return option;
			}
		}
		return std::nullopt;
	}
    EngineOptions supportedOptions_;
    mutable std::function<void(std::string_view, bool, TraceLevel)> logger_;
    std::atomic<bool> terminating_ = false;
    EngineProcess process_;
    std::mutex commandMutex_;
    
    std::string engineName_;
    std::string engineAuthor_;
    std::string welcomeMessage_;

    std::string identifier_;

	bool ponderMode_ = false;
    bool suppressInfoLines_ = false;

};

} // namespace QaplaTester
