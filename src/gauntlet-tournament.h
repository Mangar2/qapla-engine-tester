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
 * @author Volker B�hm
 * @copyright Copyright (c) 2025 Volker B�hm
 */

#pragma once

#include "openings.h"
#include "engine-config.h"
#include "game-task.h"
#include "game-record.h"
#include <vector>
#include <string>
#include <optional>

 /**
  * @brief Configuration parameters for a Gauntlet tournament.
  */
struct TournamentConfig {
    std::string event;
    std::string type;
    int games = 2;
    int rounds = 1;
    int repeat = 2;
    bool noSwap = false;
    TimeControl tc;
    Openings openings;
};

/**
 * @brief Manages a Gauntlet tournament where each gauntlet engine plays against all opponents.
 *
 * Provides game tasks to workers and processes results asynchronously.
 */
class GauntletTournament : public GameTaskProvider {
public:
    GauntletTournament() = default;

    /**
     * @brief Starts a Gauntlet tournament.
     *
     * @param engines List of all engines (each marked via EngineConfig::isGauntlet).
     * @param concurrency Number of parallel workers.
     * @param config Tournament parameters.
     */
    void run(const std::vector<EngineConfig>& engines,
        int concurrency,
        const TournamentConfig& config);

    /**
     * @brief Waits for all games to finish.
     * @return true if all games completed successfully, false otherwise.
     */
    bool wait();

    /**
     * @brief Provides the next GameTask to run.
     * @param whiteId The engine playing white.
     * @param blackId The engine playing black.
     * @return The next task or std::nullopt if none remain.
     */
    std::optional<GameTask> nextTask(const std::string& whiteId, const std::string& blackId) override;

    /**
     * @brief Submits the result of a finished game.
     * @param whiteId The engine playing white.
     * @param blackId The engine playing black.
     * @param record Game result and metadata.
     */
    void setGameRecord(const std::string& whiteId, const std::string& blackId,
        const GameRecord& record) override;

private:
    TournamentConfig config_;
    int concurrency_ = 1;
    std::vector<EngineConfig> gauntletEngines_;
    std::vector<EngineConfig> opponentEngines_;
    std::vector<std::string> startPositions_;
    std::vector<GameTask> tasks_;
    std::atomic<size_t> nextIndex_ = 0;
};