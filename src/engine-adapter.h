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

#include "game-start-position.h"
#include "game-state.h"

using OptionMap = std::unordered_map<std::string, std::string>;

/**
 * @brief UCI-style limits for calculating a single move.
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

    bool infinite = false;
};

 /**
  * @brief Abstract interface for communicating with and controlling a chess engine,
  *        independent of the underlying protocol (e.g. UCI, XBoard).
  */
class EngineAdapter {
public:
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
     * @brief Prepares the engine for a new game.
     * @param info Game-specific initialization parameters.
     */
    virtual void newGame(const GameStartPosition& position) = 0;

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
     */
    virtual void calcMove(const GameState& game, GoLimits& limits,
        const MoveList& limitMoves = {}) = 0;

    /**
     * @brief Instructs the engine to stop calculation.
     */
    virtual void stopCalc() = 0;

    /**
     * @brief Sends a raw string command to the engine.
     */
    virtual void writeCommand(const std::string& command) = 0;

    /**
     * @brief Returns the current engine option list.
     */
    virtual const OptionMap& getOptionMap() const = 0;

    /**
     * @brief Applies a new engine option list.
     */
    virtual void setOptionMap(const OptionMap& list) = 0;

};
