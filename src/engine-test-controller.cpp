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

#include <memory>
#include "engine-test-controller.h"
#include "engine-worker-factory.h"
#include "engine-checklist.h"

void EngineTestController::createGameManager(std::filesystem::path enginePath, bool singleEngine) {
    EngineWorkerFactory factory;
    auto list = factory.createUci(enginePath, std::nullopt, 1);
	gameManager_ = std::make_unique<GameManager>(std::move(list[0]));
}

void EngineTestController::runAllTests(std::filesystem::path enginePath) {
    runStartStopTest(enginePath);
	createGameManager(enginePath, true);
    runHashTableMemoryTest();
    runGoLimitsTests();
    runPlaceholderTest();
	gameManager_->stop();
}

void EngineTestController::runStartStopTest(std::filesystem::path enginePath) {
    std::cout << "Running test: Starting and stopping the engine...\n";
    try {
        EngineWorkerFactory factory;
        auto list = factory.createUci(enginePath, std::nullopt, 1);
        list[0]->stop();
        startStopSucceeded = true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during start/stop test: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Unknown exception during start/stop test.\n";
    }
    if (!startStopSucceeded) {
        std::cout << "Engine could not be started or stopped. Skipping remaining tests.\n";
        return;
    }
}

void EngineTestController::runGoLimitsTests() {
    std::cout << "Running GoLimits-based move calculation tests...\n";

    struct TestCase {
        std::string name;
        TimeControl timeControl;
    };

    std::vector<std::pair<std::string, TimeControl>> testCases = {
        { "normal time with increment", [] {
            TimeControl t; t.addTimeSegment({0, 30000, 500}); return t;
        }() },
        { "movetime support", [] { TimeControl t; t.setMoveTime(1000); return t; }() },
        { "depth-limited support", [] { TimeControl t; t.setDepth(4); return t; }() },
        { "node-limited suport", [] { TimeControl t; t.setNodes(10000); return t; }() },
        { "low time with high inc", [] {
            TimeControl t; t.addTimeSegment({0, 100, 2000}); return t;
        }() },
        { "movestogo limits total time", [] {
            TimeControl t; t.addTimeSegment({5, 30000, 0}); return t;
        }() },
        { "depth should override time", [] {
            TimeControl t; t.setDepth(10); t.addTimeSegment({0, 10000, 0}); return t;
        }() },
        { "nodes should override time", [] {
            TimeControl t; t.setNodes(100000); t.addTimeSegment({0, 10000, 0}); return t;
        }() },
        { "few moves left, no increment", [] {
            TimeControl t; t.addTimeSegment({3, 300, 0}); return t;
        }() }
    };


    try {
        if (!gameManager_) {
            throw std::runtime_error("GameManager not initialized");
        }

        for (const auto& test : testCases) {
            std::cout << "  ..." << test.first << std::endl;

            gameManager_->newGame();
            gameManager_->setTime(test.second);
            gameManager_->computeMove(true);
            gameManager_->getFinishedFuture().wait();

        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during GoLimits test: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Unknown exception during GoLimits test.\n";
    }
    std::cout << "  ...done" << std::endl;
}

void EngineTestController::runHashTableMemoryTest() {
    std::cout << "Running test: Hash table memory release on shrink\n";
    try {
        if (!gameManager_ || !gameManager_->getEngine()) {
            std::cerr << "EngineWorker not initialized.\n";
            return;
        }

        // Set high hash size
        gameManager_->getEngine()->setOption("Hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Allow allocation
        std::size_t memHigh = gameManager_->getEngine()->getEngineMemoryUsage();

        // Set low hash size
        gameManager_->getEngine()->setOption("Hash", "1");
        std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Allow deallocation
        std::size_t memLow = gameManager_->getEngine()->getEngineMemoryUsage();

        handleCheck("Engine memory increases / shrinks with hash size as expected", 
            (memLow + 400000 > memHigh), 
            "Memory did not shrink as expected when changing Hash from 512 to 1; usage was "
            + std::to_string(memHigh) + " bytes before, now " + std::to_string(memLow) + " bytes.");

        // Back to normal
        gameManager_->getEngine()->setOption("Hash", "32");
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during hash table memory test: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Unknown exception during hash table memory test.\n";
    }
}


void EngineTestController::runPlaceholderTest() {
    std::cout << "Running test: Placeholder for additional tests...\n";
    try {
        // No-op test for demonstration
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during placeholder test: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Unknown exception during placeholder test.\n";
    }
}
