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

#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <iostream>
#include <iomanip>
#include "game-task.h"
#include "epd-reader.h"

class GameManager;

struct EpdTestCase {
    std::string id;
    std::string fen;
    std::vector<std::string> bestMoves; // from "bm"
    EpdEntry original;

    // Result fields
    std::string engineId;
    std::string playedMove;
    bool correct = false;
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

std::ostream& operator<<(std::ostream& os, const EpdTestCase& test);

struct TestResult {
    std::string engineName;
	std::string testSetName;
	std::vector<EpdTestCase> results;
};

using TestResults = std::vector<TestResult>;
 
/**
  * Manages the analysis of EPD test sets using multiple chess engines in parallel.
  * Provides GameTasks for engine workers and collects their results.
  */
class EpdManager : public GameTaskProvider {
public:
    EpdManager() = default;

    /**
     * @brief Loads all EPD entries from the specified file and starts analysis with the given number of engines.
     *        The method returns immediately; analysis runs asynchronously.
     * @param filepath Path to the EPD file.
     * @param engineName name of the engine
     * @param concurrency Number of engine instances to run in parallel.
	 * @param maxTimeInS Maximum allowed time in seconds for each engine to analyze a position.
	 * @param minTimeInS Minimum time in seconds each engine must spend at least on a position.
	 * @param seenPlies Minimum number of plies one of the expected moves must be shown to stop early.
     */
    void analyzeEpd(const std::string& filepath, const std::string& engineName, uint32_t concurrency, int maxTimeInS, int minTimeInS, int seenPlies);

    /**
     * @brief Waits for all engines to finish.
     * @return true if all tasks completed successfully, false if the analysis was stopped prematurely.
     */
    bool wait();

    /**
     * @brief Dynamically adjusts the number of parallel engines.
     *        New engines will be started or idle ones shut down after finishing their current task.
     *        Does not interrupt active analysis.
     * @param concurrency Desired number of concurrent engines.
     */
    void changeConcurrency(uint32_t concurrency) { throw "not yet implemented"; };

    /**
     * @brief Provides the next EPD position to analyze.
     * @param whiteId The identifier for the white player.
     * @param blackId The identifier for the black player.
     * @return An optional GameTask. If no more tasks are available, returns std::nullopt.
     */
    std::optional<GameTask> nextTask(const std::string& whiteId, const std::string& blackId) override;

    /**
     * @brief Processes the result of a completed analysis.
     * @param whiteId The identifier for the white player.
     * @param blackId The identifier for the black player.
     * @param record The result containing the engine's move(s) and evaluation.
     */
    void setGameRecord(const std::string& whiteId, const std::string& blackId,
        const GameRecord& record) override;

    /**
     * @brief Reports a principal variation (PV) found by the engine during search.
     *        Allows the provider to track correct moves and optionally stop the search early.
     *
     * @param engineId      The id of the engine computing the result.
     * @param pv            The principal variation as a list of LAN moves.
     * @param timeInMs      Elapsed time in milliseconds.
     * @param depth         Current search depth.
     * @param nodes         Number of nodes searched.
     * @param multipv       MultiPV index (1 = best line).
     * @return true if the engine should stop searching, false to continue.
     */
    bool setPV(const std::string& engineId,
        const std::vector<std::string>& pv,
        uint64_t timeInMs,
        std::optional<uint32_t> depth,
        std::optional<uint64_t> nodes,
        std::optional<uint32_t> multipv) override;

private:
    bool isSameMove(const std::string& fen, const std::string& lanMove, const std::string& sanMove) const;
    /**
     * @brief Loads and transforms all EPD entries into test cases.
	 * @param maxTimeInS Maximum allowed time in seconds for each engine to analyze a position.
	 * @param minTimeInS Minimum time in seconds each engine must spend at least on a position.
	 * @param seenPlies Minimum number of plies one of the expected moves must be shown to stop early.
     * @param clearTests true, if the tests shall be fully clear (old results gets forgotten)
     */
    void initializeTestCases(int maxTimeInS, int minTimeInS, int seenPlies, bool clearTests = true);
    /**
     * @brief Retrieves and transforms the next EPD entry into a test case.
     * @return Optional EpdTestCase or std::nullopt if no more entries are available.
     */
    std::optional<EpdTestCase> nextTestCaseFromReader();
    void printHeaderLine() const;
    std::string formatTime(uint64_t ms) const;
    std::string formatInlineResult(const EpdTestCase& test) const;
    void printTestResultLine(const EpdTestCase& current) const;

    std::unique_ptr<EpdReader> reader_;
	std::vector<EpdTestCase> tests_; 
	TestResults results_;
    std::string engineName_;
    std::string epdFileName_;
    std::mutex taskMutex_;
    int oldestIndexInUse_ = 0;
    int currentIndex_ = 0;
    TimeControl tc;

};
