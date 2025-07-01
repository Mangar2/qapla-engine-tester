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

#include <iostream>
#include <string>
#include <filesystem>
#include "engine-config.h"
#include "game-manager.h"
#include "engine-worker-factory.h"

/**
 * @brief Controls the execution flow of chess engine tests.
 */
class EngineTestController {
public:
    /**
     * @brief Runs the full suite of tests in a controlled sequence.
	 * @param engineName The name of the engine to test.
	 * @param numGames The number of games to run in the compute game test.
     */
    void runAllTests(const EngineConfig& engine, int numGames);

private:
    bool startStopSucceeded = false;

    /**
     * Runs a standardized test sequence including pre-checks, initialization, and error handling.
     * @param testName A descriptive name of the test.
     * @param testCallback A callable returning std::pair<bool, std::string> indicating success and optional error message.
     */
    void runTest(const std::string& testName, const std::function<std::pair<bool, std::string>()>& testCallback);

    void runStartStopTest();

	void runMultipleStartStopTest(int numEngines);

    void runPlaceholderTest();

    /**
      * @brief Creates and initializes a GameManager instance.
      *
      * Initializes a GameManager capable of controlling the engine via UCI protocol.
      * Must be called before executing any test that requires engine interaction.
      *
	  * @param bool singleEngine if true, only one engine instance is created, otherwise two instances are created
      */
    void createGameManager(bool singleEngine = true);

	/**
	 * @brief Starts an engine instance and sets it as unique engine to the GameManager
	 *
	 */
    void startEngine();

	/**
	 * @brief Starts several engines 
	 *
	 */
    EngineList startEngines(uint32_t count);

    /**
     * @brief Runs all tests involving GoLimits-based move calculations.
     *
     * Initially includes a single test with movetime = 1000ms.
     * Future GoLimits variants can be added here.
     */
    void runGoLimitsTests();

    /**
     * @brief Tests whether changing the UCI hash table size affects memory usage as expected.
     *
     * Sends different values for the "Hash" option to the engine and checks if memory
     * usage increases or decreases accordingly.
     */
    void runHashTableMemoryTest();

    /**
     * @brief Tests wether the UCI engine recognizes a lower case option "hash"
     */
    void runLowerCaseOptionTest();

    /**
     * Runs a set of generic tests to verify the engine's correct handling of option changes.
     * This test assumes no knowledge of specific options beyond basic types and behaviors.
     */
    void runEngineOptionTests();

    /**
	 * @brief Tests the engine's ability to analyze a position.
     */
    void runAnalyzeTest();
    void runImmediateStopTest();
	void runInfiniteAnalyzeTest();

    void testPonderHit(const GameRecord& gameRecord, EngineWorker* engine,
        const std::string ponderMove, const std::string testname,
        std::chrono::milliseconds sleep = std::chrono::seconds{ 1 });
    void testPonderMiss(const GameRecord& gameRecord, EngineWorker* engine,
        const std::string ponderMove, const std::string testname,
        std::chrono::milliseconds sleep = std::chrono::seconds{ 1 });
    void runUciPonderTest();

    /**
     * @brief computes a list of moves for epd test positions
     */
	void runEpdTests();

	/**
	 * @brief Tests the engine's ability to compute moves in a game.
	 */
    void runComputeGameTest();

    /**
     * @brief Tests the engine`s ability to ponder
     */
    void runPonderGameTest();

	/**
	 * @brief Tests the engine's ability to compute moves in a game.
	 *
	 * This test runs multipe games with different time controls in parallel.
	 */
    void runMultipleGamesTest();
    
    /**
	 * @brief Sets a specific option for the engine and checks if it runs without crashing.
	 * @param name Name of the option to set.
	 * @param value Value to set for the option.
	 * @return True if the option was set successfully, false otherwise.
     */
    std::pair<bool, std::string> setOption(const std::string& name, const std::string& value);

    EngineReport* checklist_;
    std::unique_ptr<GameManager> gameManager_;
    EngineConfig engineConfig_;
    int numGames_ = 20;
};
