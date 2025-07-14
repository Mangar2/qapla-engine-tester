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

#include "engine-report.h"
#include "engine-worker.h"
#include "timer.h"
#include "time-control.h"
#include "game-state.h"
#include "move-record.h"
#include "game-record.h"
#include "test-tournament.h"
#include "player-context.h"

 /**
  * @brief Manages a single chess game between the application and an engine.
  *        Controls the engine's lifecycle and reacts to engine events via FSM logic.
  */
class GameManager {
public:
    struct ExtendedTask {
        GameTask task;
        GameTaskProvider* provider = nullptr;
        std::unique_ptr<EngineWorker> white;
        std::unique_ptr<EngineWorker> black;
    };

public:
	GameManager();
	~GameManager();

    /**
     * @brief sets a new engine to play both sides
	 * @param engine The new engine to be set.
     */
    void initUniqueEngine(std::unique_ptr<EngineWorker> engine);

    /**
	 * @brief sets two engines to play against each other
	 * @param white The engine to play as white.
	 * @param black The engine to play as black.
     */
    void initEngines(std::unique_ptr<EngineWorker> white, std::unique_ptr<EngineWorker> black);



    /**
	 * Sends a new game command to the engine(s).
     */
    void notifyNewGame() {
        whitePlayer_->notifyNewGame();
		if (blackPlayer_ != whitePlayer_) {
			blackPlayer_->notifyNewGame();
		}
    }

	/**
	 * @brief Sets the same time control for both sides.
	 *
	 * @param timeControl The time control to be set.
	 */
	void setUniqueTimeControl(const TimeControl& timeControl) {
		whitePlayer_->setTimeControl(timeControl);
        if (blackPlayer_ != whitePlayer_) {
            blackPlayer_->setTimeControl(timeControl);
        }
	}

    /**
	 * @brief Sets the time control for both sides.
     */
	void setTimeControls(const TimeControl& white, const TimeControl& black) {
		assert(whitePlayer_ != nullptr && blackPlayer_ != nullptr);
        assert(whitePlayer_ != blackPlayer_ || &white != &black);
		whitePlayer_->setTimeControl(white);
		blackPlayer_->setTimeControl(black);
	}

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
    void computeTasks(GameTaskProvider* taskProvider = nullptr);

    /**
     * @brief Set the Trace level for the engine's CLI output.    
     * 
     * @param traceLevel 
     */
    void setCliTraceLevel(TraceLevel traceLevel) {
        forEachUniqueEngine([traceLevel](EngineWorker& engine) {
            engine.setTraceLevel(traceLevel);
        });
    }

    /**
     * @brief Returns a reference to the EngineWorker instance.
     *
     * @return A reference to the EngineWorker.
     */
    EngineWorker* getEngine(bool white = true) {
        return white ? whitePlayer_->getEngine() : blackPlayer_->getEngine();
    }

	/**
	 * @brief Returns the task provider used by this GameManager.
	 *
	 * @return A reference to the used GameTaskProvider.
	 */
    const GameTaskProvider* getTaskProvider() {
		return taskProvider_;
    }
    /**
     * @brief stops the engine if it is running.
     */
    void stop();
private:


    /**
     * Adds a new engine event to the processing queue.
     * This method is thread-safe and does not block.
     */
    void enqueueEvent(const EngineEvent& event);
    /**
     * Continuously processes events from the queue and performs periodic tasks.
     * Intended to run in a dedicated thread.
     */
    void processQueue();

    /**
     * Retrieves the next event from the queue and processes it.
     * Returns true if an event was processed, false if the queue was empty.
     */
    bool processNextEvent();

    /**
     * Processes a single engine event by applying the appropriate state transition logic.
     * Called exclusively by the internal processing thread.
     *
     * @param event The engine event to process.
     */
    void processEvent(const EngineEvent& event);

	void handleBestMove(const EngineEvent& event);
	/**
	 * Informs the task provider about the event, allowing it to react to engine information.
	 * This is called for events of type EngineEvent::Type::Info.
	 *
	 * @param event The engine event containing information to be processed.
	 */
    void informTask(const EngineEvent& event, const PlayerContext* player);

    /**
     * @brief Switches the engines side to play
     */
    void switchSide();

    void computeNextMove(const std::optional<EngineEvent>& event = std::nullopt);

    /**
	 * @brief template executing a function for both white and black engine, if they are not identical
     */
    template<typename Func>
    void forEachUniqueEngine(Func&& func) {
		auto whiteEngine = whitePlayer_->getEngine();
		auto blackEngine = blackPlayer_->getEngine();
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
    void setFromGameRecord(const GameRecord& game);

    /**
     * @brief Initiates a new game, setting the FEN string for both players and informing the gameRecord.
     *
     * @param useStartPosition If true, the game starts from the initial position.
     * @param fen The FEN string representing the game state.
     */
    void setFromFen(bool useStartPosition, const std::string& fen);

	/**
	 * @brief Checks if the game has ended.
	 *
	 * This function checks if the game has ended and handles the end of the game.
	 * It returns true if the game has ended, false otherwise.
	 */
    bool checkForGameEnd();
    std::tuple<GameEndCause, GameResult> getGameResult();

    /**
     * @brief Signals that a computation has completed. Call once per compute cycle.
     */
    void markFinished();
    void markRunning();

    /**
     * Players
     */
    PlayerContext* whitePlayer_;
    PlayerContext* blackPlayer_;
    PlayerContext player1_;
    PlayerContext player2_;
	bool switchedSide_ = false;

    std::promise<void> finishedPromise_;
    std::future<void> finishedFuture_;

    /**
     * Callback to get new tasks
     */
    GameTaskProvider* taskProvider_ = nullptr;

    /**
	 * Computes the next task from the task provider
     */
	void computeNextTask();

    /**
     * @brief Attempts to obtain a replacement task and reassign the GameManager.
     *
     * If a new task is available via the GameManagerPool, this method updates the
     * GameManager's task provider and engine assignments accordingly. Returns the
     * new task if successful.
     *
     * @return An optional GameTask if reassignment was possible; std::nullopt otherwise.
     */
    std::optional<GameTask> tryGetReplacementTask();

    /**
	 * @brief Attempts to organize a new assignment by fetching the next task from the task provider or 
	 * from the GameManagerPool, if the task proivder has no more tasks available.
	 * @return the new GameTask or std::nullopt if no more tasks are available.
     */
    std::optional<GameTask> organizeNewAssignment();

	/**
	 * @brief Computes the task based on the provided GameTask.
	 * If the task is std::nullopt, it marks the game as finished.
	 *
	 * @param task The GameTask to compute.
	 */
    void computeTask(std::optional<GameTask> task);

    /**
     * @brief True if finishedPromise_ is valid and has not yet been set.
     */
    bool finishedPromiseValid_ = false;

    bool requireLan_ = true;
	std::atomic<GameTask::Type> taskType_ = GameTask::Type::None;
    std::string taskId_;

    GameRecord gameRecord_;
    bool logMoves_ = false;

    // Queue management
    std::thread eventThread_;
    std::atomic<bool> stopThread_{ false };
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::queue<EngineEvent> eventQueue_;
};
