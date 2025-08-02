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
#include <vector>
#include <functional>

#include "player-context.h"
#include "time-control.h"
#include "game-result.h"
#include "game-record.h"
#include "engine-event.h"

 /**
  * @brief Manages all player contexts and game configuration including time control,
  *        game record, engine wiring and side assignment.
  */
class GameContext {
public:
    GameContext();
    ~GameContext() = default;

    /**
     * @brief Initializes all players using the provided engine list.
     * @param engines A list of engine instances.
     */
    void initPlayers(std::vector<std::unique_ptr<EngineWorker>> engines);

    /**
     * @brief Restarts the engine of the given player index.
     * @param index Index of the player to restart.
     */
    void restartPlayer(uint32_t index);

    /**
     * @brief Sets the time control for all players.
     * @param timeControl The time control.
     */
    void setTimeControl(const TimeControl& timeControl);

    /**
     * @brief Sets the time controls for each player based on the provided vector.
     * @param timeControls A vector of TimeControl objects for each player.
	 */
    void setTimeControls(const std::vector<TimeControl>& timeControls) {
        for (size_t i = 0; i < players_.size(); ++i) {
            if (i < timeControls.size()) {
                players_[i]->setTimeControl(timeControls[i]);
            } else {
                players_[i]->setTimeControl(TimeControl());
            }
        }
	}

    /**
     * @brief Sets the trace level to print on the terminal (std::out) for all players' engines.
     * @param traceLevel The trace level to set.
	 */
    void setCliTraceLevel(TraceLevel traceLevel) {
        for (auto& player : players_) {
			player->getEngine()->setTraceLevel(traceLevel);
        }
    }

    /**
     * @brief Stops all engines and their processes.
	 */
    void tearDown() {
        players_.clear();
	}

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
    void setPosition(bool useStartPosition, const std::string& fen = "",
        std::optional<std::vector<std::string>> playedMoves = std::nullopt);

    /**
     * @brief Sets the game record and initializes players from it.
     * @param record The game record to adopt.
     */
    void setPosition(const GameRecord& record);

    /**
     * @brief Returns the number of players.
     */
    size_t getPlayerCount() const;

    /**
     * @brief Returns pointer to the player at index.
     */
    PlayerContext* player(size_t index);

    /**
     * @brief Returns the player acting as white.
     */
    PlayerContext* getWhite();

    /**
     * @brief Returns the player acting as black.
     */
    PlayerContext* getBlack();

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
    void setEventCallback(std::function<void(EngineEvent&&)> callback);

    /**
     * @brief Returns the current game record.
     */
    GameRecord& gameRecord();

    /**
     * @brief Returns the current game record (const).
     */
    const GameRecord& gameRecord() const;

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
    EngineWorker* getEngine(uint32_t index = 0) {
		return players_.empty() ? nullptr : players_[index]->getEngine();
    }

    /**
     * @brief Finds the player matching the given engine identifier.
     * @param identifier The engine identifier to match.
     * @return Pointer to the matching PlayerContext, or nullptr if not found.
     */
    PlayerContext* findPlayerByEngineId(const std::string& identifier);

    /**
     * @brief Restarts players whose engine configuration requires restart.
     */
    void restartIfConfigured();

    /**
     * @brief Cancels any running computation on all players.
     */
    void cancelCompute();

private:
    std::vector<std::unique_ptr<PlayerContext>> players_;
    GameRecord gameRecord_;
    std::function<void(EngineEvent&&)> eventCallback_;
    bool switchedSide_ = false;
};
