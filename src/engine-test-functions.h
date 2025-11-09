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

#include <vector>
#include <string>
#include <functional>
#include "engine-config.h"
#include "engine-worker-factory.h"

namespace QaplaTester {

/**
 * @brief Function-based engine test implementations
 * 
 * This file contains test functions that can be called from various contexts.
 * Each test function takes all necessary parameters and returns results as a vector of TestResultEntry.
 */

/**
 * @brief Single test result entry
 */
struct TestResultEntry {
    std::string testName;
    std::string result;
    bool success;

    TestResultEntry(std::string name, std::string res, bool succ)
        : testName(std::move(name)), result(std::move(res)), success(succ) {}
};

/**
 * @brief Result type for test functions: vector of test result entries
 */
using TestResult = std::vector<TestResultEntry>;

/**
 * @brief Runs a test with engine management
 * 
 * This function starts engines based on the provided configurations, calls the test callback
 * with the started engines, and then stops all engines. If the engine vector is empty,
 * no engines are started but the callback is still called with an empty engine list.
 * 
 * @param engineConfigs Vector of engine configurations to start
 * @param testCallback Callback function that receives the engine list and returns test result
 * @return TestResult Vector of key-value pairs with test results
 */
template<typename TestCallback>
TestResult runTest(
    const std::vector<EngineConfig>& engineConfigs,
    TestCallback&& testCallback
);

/**
 * @brief Tests engine start and stop functionality
 * 
 * Starts an engine, measures startup time, memory usage, and shutdown time.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing timing and memory information
 */
TestResult runEngineStartStopTest(const EngineConfig& engineConfig);

/**
 * @brief Tests multiple parallel engine start and stop functionality
 * 
 * Starts multiple engines in parallel, measures startup and shutdown time.
 * 
 * @param engineConfig Configuration for the engines to test
 * @param numEngines Number of engines to start in parallel
 * @return TestResult Vector containing timing information
 */
TestResult runEngineMultipleStartStopTest(const EngineConfig& engineConfig, uint32_t numEngines);

/**
 * @brief Tests if hash table memory shrinks when reducing Hash option
 * 
 * Sets Hash to 512MB, measures memory usage, then sets Hash to 16MB and measures again.
 * Verifies that memory usage decreases by at least 400MB.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing memory usage information and test result
 */
TestResult runHashTableMemoryTest(const EngineConfig& engineConfig);

/**
 * @brief Tests if engine accepts lowercase option names
 * 
 * Sets lowercase 'hash' option to 512, measures memory, then sets uppercase 'Hash' to 512
 * and measures again. Verifies that memory usage is similar (within 1000 bytes).
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing test result
 */
TestResult runLowerCaseOptionTest(const EngineConfig& engineConfig);

/**
 * @brief Tests all engine options with various edge case values
 * 
 * Iterates through all supported engine options and tests them with edge case values
 * based on their type (check, spin, combo, string). Verifies engine doesn't crash
 * when setting each option.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing test results for all options
 */
TestResult runEngineOptionTests(const EngineConfig& engineConfig);

/**
 * @brief Tests if engine reacts correctly to stop command during analysis
 * 
 * Starts analysis on two different positions, waits 1 second, sends stop command,
 * and verifies that bestmove is sent within timeout.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing test result
 */
TestResult runAnalyzeTest(const EngineConfig& engineConfig);

/**
 * @brief Tests if engine handles immediate stop command correctly
 * 
 * Starts analysis, immediately sends stop command, and verifies that bestmove is sent.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing test result
 */
TestResult runImmediateStopTest(const EngineConfig& engineConfig);

/**
 * @brief Tests if engine correctly handles infinite analysis mode
 * 
 * Starts infinite analysis, waits 10 seconds to ensure no premature bestmove,
 * then sends stop and verifies bestmove is sent.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing test result
 */
TestResult runInfiniteAnalyzeTest(const EngineConfig& engineConfig);

/**
 * @brief Tests various go command time limits and constraints
 * 
 * Tests different time control settings including:
 * - Time with increment (no loss on time)
 * - Move time limits
 * - Depth limits
 * - Node limits
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing test results for all limit types
 */
TestResult runGoLimitsTest(const EngineConfig& engineConfig);

/**
 * @brief Tests en passant handling from FEN position
 * 
 * Sets up a position with en passant square from FEN and performs en passant capture.
 * Verifies that engine handles the position correctly.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing test result
 */
TestResult runEpFromFenTest(const EngineConfig& engineConfig);

/**
 * @brief Tests engine playing a complete game against itself
 * 
 * Starts two engine instances and has them play a complete game.
 * Monitors for illegal moves, crashes, and protocol violations.
 * 
 * @param engineConfig Configuration for the engines to test
 * @return TestResult Vector containing game result and any errors
 */
TestResult runComputeGameTest(const EngineConfig& engineConfig, bool logMoves = true);

/**
 * @brief Tests UCI pondering functionality
 * 
 * Tests ponder hit and ponder miss scenarios:
 * - Engine pondering on predicted move
 * - Engine receiving ponderhit command
 * - Engine receiving stop command during ponder (ponder miss)
 * - Response times and behavior validation
 * 
 * @param engineConfig Configuration for the engine to test
 * @param logMoves Whether to log moves during the game
 * @return TestResult Vector containing test results for UCI ponder scenarios
 */
TestResult runUciPonderTest(const EngineConfig& engineConfig);

/**
 * @brief Tests pondering during a complete game
 * 
 * Starts two engine instances with pondering enabled and has them
 * play a complete game. Monitors pondering behavior throughout the game.
 * 
 * @param engineConfig Configuration for the engines to test
 * @param logMoves Whether to log moves during the game
 * @return TestResult Vector containing game result and pondering behavior
 */
TestResult runPonderGameTest(const EngineConfig& engineConfig, bool logMoves = true);

/**
 * @brief Tests EPD (Extended Position Description) test suite
 * 
 * Tests if the engine finds correct moves for a list of positions.
 * Uses EpdTestManager to evaluate engine performance on standardized
 * tactical and strategic positions.
 * 
 * @param engineConfig Configuration for the engine to test
 * @return TestResult Vector containing results for each EPD position
 */
TestResult runEpdTest(const EngineConfig& engineConfig);

/**
 * @brief Tests playing multiple games in parallel
 * 
 * Runs multiple games with the engine playing against itself using
 * TestTournament. Tests concurrent game handling and validates that
 * the engine can handle multiple simultaneous games correctly.
 * 
 * @param engineConfig Configuration for the engine to test
 * @param numGames Number of games to play (default: determined by test)
 * @return TestResult Vector containing tournament results
 */
TestResult runMultipleGamesTest(const EngineConfig& engineConfig, uint32_t numGames = 10, uint32_t concurrency = 4);

} // namespace QaplaTester
