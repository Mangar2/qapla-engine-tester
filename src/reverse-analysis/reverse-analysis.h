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

#include "../base-elements/time-control.h"
#include "../chess-game/game-record.h"
#include "../engine-handling/engine-config.h"
#include "../game-manager/game-manager-pool.h"
#include "../game-manager/game-task.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief Settings for a reverse analysis run.
 */
struct ReverseAnalysisConfig {
    std::string file;            ///< Path to the PGN file holding the games to analyse
    uint32_t moveTimeMs = 200;   ///< Fixed time per half move
    uint32_t maxGames = 0;       ///< Maximum number of games to analyse (0 = all)
};

/**
 * @brief Hands out one game per task, to be recomputed from its last move backwards.
 *
 * One task is one whole game, which is what makes the analysis work: the engine keeps its
 * transposition table over the whole backward walk and meets each position already knowing how
 * the game continued from it. Several games run in parallel, but each of them on an engine of
 * its own - the pool gives every game manager its own engine process.
 */
class ReverseAnalysis : public GameTaskProvider {
public:
    ReverseAnalysis() = default;

    /**
     * @brief Loads the games to analyse and fixes the search limit per half move.
     * @param config The run's settings.
     * @return The number of games read.
     */
    size_t initialize(const ReverseAnalysisConfig& config);

    /**
     * @brief Registers this instance as a task provider and starts the run.
     * @param self Shared owner of this instance.
     * @param engine The engine that analyses the games.
     * @param pool Pool used for scheduling and concurrency control.
     */
    static void schedule(const std::shared_ptr<ReverseAnalysis>& self, const EngineConfig& engine,
        GameManagerPool& pool = GameManagerPool::getInstance());

    std::optional<GameTask> nextTask() override;

    void setGameRecord(const std::string& taskId, const GameRecord& record) override;

    /**
     * @brief Returns the games that have been analysed, in the order they were read.
     */
    [[nodiscard]] std::vector<GameRecord> getAnalysedGames() const;

    /**
     * @brief Returns the number of games that have been analysed.
     */
    [[nodiscard]] size_t getFinishedCount() const { return finishedCount_.load(); }

private:
    /**
     * @brief Reports one finished game and how much of it carries an evaluation.
     */
    void logGameResult(size_t index, const GameRecord& record) const;

    mutable std::mutex mutex_;
    std::vector<GameRecord> games_;      ///< The games as they were read
    std::vector<bool> analysed_;         ///< Which of them came back analysed
    size_t nextIndex_ = 0;               ///< Index of the game handed out next
    std::atomic<size_t> finishedCount_ = 0;
    TimeControl timeControl_;
    std::string fileName_;
};

} // namespace QaplaTester
