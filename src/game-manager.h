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

#include "engine-checklist.h"
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
	 * Sends a new game command to the engine(s).
     */
    void newGame() {
        engine_->newGame();
    }

	/**
	 * @brief Sets the time control for the game.
	 *
	 * @param timeControl The time control to be set.
	 */
	void setTime(const TimeControl& timeControl) {
		timeControl_ = timeControl;
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
    void computeMove(bool startPos, const std::string fen = "");


    /**
     * @brief Returns a reference to the EngineWorker instance.
     *
     * @return A reference to the EngineWorker.
     */
    EngineWorker* getEngine() {
       return engine_.get();
}
private:
	enum class Tasks {
        None,
		ComputeMove,
        PlayGame
	};
    void handleState(const EngineEvent& event);
	void handleBestMove(const EngineEvent& event);
	void handleInfo(const EngineEvent& event);

    bool isLegalMove(const std::string& moveText);

    void computeMove();

    /**
     * @brief Evaluates whether the engine respected the time constraints defined in GoLimits.
     *
     * This function considers all active time limits (e.g. movetime, wtime, btime, movesToGo)
     * and verifies that the engine stopped within the strictest constraint.
     * If no limits are set, it ensures that the engine used a non-trivial amount of time.
     *
     * @param event The EngineEvent containing the bestmove timestamp for timing analysis.
     */
    void checkTime(const EngineEvent& event);

    /**
     * @brief General check handling method.
	 * @param name Checklist-Name of the topic.
	 * @param detail Detailed error message to be logged
     */
    bool handleCheck(std::string_view name, bool success, std::string_view detail = "") {
        EngineChecklist::report(name, success);
        if (!success) {
            std::cerr << "Error: " << name << ": " << detail << std::endl;
        }
		return success;
    }

    /**
     * @brief Signals that a computation has completed. Call once per compute cycle.
     */
    void markFinished();

    std::unique_ptr<EngineWorker> engine_;
    std::promise<void> finishedPromise_;
    std::future<void> finishedFuture_;

    /**
     * @brief True if finishedPromise_ is valid and has not yet been set.
     */
    bool finishedPromiseValid_ = false;

    Timer timer_;
	TimeControl timeControl_;
	GameState gameState_;
	int64_t computeMoveStartTimestamp_ = 0;
    bool requireLan_ = true;
	Tasks task_ = Tasks::None;
};
