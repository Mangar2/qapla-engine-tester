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


namespace QaplaTester {

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
    explicit WinboardAdapter(const std::filesystem::path& enginePath,
        const std::optional<std::filesystem::path>& workingDirectory,
        const std::string& identifier);
    /**
     * @brief Destructor for WinboardAdapter.
     */
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
    /**
     * @brief Attempts to gracefully terminate the Winboard engine. If the engine is already
     * terminated or unreachable, this is treated as a normal condition.
     * If forced termination fails, the adapter reports a critical error.
     */
    void terminateEngine() override;

    /**
     * @brief Reads the next event from the engine.
     * @return The next EngineEvent from the engine.
     */
    EngineEvent readEvent() override;

    /**
     * @brief Is called after a moveNow command with wait=true. Runs handshake steps if needed.
     * @returns The event to wait for completing the handshake.
     */
    EngineEvent::Type waitAfterMoveNowHandshake() override;

    /**
     * @brief Handles a ponder miss - XBoard engines don't send bestmove when pondering is stopped.
     */
    EngineEvent::Type handlePonderMiss() override;

    /**
     * @brief Starts a new game with the given parameters.
     * 
     * @param gameRecord Current game state.
     * @param engineIsWhite True if the engine plays as white, false for black.
     */
    void newGame(const GameRecord& gameRecord, bool engineIsWhite) override;
    
    /**
     * @brief Sets the time control for the engine. 
     * 
     * This will start a new game and then send the time control according to Winboard protocol.
     * Supports asymmetric time controls and all xboard-compliant formats:
     *  - level (classical, increment)
     *  - st (movetime)
     *  - sd (depth)
     *  - nps (nodes-per-second)
     * 
     * @param game Current game state.
	 * @param engineIsWhite True if the engine plays as white, false for black.
     */
    void setTimeControl(const GameRecord& game, bool engineIsWhite) override;

    /**
     * @brief Notifies the adapter that a best move has been received from the engine.
     * @param sanMove The move in SAN notation.
     * @param lanMove The move in LAN notation.
     */
    void bestMoveReceived(const std::string& sanMove, const std::string& lanMove) override;

    /**
     * @brief Sends a move now command to the engine.
     */
    void moveNow() override;

    /**
     * @brief Enables or disables pondering for the engine.
     * @param enabled True to enable pondering, false to disable.
     */
    void setPonder(bool enabled) override;

    /**
     * @brief Periodic ticker function for engine management.
     */
    /**
     * @brief Periodic ticker function for engine management.
     */
    void ticker() override;

    /**
     * @brief Allows the engine to ponder on a move.
     * @param game The current game state.
     * @param limits The search limits.
     * @param ponderMove The move to ponder on.
     * @return Timestamp when pondering was initiated.
     */
    uint64_t allowPonder(const GameStruct& game, const GoLimits& limits, std::string ponderMove) override;

    /**
     * @brief Requests the engine to compute a move.
     * @param game The current game state.
     * @param limits The search limits.
     * @param ponderHit True if pondering hit, false otherwise.
     * @return Timestamp when the move computation was requested.
     */
    uint64_t computeMove(const GameStruct& game, const GoLimits& limits, bool ponderHit) override;

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
     * @brief Parses a line received during the Winboard handshake phase.
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
     * @brief Sends the current position to the engine.
     * @param game The current game structure containing the position and moves played.
     */
    void sendPosition(const GameStruct& game);

    /**
     * @brief Sends only the new opponent moves from the given GameRecord compared to the last known state.
     *        Skips the last own move if it matches the stored original move string.
     *
     * @param game The current game state with full move history.
     * @param isInfinite If true, enable analyze mode if not already enabled.
     * @return Timestamp when the 'go' or 'analyze' command was sent.
     */
    uint64_t catchupMovesAndGo(const GameStruct& game, bool isInfinite = false);

    /**
     * @brief Sets the time control for the engine.
     * 
     * This will send the time control according to Winboard protocol.
     * Supports asymmetric time controls and all xboard-compliant formats:
     *  - level (classical, increment)
     *  - st (movetime)
     *  - sd (depth)
     *  - nps (nodes-per-second)
     * 
     * @param timeControl The time control to set.
     */
    void setTimeControl(const TimeControl& timeControl);

    /**
     * @brief Sends a 'go' or 'analyze' command to the engine based on current mode.
     * @param isInfinite If true, sends 'analyze' command - if not in analyzeMode; otherwise, sends 'go'.
     * @return Timestamp when the command was sent or 0, if nothing has been sent.
     */
    uint64_t go(bool isInfinite);

    /**
     * @brief Sets the engine into force mode and remember this in forceMode_.
     */
    void setForceMode();

    /**
     * @brief Ensures all known boolean features are present in featureMap_ with correct defaults.
     * Should be called once after parsing all incoming 'feature' lines from GUI.
     */
    void finalizeFeatures();

    /**
     * @brief Checks if a feature is enabled.
     * @param key The feature key to check.
     * @return True if the feature is enabled, false otherwise.
     */
    /**
     * @brief Checks if a feature is enabled.
     * @param key The feature key to check.
     * @return True if the feature is enabled, false otherwise.
     */
    bool isEnabled(const std::string& key) const {
        auto it = featureMap_.find(key);
        return it != featureMap_.end() && it->second == "1";
    }

    /**
     * @brief Parses search information from engine output.
     * @param depthStr The depth string.
     * @param iss The input string stream.
     * @param timestamp The timestamp of the line.
     * @param originalLine The original line for error reporting.
     * @return The parsed EngineEvent.
     */
    EngineEvent parseSearchInfo(const std::string& depthStr, std::istringstream& iss, uint64_t timestamp, 
        const std::string& originalLine);

    /**
     * @brief Parses a feature line from the engine.
     * @param iss The input string stream.
     * @param timestamp The timestamp of the line.
     * @param onlyOption True if only option parsing is expected.
     * @return The parsed EngineEvent.
     */
    EngineEvent parseFeatureLine(std::istringstream& iss, uint64_t timestamp, bool onlyOption);

    /**
     * @brief Parses a comment or debug line from the engine.
     * @param engineLine The full engine output line with timestamp and completeness status.
     * @return The parsed EngineEvent.
     */
    EngineEvent parseCommentLine(const EngineLine& engineLine);

    /**
     * @brief Parses a move string from the engine.
     * @param iss The input string stream.
     * @param engineLine The full engine output line with timestamp and completeness status.
     * @return The parsed EngineEvent.
     */
    EngineEvent parseMove(std::istringstream& iss, const EngineLine& engineLine);

    /**
     * @brief Parses a hint command from the engine.
     * @param iss The input string stream.
     * @param engineLine The full engine output line with timestamp and completeness status.
     * @return The parsed EngineEvent.
     */
    EngineEvent parseHint(std::istringstream& iss, const EngineLine& engineLine);

    /**
     * @brief Parses an option feature from the feature line.
     * @param optionStr The option string.
     * @param event The EngineEvent to populate.
     */
    void parseOptionFeature(const std::string& optionStr, EngineEvent& event);

    /**
     * @brief Parses the result of a command.
     * @param iss The input string stream.
     * @param command The command that was sent.
     * @param event The EngineEvent to populate.
     * @return The parsed EngineEvent.
     */
    static EngineEvent parseResult(std::istringstream& iss, const std::string& command, EngineEvent event);

    /**
     * @brief Computes windows standard option strings.
     * @param supportedOption The supported engine option.
     * @param value The value to set.
     * @return The computed Winboard option command string.
     */
    std::string computeStandardOptions(const EngineOption& supportedOption, const std::string& value);

    /**
     * @brief Parses a command from the engine.
     * @param engineLine The full engine output line with timestamp and completeness status.
     * @return The parsed EngineEvent.
     */
    EngineEvent parseCommand(const EngineLine& engineLine);
    
    static inline int numOptionError_ = 0;
    static inline int numFeatureError_ = 0;
    static inline int numNameError_ = 0;
    static inline int numUnknownCommandError_ = 0;
    bool inFeatureSection_ = false;
    std::map<std::string, std::string> featureMap_;
	uint64_t pingCounter_ = 0;
    bool forceMode_ = false;
    bool isAnalyzeMode_ = false;

    std::string lastOwnMove_; ///> The last move made by the engine in original format.
    std::string clearTimeControlCommand_; ///> The command to clear the time control before setting a new one.

	GameStruct gameStruct_; 
};

} // namespace QaplaTester
