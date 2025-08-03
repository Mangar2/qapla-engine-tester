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
#include <map>

#include "engine-adapter.h"
#include "uci-option.h"
#include "game-record.h"

 /**
  * @brief Winboard protocol adapter implementing EngineAdapter.
  *        Runs the engine in a dedicated thread, handles Winboard I/O.
  */
class WinboardAdapter : public EngineAdapter {
public:
    /**
     * @brief Constructs a Winboard adapter for the given engine executable.
     * @param enginePath Path to the engine executable.
     * @param workingDirectory Optional working directory for the engine.
     * @param identifier Unique identifier for this engine instance.
     */
    explicit WinboardAdapter(std::filesystem::path enginePath,
        const std::optional<std::filesystem::path>& workingDirectory,
        const std::string& identifier);
    ~WinboardAdapter() override;

    /**
     * @brief Starts the engine protocol.
     */
    void startProtocol() override;

    /**
     * Winboard engines may omit "feature done=1", especially older or non-compliant ones.
     * In such cases, the EngineWorker completes the startup based on a timeout.
     *
     * Therefore, the presence of ProtocolOk is optional for Winboard.
     *
     * @return false — Winboard does not require ProtocolOk.
     */
    bool isProtocolOkRequired() const override {
        return false;
    }

    /**
     * Attempts to gracefully terminate the Winboard engine. If the engine is already
     * terminated or unreachable, this is treated as a normal condition.
     * If forced termination fails, the adapter reports a critical error.
     */
    void terminateEngine() override;

    EngineEvent readEvent() override;

    void newGame(const GameRecord& gameRecord, bool engineIsWhite) override;
    void moveNow() override;
    void setPonder(bool enabled) override;
    void ticker() override;

    uint64_t allowPonder(const GameRecord& game, const GoLimits& limits, std::string ponderMove) override;
    uint64_t computeMove(const GameRecord& game, const GoLimits& limits, bool ponderHit) override;

    /**
     * @brief Sends a are you ready command to the engine.
     */
    void askForReady() override;

    /**
     * @brief Sends a Winboard 'setoption' command to the engine with the given name and value.
     *
     * This method does not validate option names or values. It is intended for testing
     * purposes, including sending intentionally invalid options.
     *
     * @param name The name of the Winboard option to set.
     * @param value The value to assign to the option. May be empty.
     */
    void setTestOption(const std::string& name, const std::string& value = {}) override;

    /**
     * @brief Sets the engine's options based on the provided OptionValues.
     *
     * This method validates the options against the engine's supported options and
     * only sets those that fulfill the engine requirements.
     *
     * @param optionValues The option values to set.
     */
    void setOptionValues(const OptionValues& optionValues) override;

private:
    /**
     * Parses a line received during the Winboard handshake phase.
     * This function should be called only when waiting for the Winboard handshake.
     * It handles 'feature', 'xboard', and readiness lines specifically.
     * Any unexpected input is reported as a protocol error.
     *
     * @param engineLine The full engine output line with timestamp and completeness status.
     * @return Parsed EngineEvent for handshake processing.
     */
    EngineEvent readFeatureSection(const EngineLine& engineLine);

    static constexpr std::chrono::milliseconds engineIntroScanDuration{ 50 };
    static constexpr std::chrono::milliseconds winboardHandshakeTimeout{ 3000 };
    static constexpr std::chrono::milliseconds engineQuitTimeout{ 10000 };
    static constexpr std::chrono::milliseconds readTimeout{ 1000 };

    struct ProtocolError {
        std::string context;
        std::string message;
    };
    std::vector<ProtocolError> protocolErrors_;

    /**
     * @brief Sends time control to the engine according to Winboard protocol.
     *        Supports asymmetric time controls and all xboard-compliant formats:
     *        - level (classical, increment)
     *        - st (movetime)
     *        - sd (depth)
     *        - nps (nodes-per-second)
     *
     * @param gameRecord GameRecord with time control settings.
     * @param engineIsWhite True if engine is playing white.
     */
    void sendTimeControl(const GameRecord& gameRecord, bool engineIsWhite);

    /**
     * @brief Sends the current position to the engine.
     * @param game The current game record containing the position and moves played.
     */
    void sendPosition(const GameRecord& game);

    /**
     * @brief Sends only the new opponent moves from the given GameRecord compared to the last known state.
     *        Skips the last own move if it matches the stored original move string.
     *
     * @param game The current game state with full move history.
     */
    uint64_t catchupMovesAndGo(const GameRecord& game);

    /**
     * Ensures all known boolean features are present in featureMap_ with correct defaults.
     * Should be called once after parsing all incoming 'feature' lines from GUI.
     */
    void finalizeFeatures();

    bool isEnabled(const std::string& key) const {
        auto it = featureMap_.find(key);
        return it != featureMap_.end() && it->second == "1";
    }

    EngineEvent parseSearchInfo(std::string depthStr, std::istringstream& iss, uint64_t timestamp, const std::string& rawLine);
	EngineEvent parseFeatureLine(std::istringstream& iss, uint64_t timestamp, bool onlyOption);
    void parseOptionFeature(const std::string& optionStr, EngineEvent& event);
    EngineEvent parseResult(std::istringstream& iss, const std::string& command, EngineEvent event);
    

    static inline int numOptionError_ = 0;
    static inline int numFeatureError_ = 0;
    static inline int numNameError_ = 0;
    static inline int numUnknownCommandError_ = 0;
    bool inFeatureSection_ = false;
    std::map<std::string, std::string> featureMap_;
	uint64_t pingCounter_ = 0;
    bool forceMode_ = false;

	GameRecord gameRecord_; 
};

