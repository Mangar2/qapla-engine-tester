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

#include <memory>
#include <future>
#include <mutex>

#include "engine-worker.h"
#include "timer.h"
#include "time-control.h"
#include "game-state.h"

 /**
  * @brief Manages a single chess game between the application and an engine.
  *        Controls the engine's lifecycle and reacts to engine events via FSM logic.
  */
class GameManager {
public:
    /**
     * @brief Constructs a GameManager with a ready-to-use EngineWorker instance.
     */
    GameManager(std::unique_ptr<EngineWorker> engine);

	/**
	 * @brief stops the engine if it is running.
	 */
	void stop() {
		if (engine_ ) {
			engine_->stop();
		}
	}

    /**
     * @brief Starts the game by sending a compute command to the engine.
     */
    void run();

    /**
     * @brief Returns a future that becomes ready when the game is complete.
     */
    std::future<void> getFinishedFuture() {
        return std::move(finishedFuture_);
    }

    /**
     * Initiates asynchronous move computation using the current time control.
     *
     * @param game The game state to compute the move for.
     */
    void computeMove();

	/**
	 * @brief Initiates the test run for the engine.
	 */
    void runTests();

private:
    void handleState(const EngineEvent& event);

	void evaluateMovetime(const EngineEvent& event);

    std::unique_ptr<EngineWorker> engine_;
    std::promise<void> finishedPromise_;
    std::future<void> finishedFuture_ = finishedPromise_.get_future();

    Timer timer_;
	TimeControl timeControl_;
	GameState gameState_;
};
