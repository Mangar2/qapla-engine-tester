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
#include "engine-event.h"
#include "game-context.h"

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <future>

namespace QaplaTester {

/**
 * @brief Executes a single computation task such as move calculation, game play, or position analysis.
 *        Not intended to manage or queue multiple tasks.
 */
class ComputeTask
{
public:
    ComputeTask();
    ~ComputeTask();

    enum class Status: std::uint8_t
    {
        Stopped,
        Play,
        Autoplay,
        Analyze
    };

    /**
     * @brief Initializes engines
     * @param engines A vector of unique pointers to EngineWorker instances.
     */
    void initEngines(std::vector<std::unique_ptr<EngineWorker>> engines)
    {
        stop();
        gameContext_.initPlayers(std::move(engines));
    }

    /**
     * @brief Gets the amount of engines currently managed by this manager.
     */
    size_t getEngineCount() const
    {
        return gameContext_.getPlayerCount();
    }

    /**
     * @brief Returns the player at the specified index.
     * @param index The index of the player to return.
     * @return Pointer to the PlayerContext at the given index, or nullptr if no players exist.
     */
    EngineWorker *getEngine(uint32_t index = 0)
    {
        return gameContext_.getEngine(index);
    }

    void restartEngine(const std::string &id)
    {
        stop();
        gameContext_.restartPlayer(id);
    }

    /**
     * @brief Stops the engine with the given identifier.
     * @param id The unique identifier of the engine to stop.
     */
    void stopEngine(const std::string &id)
    {
        stop();
        gameContext_.stopEngine(id);
    }

    /**
     * @brief Sets the time controls for the players.
     * @param timeControl The time control.
     */
    void setTimeControl(const TimeControl &timeControl)
    {
        gameContext_.setTimeControl(timeControl);
    }

    /**
     * @brief Sets the time controls for each player.
     * @param timeControls A vector of TimeControl objects for each player.
     * @param informEngines If true, informs each player's engine of the new time control.
     */
    void setTimeControls(const std::vector<TimeControl> &timeControls, bool informEngines = true)
    {
        gameContext_.setTimeControls(timeControls, informEngines);
    }

    /**
     * @brief Notifies all engines that a new game starts and resets their internal state.
     */
    void newGame()
    {
        gameContext_.newGame();
    }

    /**
     * @brief Sets a position
     * @param useStartPosition If true, uses the standard starting position.
     * @param fen Optional FEN string.
     * @param playedMoves Optional list of moves already played.
     */
    void setPosition(bool useStartPosition, const std::string &fen = "",
                     const std::optional<std::vector<std::string>>& playedMoves = std::nullopt);

    /**
     * @brief Sets the current game position for all engines using a GameRecord.
     *
     * Initializes the game state and engine contexts from the provided GameRecord,
     * including starting position, move history, and any associated metadata.
     * All engines are updated to reflect the position and history described in the record.
     *
     * @param game The GameRecord containing the full game setup and move history.
     */
    void setPosition(const GameRecord &game);

    /**
     * @brief Sets the current move index in the game.
     * @param moveIndex The move index to set (0 = before first move).
     */
    void setNextMoveIndex(uint32_t moveIndex);

    /**
     * @brief Executes a move in the current game position.
     * @param move The move to execute. Note: move.move must be set.
     */
    void doMove(const MoveRecord& move)
    {
        gameContext_.doMove(move);
        if (taskType_ == ComputeTaskType::PlaySide)
        {
            computeMove();
        } else {
            taskType_ = ComputeTaskType::None;
        }
    }

    /**
     * @brief computes a single move for the current position.
     */
    void computeMove();

    /**
     * @brief allows pondering on a previous event.
     * @param event preceding event to ponder on
     */
    void ponderMove(const std::optional<EngineEvent>& event);

    /**
     * @brief Analyzes the current position
     * Computes a best move with unlimited time and depth without playing it.
     */
    void analyze();

    /**
     * @brief Starts playing the game from the current position.
     *        The engine will play as white and black alternately.
     */
    void playSide()
    {
        stop();
        taskType_ = ComputeTaskType::PlaySide;
        computeMove();
    }

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
    const std::future<void> &getFinishedFuture() const;

    /**
     * @brief Stops any ongoing computation immediately.
     */
    void stop();

    /**
     * Engine Information
     */
    EngineRecords getEngineRecords()
    {
        return gameContext_.getEngineRecords();
    }

    MoreRecords getMoveInfos() const
    {
        return gameContext_.getMoveInfos();
    }

    const GameRecord &gameRecord() const
    {
        return gameContext_.gameRecord();
    }

    const GameContext &getGameContext() const
    {
        return gameContext_;
    }

    GameContext& getGameContext()
    {
        return gameContext_;
    }

    /**
     * @brief Returns the current status of the task.
     */
    std::string getStatus() const
    {
        if (taskType_ == ComputeTaskType::Autoplay)
        {
            return "Auto";
        }
        if (taskType_ == ComputeTaskType::Analyze)
        {
            return "Analyze";
        }
        if (taskType_ == ComputeTaskType::PlaySide)
        {
            return "Play";
        }
        return "Stopped";
    }

    bool isStopped() const
    {
        return taskType_ == ComputeTaskType::None;
    }

private:
    GameContext gameContext_;

    /**
     * @brief Defines the currently active automated task mode.
     */
    enum class ComputeTaskType : std::uint8_t
    {
        None,
        Analyze,
        Autoplay,
        ComputeMove,
        PlaySide
    };
    ComputeTaskType taskType_ = ComputeTaskType::None;

    /**
     * @brief Continues the current automatic task after a best move, if applicable.
     */
    void nextMove(const EngineEvent &event);
    void autoPlay(const std::optional<EngineEvent> &event);

    // Pass-by-value to leverage move semantics from callers and into the queue
    void enqueueEvent(EngineEvent&& event);
    void processQueue();
    void processEvent(const EngineEvent &event);
    void handleBestMove(const EngineEvent &event);
    void handlePonderMove(const EngineEvent &event);

    bool checkGameOver(bool verbose = false);

    void markFinished();
    void markRunning();
    std::promise<void> finishedPromise_;
    std::future<void> finishedFuture_;
    bool finishedPromiseValid_ = false;

    bool logMoves_ = false;

    std::thread eventThread_;
    std::atomic<bool> stopThread_{false};
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::queue<EngineEvent> eventQueue_;
};

} // namespace QaplaTester
