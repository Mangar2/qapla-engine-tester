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
    uint64_t nodeCount = 0;
    int correctAtDepth = -1;
    uint64_t correctAtNodeCount = 0;
    uint64_t correctAtTimeInMs = 0;
};

inline std::ostream& operator<<(std::ostream& os, const EpdTestCase& test) {
    auto formatTime = [](uint64_t ms) -> std::string {
        if (ms == 0) return "-";
        uint64_t minutes = ms / 60000;
        uint64_t seconds = (ms / 1000) % 60;
        uint64_t milliseconds = ms % 1000;
        std::ostringstream timeStream;
        timeStream << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds << "."
            << std::setw(3) << milliseconds;
        return timeStream.str();
        };

    os << std::setw(20) << std::left << test.id
        << " | " << std::setw(9) << std::right
        << (test.correct ? formatTime(test.correctAtTimeInMs) : "-")
        << " | D: " << std::setw(3) << std::right
        << (test.correct ? std::to_string(test.correctAtDepth) : "-")
        << " | M: " << std::setw(6) << std::left << test.playedMove
        << " | BM: ";

    for (const auto& bm : test.bestMoves) {
        os << bm << " ";
    }
    return os;
}

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
     * @param enginepath Path to the Engine file.
     * @param concurrency Number of engine instances to run in parallel.
	 * @param maxTimeInS Maximum allowed time in seconds for each engine to analyze a position.
     */
    void analyzeEpd(const std::string& filepath, const std::string& enginePath, uint32_t concurrency, uint64_t maxTimeInS);

    /**
     * @brief Stops the analysis. Running tasks may still complete.
     */
    void stop();

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
     */
    void initializeTestCases();
    /**
     * @brief Retrieves and transforms the next EPD entry into a test case.
     * @return Optional EpdTestCase or std::nullopt if no more entries are available.
     */
    std::optional<EpdTestCase> nextTestCaseFromReader();
    std::unique_ptr<EpdReader> reader_;
    std::vector<std::unique_ptr<GameManager>> managers_;
	std::vector<EpdTestCase> tests_; 
    std::mutex taskMutex_;
    int oldestIndexInUse_ = 0;
    int currentIndex_ = 0;
    TimeControl tc;

};
