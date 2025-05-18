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

#include "engine-worker.h"
#include "time-control.h"
#include "game-state.h"

class PlayerContext {
public:
    PlayerContext() = default;

    /**
     * @brief Sets the time control for this player.
     *
     * @param timeControl The time control to apply.
     */
    void setTimeControl(const TimeControl& timeControl) {
        timeControl_ = timeControl;
    }

    /**
     * @brief Sets the engine worker for this player.
     *
     * @param engineWorker Shared pointer to the EngineWorker.
     */
    void setEngine(std::shared_ptr<EngineWorker> engineWorker, bool requireLan) {
        engine_ = std::move(engineWorker);
		requireLan_ = requireLan;
    }

    /**
     * @brief Returns a raw pointer to the EngineWorker instance.
     *
     * @return Pointer to the EngineWorker.
     */
    EngineWorker* getEngine() {
        return engine_.get();
    }

    /**
     * @brief Sets the timestamp when the engine started computing a move.
     *
     * @param timestamp Milliseconds since epoch.
     */
    void setComputeMoveStartTimestamp(int64_t timestamp) {
        computeMoveStartTimestamp_ = timestamp;
    }

    /**
     * @brief Gets the timestamp when the engine started computing a move.
     *
     * @return Timestamp in milliseconds since epoch.
     */
    int64_t getComputeMoveStartTimestamp() const {
        return computeMoveStartTimestamp_;
    }

    /**
     * @brief Handles an info event from the engine.
     *
     * @param event The EngineEvent containing the information.
     */
    void handleInfo(const EngineEvent& event);

    /**
     * @brief Handles a best move event from the engine.
     *
     * @param event The EngineEvent containing the best move.
     */
    void handleBestMove(const EngineEvent& event);

    /**
     * @brief Evaluates whether the engine respected the time constraints.
     *
     * This function checks if the engine respected active time constraints like movetime,
     * remaining time, and increment. It also verifies that the engine used time appropriately
     * even if no strict constraints were set.
     *
     * @param event The EngineEvent containing timing information.
     */
    void checkTime(const EngineEvent& event);

    /**
     * @brief Sets the move text and updates game state.
     *
     * @param moveText The move in algebraic notation.
     */
    void setMove(const std::string& moveText);

	const TimeControl& getTimeControl() const {
		return timeControl_;
	}

private:
    /**
     * @brief Checks if a move is legal in the current game state.
     *
     * @param moveText The move in algebraic notation.
     * @return true if the move is legal, false otherwise.
     */
    bool isLegalMove(const std::string& moveText);

    std::shared_ptr<EngineWorker> engine_;
    TimeControl timeControl_;
    GameState gameState_;
    int64_t computeMoveStartTimestamp_ = 0;
    GoLimits goLimits_;
    bool requireLan_;
    MoveRecord currentMove_;
};
