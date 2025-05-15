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

    EngineEvent readEvent() override;

    void newGame() override;
    void moveNow() override;
    void setPonder(bool enabled) override;
    void ticker() override;

    void ponder(const GameRecord& game, GoLimits& limits) override;
    int64_t computeMove(const GameRecord& game, const GoLimits& limits) override;

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
    void setOption(const std::string& name, const std::string& value = {}) override;

private:
    static constexpr std::chrono::milliseconds engineIntroScanDuration{ 50 };
    static constexpr std::chrono::milliseconds uciHandshakeTimeout{ 3000 };
    static constexpr std::chrono::milliseconds engineQuitTimeout{ 10000 };
    static constexpr std::chrono::milliseconds readTimeout{ 1000 };

    struct ProtocolError {
        std::string context;
        std::string message;
    };
	std::vector<ProtocolError> protocolErrors_; // Stores protocol errors


    void sendPosition(const GameRecord& game);   // Sends position + moves
	void runUciHandshake();                     // Runs the UCI handshake

    void reportProtocolError(std::string_view context, std::string_view message) {
        protocolErrors_.emplace_back(std::string(context), std::string(message));
		std::cerr << "Protocol error in " << context << ": " << message << std::endl;
    }

    EngineEvent parseSearchInfo(const std::string& line, int64_t timestamp);

};
