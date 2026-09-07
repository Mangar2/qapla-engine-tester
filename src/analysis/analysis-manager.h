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
 * @brief Direction a game is recomputed in.
 */
enum class AnalysisDirection : std::uint8_t {
    Forward,   ///< From the first move to the last, the order the game was played in
    Backward   ///< From the last move to the first
};

/**
 * @brief Settings for an analysis run.
 */
struct AnalysisConfig {
    std::string pgnFile;                                          ///< PGN file holding the games
    AnalysisDirection direction = AnalysisDirection::Backward;    ///< Direction to recompute in
    uint32_t maxGames = 0;                                        ///< Games to analyse (0 = all)
};

/**
 * @brief Hands out one game per task, to be recomputed move by move.
 *
 * One task is one whole game, which is what the backward direction lives on: the engine keeps its
 * transposition table over the whole walk and meets each position already knowing how the game
 * continued from it. Several games run at the same time, but each of them on an engine of its own
 * - the pool gives every game manager its own engine process.
 *
 * The search limit comes from the engine's time control and has to be a per-move limit; a game
 * clock has nothing to apply to here, since every position is given the same limit.
 */
class AnalysisManager : public GameTaskProvider {
public:
    AnalysisManager() = default;

    /**
     * @brief Loads the games to analyse.
     * @param config The run's settings.
     * @return The number of games that can be analysed.
     */
    size_t initialize(const AnalysisConfig& config);

    /**
     * @brief Sets the engine of the current run and its search limit.
     *
     * Called once per engine: the games are analysed again for each engine given, and each run
     * writes its own copy of every game, marked with the engine that produced the evaluations.
     *
     * @param engine The engine that analyses the games in this run.
     */
    void startRun(const EngineConfig& engine);

    /**
     * @brief Registers this instance as a task provider and starts the run.
     * @param self Shared owner of this instance.
     * @param engine The engine that analyses the games.
     * @param pool Pool used for scheduling and concurrency control.
     */
    static void schedule(const std::shared_ptr<AnalysisManager>& self, const EngineConfig& engine,
        GameManagerPool& pool = GameManagerPool::getInstance());

    std::optional<GameTask> nextTask() override;

    void setGameRecord(const std::string& taskId, const GameRecord& record) override;

    /**
     * @brief Returns the number of games finished in the current run.
     */
    [[nodiscard]] size_t getFinishedCount() const { return finishedCount_.load(); }

    /**
     * @brief Checks that the engine's time control is a limit that applies to a single move.
     * @param engine The engine to check.
     * @throws AppError if the engine has no per-move limit, naming what to set instead.
     */
    static void requirePerMoveLimit(const EngineConfig& engine);

private:
    /**
     * @brief Reports one finished game and how much of it carries an evaluation.
     */
    void logGameResult(size_t index, const GameRecord& record) const;

    mutable std::mutex mutex_;
    std::vector<GameRecord> games_;   ///< The games as they were read, unchanged between runs
    size_t nextIndex_ = 0;            ///< Index of the game handed out next in this run
    std::atomic<size_t> finishedCount_ = 0;
    AnalysisDirection direction_ = AnalysisDirection::Backward;
    TimeControl timeControl_;         ///< The current engine's per-move limit
    std::string engineName_;          ///< The engine of the current run
};

} // namespace QaplaTester
