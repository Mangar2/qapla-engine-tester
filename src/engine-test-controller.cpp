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
#include "engine-test-controller.h"
#include "engine-worker-factory.h"
#include "engine-checklist.h"

void EngineTestController::createGameManager(std::filesystem::path enginePath, bool singleEngine) {
	enginePath_ = enginePath;
    gameManager_ = std::make_unique<GameManager>();
    startEngine();
}

void EngineTestController::startEngine() {
    EngineWorkerFactory factory;
    auto list = factory.createUci(enginePath_, std::nullopt, 1);
	gameManager_->setUniqueEngine(std::move(list[0])); 
    bool success = gameManager_->getEngine()->requestReady();
    handleCheck("Engine Started successfully", success, "  engine did not respond to isReady after startup in time");
    if (!success) {
		Logger::testLogger().log("Engine did not start successfully", TraceLevel::error);
    }
}

EngineList EngineTestController::startEngines(uint32_t count) {
    EngineWorkerFactory factory;
    EngineList list = factory.createUci(enginePath_, std::nullopt, count);

    std::vector<std::future<bool>> results;
    for (auto& engine : list) {
        results.push_back(std::async(std::launch::async, [&engine]() {
            return engine->requestReady();
            }));
    }

    bool allReady = std::all_of(results.begin(), results.end(), [](auto& f) {
        return f.get();
        });

    handleCheck("Engines Started successfully", allReady, "  one or more engines did not respond to isReady in time");
    if (!allReady) {
        Logger::testLogger().log("Engines did not start successfully", TraceLevel::error);
    }

    return list;
}

std::string bytesToMB(int64_t bytes) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0));
	return oss.str();
}

void EngineTestController::runAllTests(std::filesystem::path enginePath) {
    runStartStopTest(enginePath);
	runMultipleStartStopTest(enginePath, 20);
    createGameManager(enginePath, true);
    runHashTableMemoryTest();
    runEngineOptionTests();
    runGoLimitsTests();
    runMultipleGamesTest();
    //runComputeGameTest();
    gameManager_->stop();

}

void EngineTestController::runStartStopTest(std::filesystem::path enginePath) {
    try {
        EngineWorkerFactory factory;
        Timer timer;
        timer.start();
        auto list = factory.createUci(enginePath, std::nullopt, 1);
        auto startTime = timer.elapsedMs();
		auto engine = list[0].get();
        int64_t memoryInBytes = engine->getEngineMemoryUsage();
        engine->stop();
		auto stopTime = timer.elapsedMs();

        startStopSucceeded = true;
		Logger::testLogger().log("Testing the engine, welcome message, name, author: ");
		if (!engine->getWelcomeMessage().empty()) {
			Logger::testLogger().log(engine->getWelcomeMessage());
		}
        if (!engine->getEngineName().empty()) {
            Logger::testLogger().log(engine->getEngineName() + " from " + engine->getEngineAuthor());
        }
		Logger::testLogger().log("Engine started in " + std::to_string(startTime) + " ms, stopped in " +
			std::to_string(stopTime) + " ms, memory usage: " + bytesToMB(memoryInBytes) + " MB");

    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during start/stop test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during start/stop test.", TraceLevel::error);
    }
    if (!startStopSucceeded) {
		Logger::testLogger().log("Engine could not be started or stopped. Skipping remaining tests.", TraceLevel::error);
        return;
    }
}

void EngineTestController::runMultipleStartStopTest(std::filesystem::path enginePath, int numEngines) {
    try {
        EngineWorkerFactory factory;
        Timer timer;
        timer.start();
        EngineList engines = factory.createUci(enginePath, std::nullopt, numEngines);
        auto startTime = timer.elapsedMs();

        // Engines parallel stoppen
        std::vector<std::future<void>> stopFutures;
        stopFutures.reserve(numEngines);

        for (auto& engine : engines) {
            stopFutures.push_back(std::async(std::launch::async, [e = engine.get()] {
                e->stop();
                }));
        }
        for (auto& future : stopFutures) {
            future.get();
        }

        auto stopTime = timer.elapsedMs();

        Logger::testLogger().log("\nTesting start and stop of " + std::to_string(numEngines) + " Engines in parallel");
        Logger::testLogger().log("Engines started in " + std::to_string(startTime) + " ms and stopped in " +
            std::to_string(stopTime) + " ms. Below one second each is a good result.");

    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during mass start/stop test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during mass start/stop test.", TraceLevel::error);
    }
}

void EngineTestController::runGoLimitsTests() {

    struct TestCase {
        std::string name;
        TimeControl timeControl;
    };

    Logger::testLogger().log("\nTesting compute moves with different time limits, node limits and/or depth limits.");

    std::vector<std::pair<std::string, TimeControl>> testCases = {
        { "normal time with increment", [] {
            TimeControl t; t.addTimeSegment({0, 30000, 500}); return t;
        }() },
        { "movetime", [] { TimeControl t; t.setMoveTime(1000); return t; }() },
        { "depth-limited", [] { TimeControl t; t.setDepth(4); return t; }() },
        { "node-limited", [] { TimeControl t; t.setNodes(10000); return t; }() },
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
			Logger::testLogger().log("GameManager not initialized", TraceLevel::error);
        }

        for (const auto& test : testCases) {
            gameManager_->newGame();
            gameManager_->setUniqueTimeControl(test.second);
            gameManager_->computeMove(true);
            gameManager_->getFinishedFuture().wait();
        }
    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during GoLimits test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during GoLimits test.", TraceLevel::error);
    }
}

void EngineTestController::runHashTableMemoryTest() {
    try {
        if (!gameManager_ || !gameManager_->getEngine()) {
			Logger::testLogger().log("GameManager or Engine not initialized", TraceLevel::error);
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
		bool success = memLow + 400000000 < memHigh;

        handleCheck("Engine memory increases / shrinks with hash size as expected", 
            success,
            "Memory did not shrink as expected when changing Hash from 512 to 1; usage was "
            + std::to_string(memHigh) + " bytes before, now " + std::to_string(memLow) + " bytes.");

        // Back to normal
        gameManager_->getEngine()->setOption("Hash", "32");
        Logger::testLogger().log("\nTesting memory shrink. Memory consumption with 512MB Hash is " + bytesToMB(memHigh) + " MB shrinked with 1 MB Hash to " + bytesToMB(memLow) + " MB"
			+ (success ? " (as expected)" : " (not as expected)"));

    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during hash table memory test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during hash table memory test.", TraceLevel::error);
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

bool EngineTestController::setOption(std::string name, std::string value) {
    auto engine = gameManager_->getEngine();
    bool success = engine->setOption(name, value);
    if (handleCheck("Engine Options works safely", success, "  option '" + name + "' with value '" + value + "'\nruns into timeout ")) {
        return true;
    }
    bool alive = engine->isRunning();
    if (alive  && !engine->requestReady(std::chrono::seconds{ 10 })) {
		// still not responding, we assume it crashed
        alive = false;
    }
    if (!handleCheck("Engine Options works safely", alive, "  engine crashed after setting option '" + name + "'")) {
        engine->stop();
        startEngine();
    }
    return false;
}

void EngineTestController::runEngineOptionTests() {
    int errors = 0;
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
                    if (!setOption(name, value)) errors++;
                }
                catch (const std::exception& e) {
                    errors++;
					Logger::testLogger().log("Exception while setting option '" + name + "' to '" + value + "': " + e.what(), TraceLevel::error);
                }
                catch (...) {
                    errors++;
					Logger::testLogger().log("Unknown error while setting option '" + name + "' to '" + value + "'", TraceLevel::error);
                }
            }

            if (!opt.defaultValue.empty()) {
                try {
					if (!setOption(name, opt.defaultValue)) errors++;
                }
                catch (const std::exception& e) {
                    errors++;
					Logger::testLogger().log("Exception while resetting option '" + name + "' to default value: " + e.what(), TraceLevel::error);
                }
                catch (...) {
                    errors++;
					Logger::testLogger().log("Unknown error while resetting option '" + name + "' to default value", TraceLevel::error);
                }
            }
        }
        Logger::testLogger().log("\nTried to harm the engine by setting valid and invalid options. " +
            (errors == 0 ? "The engine is very stable, no errors produced." : std::to_string(errors) + " hangs or crashes occurred. See the log for details."));
    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during engine option test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during engine option test.", TraceLevel::error);
    }
}

void EngineTestController::runComputeGameTest() {
	Logger::testLogger().log("\nThe engine now plays against itself. I control all engine output, and check its validity while playing.");
    EngineList engines = startEngines(2);
    gameManager_->setEngines(std::move(engines[0]), std::move(engines[1]));
    try {
        gameManager_->newGame();
        TimeControl t1; t1.addTimeSegment({ 0, 30000, 500 });
        TimeControl t2; t2.addTimeSegment({ 0, 10000, 100 });
        gameManager_->setTimeControls(t1, t2);
        gameManager_->computeGame(true, "", true);
        gameManager_->getFinishedFuture().wait();
    }
    catch (const std::exception& e) {
		Logger::testLogger().log("Exception during compute games test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
		Logger::testLogger().log("Unknown exception during compute games test.", TraceLevel::error);
    }
}

std::pair<TimeControl, TimeControl> createTestTimeControls(int index) {
    TimeControl white;
    white.addTimeSegment({ 0, 30000, 500 });

    TimeControl black;
    black.addTimeSegment({ 0, 10000, 100 });

    return { white, black };
}

struct TaskSource {
    int current = 0;
    const int max;

    explicit TaskSource(int total) : max(total) {}

    std::optional<int> nextIndex() {
        if (current < max) {
            return current++;
        }
        return std::nullopt;
    }
};

void EngineTestController::runMultipleGamesTest() {
    constexpr int totalGames = 100;
    constexpr int parallelGames = 20;

    Logger::testLogger().log("\nTesting different time controls by playing multiple games in parallel.");

    EngineWorkerFactory factory;
    EngineList engines = factory.createUci(enginePath_, std::nullopt, parallelGames * 2);

    TournamentManager tournament(totalGames);

    std::vector<std::unique_ptr<GameManager>> managers;
    try {
        for (int i = 0; i < parallelGames; ++i) {
            auto manager = std::make_unique<GameManager>();
            manager->setEngines(std::move(engines[i * 2]), std::move(engines[i * 2 + 1]));

            manager->computeGames([&tournament]() {
                return tournament.nextTask();
                });

            managers.push_back(std::move(manager));
        }

        for (auto& m : managers) {
            m->getFinishedFuture().wait();
        }

        Logger::testLogger().log("All games completed.");
    }
    catch (const std::exception& e) {
        Logger::testLogger().log("Exception during compute games test: " + std::string(e.what()), TraceLevel::error);
    }
    catch (...) {
        Logger::testLogger().log("Unknown exception during compute games test.", TraceLevel::error);
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
