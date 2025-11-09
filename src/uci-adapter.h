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

#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <condition_variable>
#include <optional>
#include <unordered_map>
#include <iostream>

#include "engine-adapter.h"
#include "uci-option.h"

namespace QaplaTester {

 /**
  * @brief UCI protocol adapter implementing EngineAdapter.
  *        Runs the engine in a dedicated thread, handles UCI I/O.
  */
class UciAdapter : public EngineAdapter {
public:
	/**
	 * @brief Constructs a UCI adapter for the given engine executable.
	 * @param enginePath Path to the engine executable.
	 * @param workingDirectory Optional working directory for the engine.
	 * @param identifier Unique identifier for this engine instance.
	 */
    explicit UciAdapter(const std::filesystem::path& enginePath,
        const std::optional<std::filesystem::path>& workingDirectory,
        const std::string& identifier);
    ~UciAdapter() override;

    /**
     * @brief Starts the engine protokoll.
     */
    void startProtocol() override;

    /**
     * UCI requires an explicit "uciok" response from the engine to confirm
     * that the protocol has been correctly initialized.
     *
     * If this is missing, the engine is considered non-compliant and startup must fail.
     *
     * @return true — UCI requires ProtocolOk.
     */
    bool isProtocolOkRequired() const override {
        return true;
    }

    /**
     * Attempts to gracefully terminate the UCI engine. If the engine is already
     * terminated or unreachable, this is treated as a normal condition.
     * If forced termination fails, the adapter reports a critical error.
     */
    void terminateEngine() override;

    /**
     * @brief Is called after a moveNow command with wait=true. Runs handshake steps if needed.
     * @returns The event to wait for completing the handshake.
     */
    EngineEvent::Type waitAfterMoveNowHandshake() override {
        return EngineEvent::Type::BestMove;
    }

    /**
     * @brief Handles a ponder miss by sending 'stop' and waiting for bestmove.
     */
    EngineEvent::Type handlePonderMiss() override {
        moveNow();
        return EngineEvent::Type::BestMove;
    }

    EngineEvent readEvent() override;

    void newGame(const GameRecord& gameRecord, bool engineIsWhite) override;
    void setTimeControl(
        [[maybe_unused]] const GameRecord& gameRecord, 
        [[maybe_unused]] bool engineIsWhite) override {
        // Nothing to do for UCI, time control is sent with 'go' command
    }
    void moveNow() override;
    void setPonder(bool enabled) override;
    void ticker() override;

    uint64_t allowPonder(const GameStruct& game, const GoLimits& limits, std::string ponderMove) override;
    uint64_t computeMove(const GameStruct& game, const GoLimits& limits, bool ponderHit) override;

    /**
     * @brief Sends a are you ready command to the engine.
     */
    void askForReady() override;

    /**
     * @brief Sends a UCI 'setoption' command to the engine with the given name and value.
     *
     * This method does not validate option names or values. It is intended for testing
     * purposes, including sending intentionally invalid options.
     *
     * @param name The name of the UCI option to set.
     * @param value The value to assign to the option. May be empty.
     */
    void setTestOption(const std::string& name, const std::string& value = {}) override;

    /**
     * @brief Sets the engine's options based on the provided OptionValues.
     * 
	 * This method validates the options against the engine's supported options and
	 * only sets those that fulfills the engine requirements.
     *
     * @param optionValues The option values to set.
     */
    void setOptionValues(const OptionValues& optionValues) override;

private:
     /**
      * Parses a line received during the UCI handshake phase.
      * This function should be called only when waiting for the UCI handshake.
      * It handles 'id', 'option', and 'uciok' lines specifically.
      * Any unexpected input is reported as a protocol error.
      *
      * @param engineLine The full engine output line with timestamp and completeness status.
      * @return Parsed EngineEvent for handshake processing.
      */
    EngineEvent readUciEvent(const EngineLine& engineLine);

    static constexpr std::chrono::milliseconds engineIntroScanDuration{ 50 };
    static constexpr std::chrono::milliseconds uciHandshakeTimeout{ 3000 };
    static constexpr std::chrono::milliseconds engineQuitTimeout{ 10000 };
    static constexpr std::chrono::milliseconds readTimeout{ 1000 };

    struct ProtocolError {
        std::string context;
        std::string message;
    };
	std::vector<ProtocolError> protocolErrors_; // Stores protocol errors

    /**
     * @brief Compute the time setting options required for the go command
     */
	static std::string computeGoOptions(const GoLimits& limits);

	/**
	 * @brief Sends the current position to the engine.
	 * @param game The current game structure containing the position and moves played.
	 * @param ponderMove Optional move to ponder on, if any.
	 */
    void sendPosition(const GameStruct& game, const std::string& ponderMove = "");   

    EngineEvent parseSearchInfo(std::istringstream& iss, uint64_t timestamp, const std::string& originalLine);

	static inline int numOptionError_ = 0; 
    static inline int numIdError_ = 0;
    static inline int numNameError_ = 0;
	static inline int numUnknownCommandError_ = 0;
    bool inUciHandshake_ = false;

};

} // namespace QaplaTester
