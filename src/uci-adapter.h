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
#include "engine-process.h"
#include "uci-option.h"

using UciOptions = std::unordered_map<std::string, UciOption>;

enum class UciState {
    Uninitialized,   
	Initialized,     // After uciok
	Ready,           // After isready
    Terminating      // Quitting
};

 /**
  * @brief UCI protocol adapter implementing EngineAdapter.
  *        Runs the engine in a dedicated thread, handles UCI I/O.
  */
class UciAdapter : public EngineAdapter {
public:
    explicit UciAdapter(std::filesystem::path enginePath,
        const std::optional<std::filesystem::path>& workingDirectory = std::nullopt);
    ~UciAdapter();

    void runEngine() override;

    /**
     * Attempts to gracefully terminate the UCI engine. If the engine is already
     * terminated or unreachable, this is treated as a normal condition.
     * If forced termination fails, the adapter reports a critical error.
     */
    void terminateEngine() override;

    void newGame(const GameStartPosition& position) override;
    void moveNow() override;
    void setPonder(bool enabled) override;
    void ticker() override;

    void ponder(const GameState& game, GoLimits& limits) override;
    void calcMove(const GameState& game, GoLimits& limits,
        const MoveList& limitMoves = {}) override;

    void stopCalc() override;
    void writeCommand(const std::string& command) override;

    const OptionMap& getOptionMap() const override;
    void setOptionMap(const OptionMap& list) override;

private:
    static constexpr std::chrono::milliseconds engineIntroScanDuration{ 50 };
    static constexpr std::chrono::milliseconds uciHandshakeTimeout{ 500 };
    static constexpr std::chrono::milliseconds engineQuitTimeout{ 1000 };

    struct ProtocolError {
        std::string context;
        std::string message;
    };
	std::vector<ProtocolError> protocolErrors_; // Stores protocol errors

    void waitForReady();                        // Sends "isready" and waits for "readyok"
    void sendPosition(const GameState& game);   // Sends position + moves
	void runUciHandshake();                     // Runs the UCI handshake
    void skipLines(std::chrono::milliseconds timeout);

    void reportProtocolError(std::string_view context, std::string_view message) {
        protocolErrors_.emplace_back(std::string(context), std::string(message));
		std::cerr << "Protocol error in " << context << ": " << message << std::endl;
    }

	void logEngineOutput(std::string_view message) {
		std::cout << "[] -> " << message << std::endl;
	}

    EngineProcess process_;

	UciOptions supportedOptions_;
    OptionMap options_;
    std::mutex commandMutex_;

	std::string engineName_;
	std::string engineAuthor_;

    std::atomic<UciState> state_ = UciState::Uninitialized;

};
