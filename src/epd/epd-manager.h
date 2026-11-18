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

#include "epd-test.h"

#include "../opening/epd-reader.h"

#include "../engine-handling/engine-config.h"
#include "../base-elements/time-control.h"
#include "../base-elements/table-format.h"
#include "../game-manager/game-manager-pool.h"

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <iostream>

namespace QaplaTester {

class GameManager;
using TestResults = std::vector<EpdTestResult>;

/**
 * @brief Configuration parameters for EPD test execution.
 */
struct EpdConfig {
    std::string file;           ///< Path to the EPD file
    uint32_t maxTime = 20;      ///< Maximum allowed time in seconds per move
    uint32_t minTime = 2;       ///< Minimum required time for early stop
    uint32_t seenPlies = 0;     ///< Amount of plies one of the expected moves must be shown to stop early
    uint32_t depth = 0;         ///< Fixed depth limit (0 = disabled)
    uint32_t nodes = 0;         ///< Fixed node limit (0 = disabled)
    uint32_t minSuccess = 0;    ///< Minimum percentage of best moves that must be found
};

struct EpdStatusResult {
    TableData statusTable;
    uint64_t totalNodes = 0;
    uint64_t solvedCount = 0;
    uint64_t totalCount = 0;
    double hitRatePercent = 0.0;
};
 
/**
  * Manages the analysis of EPD test sets using multiple chess engines in parallel.
  * Provides GameTasks for engine workers and collects their results.
  */
class EpdManager {
public:
    EpdManager() = default;

    /**
     * @brief Loads all EPD entries from the specified file and starts analysis with the given number of engines.
     *        The method returns immediately; analysis runs asynchronously.
     * @param filepath Path to the EPD file.
	 * @param maxTimeInS Maximum allowed time in seconds for each engine to analyze a position.
	 * @param minTimeInS Minimum time in seconds each engine must spend at least on a position.
	 * @param seenPlies Minimum number of plies one of the expected moves must be shown to stop early.
	 * @param depth Fixed depth limit for each position. 0 means disabled.
	 * @param nodes Fixed node limit for each position. 0 means disabled.
	 * @param maxPositions Reads at most this many positions from the file (empty lines not
	 *        counted) and leaves the rest out; unset reads all. See wasTruncated().
     */
    void initialize(const std::string& filepath, uint64_t maxTimeInS, uint64_t minTimeInS, uint32_t seenPlies,
        uint32_t depth, uint32_t nodes, std::optional<size_t> maxPositions = std::nullopt);

    /**
     * @brief Whether the last initialize() left positions of the file out because of its
     *        maxPositions: the file holds more than that, and only the first ones were read.
     */
    [[nodiscard]] bool wasTruncated() const {
        return reader_ && reader_->wasTruncated();
    }

    /**
     * @brief Continues the analysis from the current state.
     */
    void continueAnalysis();
    
    /**
	 * @brief Removes all current test cases and resets the EPD manager.
	 * *** Attention: You need to ensure that the ManagerPool is cleared before ***
	 */
    void clear();

    /**
     * @brief Registers this EpdManager instance as a task provider in the GameManagerPool.
     *
     * This method must be called with a shared_ptr to this instance, ensuring proper lifetime management.
     *
	 * @param engineConfig The configuration for the engine to be used in the analysis.
     */
    void schedule(const EngineConfig& engineConfig, GameManagerPool& pool = GameManagerPool::getInstance());

	[[nodiscard]] double getSuccessRate() const;

    /**
     * @brief Get a copy of all current test results.
     * 
     * @return TestResults 
     */
    [[nodiscard]] TestResults getResultsCopy() const;

    /**
	 * @brief Returns the update count of all test instances. 
	 * Use this to check, if the EPD tests have been updated since the last getResultsCopy call.
	 */
    [[nodiscard]] uint64_t getUpdateCount() const {
		uint64_t count = 0;
        for (const auto& instance : testInstances_) {
			count += instance->getUpdateCount();
        }
        return count;
	}

    /**
     * @brief Per engine, what changed in its results since the update counts a caller has seen.
     *
     * For a caller that keeps a copy of the results: it applies the changed positions instead of
     * copying the whole set on every update.
     *
     * @param seenCounts Per engine, in the order of the answer, the updateCount it last returned;
     *        an engine without one counts as nothing seen.
     * @return One entry per engine; an engine's whole results where the caller's copy is void.
     */
    [[nodiscard]] std::vector<EpdResultChanges> getChangesSince(
        const std::vector<uint64_t>& seenCounts) const;

    [[nodiscard]] TableData getStatusTable() const;

    [[nodiscard]] EpdStatusResult getStatusResult() const;

    /**
     * @brief Outputs the current results to the provided output stream in a human-readable format.
     * @param os The output stream to write results to.
     */
    void saveResults(std::ostream& os) const;

    /**
     * @brief Loads results from the provided input stream, expecting the same format as produced by saveResults.
     * @param is The input stream to read results from.
     * @return true if any data was loaded, false otherwise.
     */
    bool loadResults(std::istream& is);

private:
    /**
     * @brief Loads and transforms all EPD entries into test cases.
	 * @param maxTimeInS Maximum allowed time in seconds for each engine to analyze a position.
	 * @param minTimeInS Minimum time in seconds each engine must spend at least on a position.
	 * @param seenPlies Minimum number of plies one of the expected moves must be shown to stop early.
	 * @param depth Fixed depth limit for each position. 0 means disabled.
	 * @param nodes Fixed node limit for each position. 0 means disabled.
     */
    void initializeTestCases(uint64_t maxTimeInS, uint64_t minTimeInS, uint32_t seenPlies,
        uint32_t depth, uint32_t nodes);
    /**
     * @brief Retrieves and transforms the next EPD entry into a test case.
     * @return Optional EpdTestCase or std::nullopt if no more entries are available.
     */
    std::optional<EpdTestCase> nextTestCaseFromReader();
    
   [[nodiscard]] std::string generateHeaderLine() const;
    void logHeaderLine() const;

    /**
     * @brief One line of the result table: the position, then what each engine found for it.
     *
     * Every engine's results hold the positions in the order of testsRead_, so the caller hands in
     * each engine's entry for the position directly. A search by id per line made writing the
     * table quadratic in the number of positions: 600 000 positions never finished.
     *
     * @param current The position as read from the EPD file.
     * @param engineResults Per engine its result for this position, nullptr where it has none.
     */
    static std::string generateResultLine(const EpdTestCase& current,
        const std::vector<const EpdTestCase*>& engineResults);

    /**
     * @brief Each engine's result for the position at index, nullptr where it has none.
     * @param index Position in testsRead_.
     * @param results Results of all engines, each in the order of testsRead_.
     */
    [[nodiscard]] std::vector<const EpdTestCase*> engineResultsAt(size_t index,
        const TestResults& results) const;

    std::unique_ptr<EpdReader> reader_;
    std::optional<size_t> maxPositions_;    ///< The limit reader_ was read with
    std::vector<EpdTestCase> testsRead_;
    std::vector<std::shared_ptr<EpdTest>> testInstances_;
    TimeControl tc_;
    std::string epdFileName_;
};

} // namespace QaplaTester
