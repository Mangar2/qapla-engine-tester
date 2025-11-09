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

#include <optional>
#include <string>
#include "game-record.h"

namespace QaplaTester {

struct GameTask {
    enum class Type: std::uint8_t {
        None,
        FetchNextTask,
        ComputeMove,
        PlayGame
    };

    /** Unique identifier for tracking the task across engine interactions */
    std::string taskId;
    bool switchSide = false;
    Type taskType;
    GameRecord gameRecord;
};

class GameTaskProvider {
public:
    GameTaskProvider() = default;
    virtual ~GameTaskProvider() = default;

    /**
     * @brief Provides the next game task.
     * @return An optional GameTask. If no more tasks are available, returns std::nullopt.
     */
    virtual std::optional<GameTask> nextTask() = 0;

    /**
     * @brief Sets the game record for a given task.
     * @param taskId The identifier of the task.
     * @param record The game record to associate with the task.
     */
    virtual void setGameRecord(const std::string& taskId, const GameRecord& record) = 0;

    /**
     * @brief Reports a principal variation (PV) found by the engine during search.
     *        Allows the provider to track correct moves and optionally stop the search early.
     *
     * @param taskId        The id of the task receiving this update.
     * @param pv            The principal variation as a list of LAN moves.
     * @param timeInMs      Elapsed time in milliseconds.
     * @param depth         Current search depth.
     * @param nodes         Number of nodes searched.
     * @param multipv       MultiPV index (1 = best line).
     * @return true if the engine should stop searching, false to continue.
     */
    virtual bool setPV(
        [[maybe_unused]] const std::string& taskId,
        [[maybe_unused]] const std::vector<std::string>& pv,
        [[maybe_unused]] uint64_t timeInMs,
        [[maybe_unused]] std::optional<uint32_t> depth,
        [[maybe_unused]] std::optional<uint64_t> nodes,
        [[maybe_unused]] std::optional<uint32_t> multipv)
    {
        return false;
    }
};

} // namespace QaplaTester
