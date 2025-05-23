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

#include "checklist.h"
#include "engine-worker.h"
#include "timer.h"
#include "time-control.h"
#include "game-state.h"
#include "move-record.h"
#include "game-record.h"
#include "tournament-manager.h"
#include "player-context.h"

 /**
  * @brief Manages a single chess game between the application and an engine.
  *        Controls the engine's lifecycle and reacts to engine events via FSM logic.
  */
class GameManager {
public:
	GameManager();

    /**
     * @brief sets a new engine to play both sides
	 * @param engine The new engine to be set.
     */
    void setUniqueEngine(std::shared_ptr<EngineWorker> engine);

    /**
	 * @brief sets two engines to play against each other
	 * @param white The engine to play as white.
	 * @param black The engine to play as black.
     */
    void setEngines(std::shared_ptr<EngineWorker> white, std::shared_ptr<EngineWorker> black);

    /**
     * @brief Switches the engines side to play
     */
    void switchSide();

	/**
	 * @brief stops the engine if it is running.
	 */
	void stop() {
		forEachUniqueEngine([](EngineWorker& engine) {
			engine.stop();
			});
	}

    /**
	 * Sends a new game command to the engine(s).
     */
    void newGame() {
		forEachUniqueEngine([](EngineWorker& engine) {
			engine.newGame();
			});
    }

	/**
	 * @brief Sets the same time control for both sides.
	 *
	 * @param timeControl The time control to be set.
	 */
	void setUniqueTimeControl(const TimeControl& timeControl) {
		whitePlayer_.setTimeControl(timeControl);
		blackPlayer_.setTimeControl(timeControl);
	}

    /**
	 * @brief Sets the time control for both sides.
     */
	void setTimeControls(const TimeControl& white, const TimeControl& black) {
		whitePlayer_.setTimeControl(white);
		blackPlayer_.setTimeControl(black);
	}

    /**
     * @brief Starts the game by sending a compute command to the engine.
     */
    void run();

    /**
     * @brief Returns a future that becomes ready when the game is complete.
     */
    const std::future<void>& getFinishedFuture() const {
        return finishedFuture_;
    }

    /**
     * Initiates asynchronous move computation using the current time control.
     *
	 * @param useStartPosition If true, the game starts from the initial position.
	 * @param fen The FEN string representing the game state.
     */
    void computeMove(bool useStartPosition, const std::string fen = "");

	/**
	 * @brief Tells the engine to stop the current move calculation and sends the best move
	 */
    void moveNow();

	/**
	 * Initiates asynchronous game computation using the current time control.
	 *
	 * @param useStartPosition If true, the game starts from the initial position.
	 * @param fen The FEN string representing the game state.
     * @param logMoves true, then moves will be logged
	 */
    void computeGame(bool useStartPosition, const std::string fen = "", bool logMoves = false);

    /**
     * @brief Starts and manages multiple consecutive tasks such as games or compute move using a task callback.
     *
     * Each task is initiated asynchronously after the previous one finishes. The taskProvider
     * callback must return a valid GameTask or std::nullopt to signal completion.
     *
     * @param taskProvider Function that returns the next Task or std::nullopt if done.
     */
    void computeTasks(GameTaskProvider* taskProvider);

    /**
     * @brief Returns a reference to the EngineWorker instance.
     *
     * @return A reference to the EngineWorker.
     */
    EngineWorker* getEngine(bool white = true) {
        return white ? whitePlayer_.getEngine() : blackPlayer_.getEngine();
    }
private:

    void handleState(const EngineEvent& event);
	void handleBestMove(const EngineEvent& event);

    void computeNextMove();

    /**
	 * @brief template executing a function for both white and black engine, if they are not identical
     */
    template<typename Func>
    void forEachUniqueEngine(Func&& func) {
		auto whiteEngine = whitePlayer_.getEngine();
		auto blackEngine = blackPlayer_.getEngine();
        if (whiteEngine) {
            func(*whiteEngine);
        }
        if (blackEngine && blackEngine != whiteEngine) {
            func(*blackEngine);
        }
    }

	/**
	 * @brief Initiates a new game, setting the FEN string for both players and informing the gameRecord.
	 *
	 * @param useStartPosition If true, the game starts from the initial position.
	 * @param fen The FEN string representing the game state.
	 */
	void newGame(bool useStartPosition, const std::string& fen) {
        whitePlayer_.setFen(useStartPosition, fen);
		if (whitePlayer_.getEngine() != blackPlayer_.getEngine()) {
			blackPlayer_.setFen(useStartPosition, fen);
		}
        gameRecord_.newGame(useStartPosition, fen, whitePlayer_.isWhiteToMove());
	}

	/**
	 * @brief Checks if the game has ended.
	 *
	 * This function checks if the game has ended and handles the end of the game.
	 * It returns true if the game has ended, false otherwise.
	 */
    bool checkForGameEnd();

    /**
     * @brief Signals that a computation has completed. Call once per compute cycle.
     */
    void markFinished();

    PlayerContext whitePlayer_;
    PlayerContext blackPlayer_;

    std::promise<void> finishedPromise_;
    std::future<void> finishedFuture_;

    /**
     * Callback to get new tasks
     */
    GameTaskProvider* taskProvider_;
    /**
	 * Computes the next task from the task provider
     */
	void computeNextTask();

    /**
     * @brief True if finishedPromise_ is valid and has not yet been set.
     */
    bool finishedPromiseValid_ = false;

    bool requireLan_ = true;
	GameTask::Type taskType_ = GameTask::Type::None;
	GameRecord gameRecord_;
    bool logMoves_ = false;

	// Mutex to protect access to the event queue and state
    std::mutex eventMutex_;
};
