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

#include "game-task.h"
#include "epd-reader.h"
#include "engine-config.h"

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <atomic>
#include <iostream>
#include <iomanip>
#include <functional>

namespace QaplaTester {

class GameManager;
class GameManagerPool;

struct EpdTestCase {
    std::string id;
    std::string fen;
    std::vector<std::string> bestMoves; // from "bm"
    EpdEntry original;

    // Result fields
    std::string playedMove;
    bool correct = false;
    bool tested = false;
    int searchDepth = -1;
    uint64_t timeMs = 0;
    uint64_t maxTimeInS = 0;
    uint64_t minTimeInS = 0;
    int seenPlies = -1;
    uint64_t nodeCount = 0;
    int correctAtDepth = -1;
    uint64_t correctAtNodeCount = 0;
    uint64_t correctAtTimeInMs = 0;
};

// std::ostream& operator<<(std::ostream& os, const EpdTestCase& test);

struct EpdTestResult {
    TimeControl tc_;
    std::string engineName;
	std::string testSetName;
	std::vector<EpdTestCase> result;
};
 
/**
  * Manages the analysis of EPD test sets using multiple chess engines in parallel.
  * Provides GameTasks for engine workers and collects their results.
  */
class EpdTest : public GameTaskProvider {
public:
    using TestResultCallback = std::function<void(EpdTest*, size_t fromIndex, size_t toIndex)>;

    EpdTest() = default;

    /**
	 * @brief initializes the EpdManager with the specified analysis parameters.
	 * @param tests The EpdTestResult containing the analysis parameters.
     */
    void initialize(const EpdTestResult& tests);

    /**
     * @brief Registers this EpdManager instance as a task provider in the GameManagerPool.
     *
     * This method must be called with a shared_ptr to this instance, ensuring proper lifetime management.
     *
     * @param self The shared_ptr owning this EpdManager instance.
     * @param engine The engine configuration to use for analysis.
     * @param pool The GameManagerPool to register with (default is the singleton instance).
     */
    static void schedule(const std::shared_ptr<EpdTest>& self, const EngineConfig& engine, 
        GameManagerPool& pool);

    /**
     * @brief Continues the analysis from the current state.
     *
     */
    void continueAnalysis();

    /**
     * @brief Provides the next EPD position to analyze.
     *
     * @return A GameTask with a unique taskId or std::nullopt if all positions have been analyzed.
     */
    std::optional<GameTask> nextTask() override;

    /**
     * @brief Processes the result of a completed analysis, matched via taskId.
     *
     * @param taskId The identifier of the task this result belongs to.
     * @param record The result containing the engine's move(s) and evaluation.
     * 
	 * @throws std::runtime_error if the taskId does not match any active task.
     */
    void setGameRecord(const std::string& taskId, const GameRecord& record) override;

    /**
     * @brief Sets a callback function to be invoked whenever a GameRecord is set.
     * This allows external components to react to new results.
     * 
     * @param callback The callback function to set, which takes a pointer to this EpdTest instance and 
     * the updated EpdTestCase.
     */
    void setTestResultCallback(TestResultCallback callback) {
        testResultCallback_ = std::move(callback);
    }

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
    bool setPV(const std::string& taskId,
        const std::vector<std::string>& pv,
        uint64_t timeInMs,
        std::optional<uint32_t> depth,
        std::optional<uint64_t> nodes,
        std::optional<uint32_t> multipv) override;

    /**
     * @brief Returns a copy of the current test results.
     * @warning This is an expensive operation that creates a deep copy of all results.
     * Use sparingly and cache the result if needed multiple times.
     * @return A copy of the current test results.
     */
    EpdTestResult getResultsCopy() const {
		std::scoped_lock lock(testResultMutex_);
        return result_;
	}

    uint64_t getUpdateCount() const {
        return updateCnt_;
	}

private:

    static bool isSameMove(const std::string& fen, const std::string& move1str, const std::string& move2str);

    TestResultCallback testResultCallback_;

    mutable std::mutex testResultMutex_;
    EpdTestResult result_;
    std::atomic<size_t> oldestIndexInUse_ = 0;
    std::atomic<size_t> testIndex_ = 0;
	std::atomic<uint64_t> updateCnt_ = 0;

};

} // namespace QaplaTester
