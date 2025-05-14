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
#include "game-manager.h"
#include "engine-worker-factory.h"

/**
 * @brief Controls the execution flow of chess engine tests.
 */
class EngineTestController {
public:
    /**
     * @brief Runs the full suite of tests in a controlled sequence.
     */
    void runAllTests(std::filesystem::path enginePath);

private:
    bool startStopSucceeded = false;

    void runStartStopTest(std::filesystem::path enginePath);

	void runMultipleStartStopTest(std::filesystem::path enginePath, int numEngines);

    void runPlaceholderTest();

    /**
      * @brief Creates and initializes a GameManager instance.
      *
      * Initializes a GameManager capable of controlling the engine via UCI protocol.
      * Must be called before executing any test that requires engine interaction.
      *
      * @param enginePath Path to the engine binary.
      */
    void createGameManager(std::filesystem::path enginePath, bool singleEngine = true);

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
     * Runs a set of generic tests to verify the engine's correct handling of option changes.
     * This test assumes no knowledge of specific options beyond basic types and behaviors.
     */
    void runEngineOptionTests();

    void runComputeGameTest();

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
    bool setOption(std::string name, std::string value);

    /**
     * @brief General check handling method.
     * @param name Checklist-Name of the topic.
	 * @param success Result of the check
     * @param detail Detailed error message to be logged
     */
    bool handleCheck(std::string name, bool success, std::string detail = "") {
        EngineChecklist::report(name, success);
        if (!success) {
            Logger::testLogger().log("Error: " + name + ": " + detail, TraceLevel::error);
        }
        return success;
    }

    std::unique_ptr<GameManager> gameManager_;
    std::filesystem::path enginePath_;

};
