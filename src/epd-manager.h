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
#include "epd-test.h"
#include "time-control.h"
#include "game-manager-pool.h"

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <iostream>
#include <iomanip>

namespace QaplaTester {

class GameManager;
using TestResults = std::vector<EpdTestResult>;
 
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
     */
    void initialize(const std::string& filepath, uint64_t maxTimeInS, uint64_t minTimeInS, uint32_t seenPlies);

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

	double getSuccessRate() const;

    /**
     * @brief Get a copy of all current test results.
     * 
     * @return TestResults 
     */
    TestResults getResultsCopy() const;

    /**
	 * @brief Returns the update count of all test instances. 
	 * Use this to check, if the EPD tests have been updated since the last getResultsCopy call.
	 */
    uint64_t getUpdateCount() const {
		uint64_t count = 0;
        for (const auto& instance : testInstances_) {
			count += instance->getUpdateCount();
        }
        return count;
	}

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
     */
    void initializeTestCases(uint64_t maxTimeInS, uint64_t minTimeInS, uint32_t seenPlies);
    /**
     * @brief Retrieves and transforms the next EPD entry into a test case.
     * @return Optional EpdTestCase or std::nullopt if no more entries are available.
     */
    std::optional<EpdTestCase> nextTestCaseFromReader();
    
    std::string generateHeaderLine() const;
    void logHeaderLine() const;

    static std::string generateResultLine(const EpdTestCase& current, const TestResults& results);
    void logResultLine(const EpdTestCase& current) const;

    std::unique_ptr<EpdReader> reader_;
    std::vector<EpdTestCase> testsRead_;
    std::vector<std::shared_ptr<EpdTest>> testInstances_;
    TimeControl tc_;
    std::string epdFileName_;
};

} // namespace QaplaTester
