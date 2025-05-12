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
     * @brief Runs all tests involving GoLimits-based move calculations.
     *
     * Initially includes a single test with movetime = 1000ms.
     * Future GoLimits variants can be added here.
     */
    void runGoLimitsTests();

    std::unique_ptr<GameManager> gameManager_;
    std::unique_ptr<EngineWorker> engineWorker_;

};
