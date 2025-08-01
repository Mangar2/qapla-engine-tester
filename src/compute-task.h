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
#include <string>
#include <optional>
#include <vector>
#include <future>

#include "engine-worker.h"
#include "engine-event.h"
#include "game-context.h"

 /**
  * @brief Executes a single computation task such as move calculation, game play, or position analysis.
  *        Not intended to manage or queue multiple tasks.
  */
class ComputeTask {
public:
    ComputeTask();
    ~ComputeTask();

    /**
     * @brief Initializes engines
     * @param engines A vector of unique pointers to EngineWorker instances.
     */
    void initEngines(std::vector<std::unique_ptr<EngineWorker>> engines) {
        gameContext_.initPlayers(std::move(engines));
    }

    /**
	 * @brief Gets the amount of engines currently managed by this manager.
     */
    size_t getEngineCount() const {
		return gameContext_.getPlayerCount();
    }

    /**
     * @brief Returns the player at the specified index.
     * @param index The index of the player to return.
     * @return Pointer to the PlayerContext at the given index, or nullptr if no players exist.
     */
    EngineWorker* getEngine(uint32_t index = 0) {
        return gameContext_.getEngine(index);
    }

    /**
	 * @brief Restarts the engine at the specified index.
	 * @param index The index of the engine to restart.
	 */
    void restartEngine(uint32_t index) {
		gameContext_.restartPlayer(index);
    }

    /**
	 * @brief Sets the time controls for the players.
     * @param timeControl The time control.
     */
    void setTimeControl(const TimeControl& timeControl) {
        gameContext_.setTimeControl(timeControl);
	}

    /**
     * @brief Sets the time controls for each player.
     * @param timeControls A vector of TimeControl objects for each player.
	 */
	void setTimeControls(const std::vector<TimeControl>& timeControls) {
		gameContext_.setTimeControls(timeControls);
	}

    /**
     * @brief Notifies all engines that a new game starts and resets their internal state.
     */
    void newGame() {
		gameContext_.newGame();
    }

    /**
     * @brief Sets a position
     * @param useStartPosition If true, uses the standard starting position.
     * @param fen Optional FEN string.
     * @param playedMoves Optional list of moves already played.
     */
    void setPosition(bool useStartPosition, const std::string& fen = "",
        std::optional<std::vector<std::string>> playedMoves = std::nullopt) {
		gameContext_.setPosition(useStartPosition, fen, playedMoves);
    }

    /**
	 * @brief computes a single move for the current position.
     */
    void computeMove();

    /**
     * @brief Starts a game continuation until the end.
     */
    void autoPlay(bool logMoves = false);

    /**
     * @brief Forces the engine to return the best move immediately.
     */
    void moveNow();

    /**
     * @brief Returns a future that signals when the task is complete.
     */
    const std::future<void>& getFinishedFuture() const;

    /**
     * @brief Stops any ongoing computation immediately.
     */
    void stop();

private:
    GameContext gameContext_;

    /**
     * @brief Defines the currently active automated task mode.
     */
    enum class ComputeTaskType {
        None,
        Autoplay
    };
    ComputeTaskType taskType_ = ComputeTaskType::None;

    /**
     * @brief Continues the current automatic task after a best move, if applicable.
     */
    void nextMove(const EngineEvent& event);
    void autoPlay(const std::optional<EngineEvent>& event);


    void enqueueEvent(const EngineEvent& event);
    void processQueue();
    void processEvent(const EngineEvent& event);
    void handleBestMove(const EngineEvent& event);

    bool checkGameOver(bool verbose = false);


    void markFinished();
    void markRunning();
    std::promise<void> finishedPromise_;
    std::future<void> finishedFuture_;
    bool finishedPromiseValid_ = false;

    bool logMoves_ = false;

    std::thread eventThread_;
    std::atomic<bool> stopThread_{ false };
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::queue<EngineEvent> eventQueue_;
};
