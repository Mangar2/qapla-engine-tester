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
#include <chrono>
#include "engine-test-controller.h"
#include "engine-worker-factory.h"
#include "engine-checklist.h"

void EngineTestController::createGameManager(std::filesystem::path enginePath, bool singleEngine) {
	enginePath_ = enginePath;
    gameManager_ = std::make_unique<GameManager>();
    startEngine();
}

void EngineTestController::startEngine() {
	std::cout << "Starting engine..." << std::endl;
    EngineWorkerFactory factory;
    auto list = factory.createUci(enginePath_, std::nullopt, 1);
	gameManager_->setEngine(std::move(list[0])); 
    bool success = gameManager_->getEngine()->requestReady();
    handleCheck("Engine Started successfully", success, "  engine did not respond to isReady after startup in time");
    if (!success) {
        throw std::runtime_error("Engine startup did not succeed");
    }
}

void EngineTestController::runAllTests(std::filesystem::path enginePath) {
    runStartStopTest(enginePath);
    createGameManager(enginePath, true);
    runEngineOptionTests();
    runHashTableMemoryTest();
    runGoLimitsTests();
    gameManager_->newGame();
    TimeControl t; t.addTimeSegment({ 0, 30000, 500 });
    gameManager_->setTime(t);
    gameManager_->computeGame(true);
    gameManager_->getFinishedFuture().wait();
    
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
        setOption("Hash", "512");
        std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Allow allocation
        std::size_t memHigh = gameManager_->getEngine()->getEngineMemoryUsage();

        // Set low hash size
        setOption("Hash", "1");
        std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Allow deallocation
        std::size_t memLow = gameManager_->getEngine()->getEngineMemoryUsage();

        handleCheck("Engine memory increases / shrinks with hash size as expected", 
            (memLow + 400000 < memHigh),
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

std::vector<std::string> generateCheckValues() {
    return { "true", "false", "yes", "no", "1", "0", "invalid" };
}

std::vector<std::string> generateSpinValues(const EngineOption& opt) {
    std::vector<std::string> values;
    if (opt.min && opt.max) {
        int min = *opt.min;
        int max = *opt.max;
        int mid = min + (max - min) / 2;
        values = {
            std::to_string(min),
            std::to_string(max),
            std::to_string(mid),
            std::to_string(min - 1),
            std::to_string(max + 1),
            "not_a_number"
        };
    }
    else {
        values = { "0", "100", "-1", "NaN" };
    }
    return values;
}

std::vector<std::string> generateComboValues(const EngineOption& opt) {
    std::vector<std::string> values = opt.vars;
    values.push_back("invalid_option");
    return values;
}

std::vector<std::string> generateStringValues() {
    return {
        "simple", "",
        "with space",
        "öäüß",               
        "C:\\invalid\\path",
        std::string(1024, 'A'), 
        "\x01\x02\x03\xFF"      
    };
}

void EngineTestController::setOption(std::string name, std::string value) {
    auto engine = gameManager_->getEngine();
    bool success = engine->setOption(name, value);
    if (handleCheck("Engine Options works safely", success, "  option '" + name + "' with value '" + value + " runs into timeout ")) {
        return;
    }
    bool alive = engine->isRunning();
    if (alive  && !engine->requestReady(std::chrono::seconds{ 10 })) {
		// still not responding, we assume it crashed
        alive = false;
    }
    if (!handleCheck("Engine Options works safely", alive, "  engine crashed after setting option '" + name + "'")) {
        std::cerr << "    Restarting engine due to crash\n";
        engine->stop();
        startEngine();
    }
}

void EngineTestController::runEngineOptionTests() {
    std::cout << "Running engine option tests...\n";

    try {
        if (!gameManager_) {
            throw std::runtime_error("GameManager not initialized");
        }

        auto engine = gameManager_->getEngine();
        const EngineOptions& options = engine->getSupportedOptions();

        for (const auto& [name, opt] : options) {
            if (name == "Hash" || opt.type == EngineOption::Type::Button) {
                continue;
            }

            std::vector<std::string> testValues;

            switch (opt.type) {
            case EngineOption::Type::Check:
                testValues = generateCheckValues();
                break;
            case EngineOption::Type::Spin:
                testValues = generateSpinValues(opt);
                break;
            case EngineOption::Type::Combo:
                testValues = generateComboValues(opt);
                break;
            case EngineOption::Type::String:
                testValues = generateStringValues();
                break;
            default:
                continue;
            }

            for (const auto& value : testValues) {
                try {
					setOption(name, value);
                }
                catch (const std::exception& e) {
                    std::cerr << "    Exception: " << e.what() << "\n";
                }
                catch (...) {
                    std::cerr << "    Unknown error\n";
                }
            }

            if (!opt.defaultValue.empty()) {
                try {
					setOption(name, opt.defaultValue);
                }
                catch (const std::exception& e) {
                    std::cerr << "    Exception while resetting: " << e.what() << "\n";
                }
                catch (...) {
                    std::cerr << "    Unknown error while resetting\n";
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during engine option test: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Unknown exception during engine option test.\n";
    }

    std::cout << "  ...done" << std::endl;
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
