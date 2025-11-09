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

#include "player-context.h"
#include "time-control.h"
#include "game-result.h"
#include "game-record.h"
#include "engine-event.h"
#include "engine-record.h"

#include <memory>
#include <vector>
#include <functional>
#include <utility>

namespace QaplaTester {

/**
 * @brief Manages all player contexts and game configuration including time control,
 *        game record, engine wiring and side assignment.
 */
class GameContext
{
public:
    GameContext();
    ~GameContext();

    /**
     * @brief Initializes all players using the provided engine list.
     * @param engines A list of engine instances.
     */
    void initPlayers(std::vector<std::unique_ptr<EngineWorker>> engines);

    /**
     * @brief Restarts the player with the given identifier.
     * @param id The unique identifier of the player to restart.
     */
    void restartPlayer(const std::string &id);

    /**
     * @brief Stops the engine process for the player with the given identifier.
     *
     * This method searches for the player whose engine matches the provided ID
     * and terminates its engine process.
     *
     * @param id The unique identifier of the engine to stop.
     */
    void stopEngine(const std::string &id);

    /**
     * @brief Sets the time control for all players.
     * @param timeControl The time control.
     */
    void setTimeControl(const TimeControl &timeControl);

    /**
     * @brief Sets the time controls for each player based on the provided vector.
     * @param timeControls A vector of TimeControl objects for each player.
     * @param informEngines If true, informs each player's engine of the new time control.
     */
    void setTimeControls(const std::vector<TimeControl> &timeControls, bool informEngines = true);

    /**
     * @brief Sets the trace level to print on the terminal (std::out) for all players' engines.
     * @param traceLevel The trace level to set.
     */
    void setCliTraceLevel(TraceLevel traceLevel)
    {
        for (auto &player : players_)
        {
            player->getEngine()->setTraceLevel(traceLevel);
        }
    }

    /**
     * @brief Stops all engines and their processes.
     */
    void tearDown();

    /**
     * @brief Informs all players that a new game has started.
     */
    void newGame();

    /**
     * @brief Sets the game position and optionally applies a move history.
     * @param useStartPosition Use standard start position if true.
     * @param fen Optional FEN string.
     * @param playedMoves Optional move history.
     */
    void setPosition(bool useStartPosition, const std::string &fen = "",
                     const std::optional<std::vector<std::string>>& playedMoves = std::nullopt);

    /**
     * @brief Sets the game record and initializes players from it.
     * @param game The game record to adopt.
     */
    void setPosition(const GameRecord &gameRecord);

    /**
     * @brief Sets the current move index in the game.
     * @param moveIndex The move index to set (0 = before first move).
     */
    void setNextMoveIndex(uint32_t moveIndex);

    /**
     * @brief Executes a new move to the move record.
     * @param move The move to execute. Note: move.move must be set.
     */
    void doMove(const MoveRecord& move);

    /**
     * @brief Returns the number of players.
     */
    size_t getPlayerCount() const;

    /**
     * @brief Returns pointer to the player at index.
     */
    PlayerContext *player(size_t index);

    /**
     * @brief Returns the player acting as white.
     */
    PlayerContext *getWhite();
    const PlayerContext *getWhite() const;

    /**
     * @brief Returns the player acting as black.
     */
    PlayerContext *getBlack();
    const PlayerContext *getBlack() const;

    /**
     * @brief Sets whether white/black are logically switched.
     * @param switched If true, logical colors are flipped.
     */
    void setSideSwitched(bool switched);

    /**
     * @brief Returns true if white/black roles are currently switched.
     */
    bool isSideSwitched() const;

    /**
     * @brief Sets the event callback that is assigned to all engines.
     */
    void setEventCallback(std::function<void(EngineEvent &&)> callback);

    /**
     * @brief sets the game record.
     * @param record The game record to set.
     */
    void setGameRecord(const GameRecord &record)
    {
        std::scoped_lock lock(gameRecordMutex_);
        gameRecord_ = record;
    }

    /**
     * @brief Adds a move to the game record.
     * @param move The move to add.
     */
    void addMove(const MoveRecord &move)
    {
        std::scoped_lock lock(gameRecordMutex_);
        gameRecord_.addMove(move);
    }

    /**
     * @brief Adds a move to the game record.
     * @param move The move to add.
     */
    void updateMove(const MoveRecord &move)
    {
        std::scoped_lock lock(gameRecordMutex_);
        gameRecord_.updateMove(move);
    }

    /**
     * @brief Sets the end state of the current game.
     *
     * @param cause The reason why the game ended (e.g. checkmate, draw, resignation).
     * @param result The result of the game (e.g. win, loss, draw).
     */
    void setGameEnd(GameEndCause cause, GameResult result)
    {
        std::scoped_lock lock(gameRecordMutex_);
        gameRecord_.setGameEnd(cause, result);
    }

    /**
     * @brief Returns the current game record (const).
     * Note it is not thread-safe to get the game record. Use withGameRecord() instead.
     */
    const GameRecord &gameRecord() const;

    /**
     * Executes the given callable with thread-safe access to the game record.
     * The callable receives a const reference to the game record.
     *
     * @param accessFn A callable that takes a const GameRecord&.
     */
    void withGameRecord(const std::function<void(const GameRecord &)> &accessFn) const;

    /**
     * @brief Returns the result of the game.
     */
    std::tuple<GameEndCause, GameResult> checkGameResult();

    /**
     * @brief Checks all players for engine timeout and restarts them if necessary.
     * @return True if at least one engine was restarted.
     */
    bool checkForTimeoutsAndRestart();

    /**
     * @brief Returns the player at the specified index.
     * @param index The index of the player to return.
     * @return Pointer to the PlayerContext at the given index, or nullptr if no players exist.
     */
    EngineWorker *getEngine(uint32_t index = 0)
    {
        return players_.empty() ? nullptr : players_[index]->getEngine();
    }

    /**
     * @brief Finds the player matching the given engine identifier.
     * @param identifier The engine identifier to match.
     * @return Pointer to the matching PlayerContext, or nullptr if not found.
     */
    PlayerContext *findPlayerByEngineId(const std::string &identifier);

    /**
     * @brief Restarts players whose engine configuration requires restart.
     */
    void restartIfConfigured();

    /**
     * @brief Cancels any running computation on all players.
     * @param keepPondering If true, keeps pondering engines in pondering state.
     */
    void cancelCompute(bool keepPondering = false);

    /**
     * @brief Returns list of information about all engines.
     * @return A vector of EngineRecords containing engine information for each player.
     */
    EngineRecords getEngineRecords() const {
        std::scoped_lock lock(engineMutex_);
        return mkEngineRecords();
    }

    /**
     * @brief Executes the given callable with thread-safe access to the engine records.
     * @param accessFn A callable that takes a const EngineRecords&.
     */
    void withEngineRecords(const std::function<void(const EngineRecords&)>& accessFn) const
    {
        std::scoped_lock lock(engineMutex_);
        accessFn(mkEngineRecords());
    }

    std::pair<std::string, std::string> getEngineNames() const
    {
        std::scoped_lock lock(engineMutex_);
        if (getWhite() == nullptr || getBlack() == nullptr)
        {
            return {"", ""};
        }
        return {
            getWhite()->getEngine()->getConfig().getName(),
            getBlack()->getEngine()->getConfig().getName()};
    }

    /**
     * @brief Returns the current move information for all players.
     * @return A vector of MoveInfos containing move information for each player.
     */
    MoreRecords getMoveInfos() const;

    /**
     * @brief Executes the given callable with thread-safe access to the current move records of all players.
     * @param accessFn A callable that takes a const MoveRecord&.
     */
    void withMoveRecord(std::function<void(const MoveRecord&, uint32_t)> accessFn) const;

    /**
     * @brief Ensures all engines are started and ready for the next command.
     */
    void ensureStarted();

private:
    void updateEngineNames();
    /**
     * @brief Restarts the engine for the given player.
     *
     * @param player The player whose engine should be restarted.
     * @param differentThread If true, the restart is triggered from a different thread.
     */
    void playerRestartEngine(PlayerContext* player, bool differentThread = true);

    mutable std::mutex engineMutex_;
    std::vector<std::unique_ptr<PlayerContext>> players_;

    mutable std::mutex gameRecordMutex_;
    GameRecord gameRecord_;
    /**
     * @brief Returns list of information about all engines.
     * @return A vector of EngineRecords containing engine information for each player.
     */
    EngineRecords mkEngineRecords() const;


    std::function<void(EngineEvent &&)> eventCallback_;
    bool switchedSide_ = false;
};

} // namespace QaplaTester
