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
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#pragma once

#include "book-tree.h"

#include "../base-elements/callback-timer.h"
#include "../base-elements/time-control.h"
#include "../engine-handling/engine-config.h"
#include "../game-manager/game-manager-pool.h"
#include "../game-manager/game-task.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace QaplaTester {

/**
 * @brief Settings for evaluating the positions of a book.
 */
struct BookEvalConfig {
    std::string bookFile;           ///< The 9 bit book whose positions are evaluated
    std::string outFile;            ///< The evaluated book; also the state a run continues from
    uint32_t saveIntervalMs = 60000; ///< Time between two saves of the evaluated book
    uint64_t maxPositions = 0;      ///< Positions to search in this run (0 = all that are left)
    /**
     * Version of the evaluated book: 2 (the default) gives every node the score of the position
     * after its move, 1 every node the score of the position it is played from.
     */
    uint32_t version = 2;
};

/**
 * @brief Searches every position of a 9 bit book once and writes the scores into a new book.
 *
 * Version 2, the default, searches every position: the start position and the one after every node,
 * which carries the score from the side that played its move (qapla-search-score-v2). Version 1
 * searches the positions a move of the book is played from - the start position and every position
 * after a move that has moves below it - and gives every node the score of the position before its
 * move (qapla-search-score-v1). Each search is a task of its own, with a new game
 * before it: the engine starts cold, from the position and the moves leading to it, so a score
 * does not depend on what the engine searched before, and therefore neither on the order of the
 * positions nor on the number of engines. It is the search --analysis makes of the last move of a
 * game, before the transposition table holds anything.
 *
 * The evaluated book is the state of the run. It is written every saveIntervalMs and at the end,
 * with no score for the positions still open, and a run given the same output file continues from
 * there: the tree has to be the one of the input book and the engine and its limit those of the
 * book's header.
 *
 * A book that is evaluated already - in either version, with the same engine and limit - brings its
 * scores into a new run: a version 2 run of a version 1 book only searches the leaves, every other
 * position was searched with the same moves before it and gives the same score.
 */
class BookEvalManager : public GameTaskProvider {
public:
    BookEvalManager() = default;

    /**
     * @brief Reads the book and, if the output book exists, continues from its scores.
     * @param config The run's settings.
     * @param engine The engine that searches; its time control is the limit of every search.
     * @return The number of positions still to search.
     * @throws AppError if the books cannot be read, do not belong together or were evaluated with
     *         another engine or limit.
     */
    size_t initialize(const BookEvalConfig& config, const EngineConfig& engine);

    /**
     * @brief Registers this instance as a task provider and starts the run.
     */
    static void schedule(const std::shared_ptr<BookEvalManager>& self, const EngineConfig& engine,
        GameManagerPool& pool = GameManagerPool::getInstance());

    std::optional<GameTask> nextTask() override;

    void setGameRecord(const std::string& taskId, const GameRecord& record) override;

    /**
     * @brief Writes the evaluated book as far as it is.
     */
    void save();

    /** @brief Positions whose score was taken from the input book instead of searched. */
    [[nodiscard]] size_t getTakenCount() const;
    /** @brief Positions that get a score, in the whole book. */
    [[nodiscard]] size_t getPositionCount() const;
    /** @brief Positions that have their score. */
    [[nodiscard]] size_t getEvaluatedCount() const;
    /** @brief Searches whose result did not bring a score. */
    [[nodiscard]] size_t getFailedCount() const;
    /** @brief True once every position has its score. */
    [[nodiscard]] bool isComplete() const;
    /** @brief Positions searched in this run per second, over the run so far. */
    [[nodiscard]] double getPositionsPerSecond() const;
    /**
     * @brief True once this run has handed out every position it is going to and has every
     *        search back - the moment to save and report, for a caller that drives the pool itself.
     */
    [[nodiscard]] bool isRunFinished() const;

    /** @brief Writes a line with the state of the run to the report log. */
    void logProgress() const;

    /**
     * @brief The text the header of an evaluated book names its engine and limit with.
     */
    [[nodiscard]] static std::string describeSearch(const EngineConfig& engine);

private:
    void logProgressLocked() const;

    mutable std::mutex mutex_;
    BookTree tree_;
    BookEvalConfig config_;
    QaplaBook::BookHeader outHeader_;
    TimeControl timeControl_;
    std::vector<uint32_t> open_;      ///< Positions without a score when the run started
    size_t nextOpen_ = 0;             ///< Index into open_ of the position handed out next
    std::map<uint32_t, size_t> inFlight_;   ///< Positions being searched, with the length of their line
    size_t positionCount_ = 0;
    size_t evaluatedCount_ = 0;
    size_t searchedInRun_ = 0;
    size_t failedCount_ = 0;
    size_t takenCount_ = 0;
    std::chrono::steady_clock::time_point startTime_;
    QaplaHelpers::CallbackTimer saveTimer_;
    QaplaHelpers::CallbackTimer progressTimer_;
};

} // namespace QaplaTester
