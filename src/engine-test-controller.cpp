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
#include <sstream>
#include <string>
#include <thread>

#include "engine-test-controller.h"
#include "engine-worker-factory.h"
#include "engine-report.h"
#include "cli-settings-manager.h"
#include "epd-test-manager.h"
#include "compute-task.h"
#include "game-manager.h"
#include "game-manager-pool.h"
#include "event-sink-recorder.h"
#include "engine-test-functions.h"
#include "test-tournament.h"

namespace QaplaTester {

void EngineTestController::createGameManager() {
    computeTask_ = std::make_unique<ComputeTask>();
    startEngine();
}

void EngineTestController::startEngine() {
    bool success = false;
    try {
        auto ctList = EngineWorkerFactory::createEngines(engineConfig_, 1);
		computeTask_->initEngines(std::move(ctList));
		success = computeTask_->getEngine()->requestReady();
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Configuration error during engine test for " + 
            engineConfig_.getName() + ": " + std::string(e.what()), 
            TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during engine test for " + engineConfig_.getName(), 
        TraceLevel::error);
    }
    checklist_->logReport("starts-and-stops-cleanly", success, "  engine did not respond to isReady after startup in time");
    if (!success) {
		Logger::testLogger().log("Engine did not start successfully", TraceLevel::error);
		throw(std::runtime_error("Engine did not start successfully"));
    }
}

EngineList EngineTestController::startEngines(uint32_t count) {
    EngineList list = EngineWorkerFactory::createEngines(engineConfig_, count);

    std::vector<std::future<bool>> results;
    for (auto& engine : list) {
        results.push_back(std::async(std::launch::async, [&engine]() {
            return engine->requestReady();
            }));
    }

    bool allReady = std::ranges::all_of(results, [](auto& f) {
        return f.get();
    });

    checklist_->logReport("starts-and-stops-cleanly", allReady, "  one or more engines did not respond to isReady in time");
    if (!allReady) {
        Logger::testLogger().log("Engines did not start successfully", TraceLevel::error);
    }

    return list;
}

void EngineTestController::runAllTests(const EngineConfig& engine, int numGames) {
    engineConfig_ = engine;
	checklist_ = EngineReport::getChecklist(engineConfig_.getName());
    try {
        const CliSettings::GroupInstance testSettings = *CliSettings::Manager::getGroupInstance("test");
        numGames_ = numGames;
        createGameManager();
        runStartStopTest();
        runMultipleStartStopTest(20);
        if (!testSettings.get<bool>("nomemory")) {
            runHashTableMemoryTest();
        }
        if (!testSettings.get<bool>("nooption")) {
            runLowerCaseOptionTest();
            runEngineOptionTests();
        }
        runAnalyzeTest();
		if (!testSettings.get<bool>("nostop")) {
            runImmediateStopTest();
        }
		if (!testSettings.get<bool>("nowait")) {
            runInfiniteAnalyzeTest();
        }
        runGoLimitsTests();
        runEpFromFenTest();
        if (!testSettings.get<bool>("noepd")) {
			runEpdTests();
		}
        runComputeGameTest();
        if (!testSettings.get<bool>("noponder")) {
            runUciPonderTest();
            runPonderGameTest();
        }
        runMultipleGamesTest();
    }
	catch (const std::exception& e) {
		Logger::testLogger().log("Exception during engine tests, all remaining tests cancelled: " + std::string(e.what()), TraceLevel::error);
	}
	catch (...) {
		Logger::testLogger().log("Unknown exception during engine tests, all remaining tests cancelled.", TraceLevel::error);
	}

}

/**
 * Runs a standardized test sequence including pre-checks, initialization, and error handling.
 * @param testName A descriptive name of the test.
 * @param testCallback A callable returning std::pair<bool, std::string> indicating success and optional error message.
 */
void EngineTestController::runTest(
    const std::string& testName,
    const std::function<std::pair<bool, std::string>()>& testCallback)
{
    constexpr std::chrono::seconds timeout{ 2 };
    try {
        if (!computeTask_) {
			Logger::testLogger().log("ComputeTask not initialized", TraceLevel::error);
            return;
        }
        if (computeTask_->getEngine() == nullptr) {
            startEngine();
        }
		bool isComputeReady = computeTask_->getEngine()->requestReady(timeout);
        if (!isComputeReady) {
            startEngine();
		}

        const auto [success, errorMessage] = testCallback();
        if (!testName.empty()) {
            checklist_->logReport(testName, success, errorMessage);
        }
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during test '" + testName + "': " + e.what(), TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during test '" + testName + "'", TraceLevel::error);
    }
}

void EngineTestController::runStartStopTest() {
    // Use QaplaTester function
    TestResult result = QaplaTester::runEngineStartStopTest(engineConfig_);
    
    // Check if the test succeeded using success flag
    startStopSucceeded = true;
    for (const auto& entry : result) {
        if (!entry.success) {
            startStopSucceeded = false;
            Logger::testLogger().log("Engine could not be started or stopped: " + entry.result, 
                TraceLevel::error);
            break;
        }
    }
    
    if (!startStopSucceeded) {
        Logger::testLogger().log("Engine could not be started or stopped. Skipping remaining tests.", 
            TraceLevel::error);
        return;
    }
}

void EngineTestController::runMultipleStartStopTest(uint32_t numEngines) {
    // Use QaplaTester function
    TestResult result = QaplaTester::runEngineMultipleStartStopTest(engineConfig_, numEngines);
    
    // Check result using success flag
    for (const auto& entry : result) {
        if (!entry.success) {
            checklist_->logReport("starts-and-stops-cleanly", false,
                "  Multiple start/stop test failed: " + entry.result);
            return;
        }
    }
}


void EngineTestController::runGoLimitsTests() {
    // Use QaplaTester function
    auto results = QaplaTester::runGoLimitsTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Go limits test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runHashTableMemoryTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runHashTableMemoryTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Hash table memory test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runLowerCaseOptionTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runLowerCaseOptionTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Lowercase option test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runEngineOptionTests() {
    // Use QaplaTester function
    auto results = QaplaTester::runEngineOptionTests(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Engine option test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runAnalyzeTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runAnalyzeTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Analyze test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runImmediateStopTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runImmediateStopTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Immediate stop test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runInfiniteAnalyzeTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runInfiniteAnalyzeTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Infinite analyze test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runUciPonderTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runUciPonderTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("UCI ponder test failed: " + entry.result, TraceLevel::error);
        }
    }
}


void EngineTestController::runEpdTests() {
    // Use QaplaTester function
    auto results = QaplaTester::runEpdTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("EPD test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runEpFromFenTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runEpFromFenTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("EP from FEN test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runComputeGameTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runComputeGameTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Compute game test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runPonderGameTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runPonderGameTest(engineConfig_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Ponder game test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runMultipleGamesTest() {
    // Use QaplaTester function
    auto results = QaplaTester::runMultipleGamesTest(engineConfig_, numGames_);
    for (const auto& entry : results) {
        if (!entry.success) {
            Logger::testLogger().log("Multiple games test failed: " + entry.result, TraceLevel::error);
        }
    }
}

void EngineTestController::runPlaceholderTest() {
    try {
        // No-op test for demonstration
    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during placeholder test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during placeholder test.", TraceLevel::error);
    }
}

} // namespace QaplaTester
